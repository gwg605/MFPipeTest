#include "TransportUDP.h"
#include <thread>
#include <chrono>
#include <cassert>

namespace comm {
namespace transports {
	/**
	*	Composing message for UDP transport
	*/
	class MsgComposeUDP : public comm::IMsgCompose, public std::enable_shared_from_this<MsgComposeUDP> {
	protected:
		/// message id
		MessageID m_MessageID;
		/// reference to packets store
		NetBuffersStore::Ptr m_BuffersStoreRef;
		/// reference to sending queue (for output data)
		SendingQueue::Ptr m_SendingQueue;
		/// message data
		std::list<NetBuffer> m_Data;
		/// next packet number
		uint32_t m_Packet;
		/// onsent notification handler
		FnOnSent m_OnSent;

	public:
		MsgComposeUDP( MessageID msg_id, const NetBuffersStore::Ptr& store, const SendingQueue::Ptr& queue )
			: m_MessageID( msg_id )
			, m_BuffersStoreRef( store )
			, m_SendingQueue( queue )
			, m_Packet( 0 )
		{
			assert( m_BuffersStoreRef != nullptr );
			assert( m_SendingQueue != nullptr );
		}

		NetBufferRef* AllocBuffer() override {
			assert( m_BuffersStoreRef != nullptr );

			if( !m_BuffersStoreRef->Alloc( m_Data, 1500 ) ) {
				return nullptr;
			}

			assert( !m_Data.empty() );
			auto& net_buffer = m_Data.back();

			// fill header
			UDPPacketHeader* ph = reinterpret_cast<UDPPacketHeader*>( net_buffer.buffer.data() );
			if( m_Packet == 0 ) {
				ph->flags = static_cast<byte>( UDPPacketFlag::First );
			}
			ph->msg_id = m_MessageID;
			ph->packet = m_Packet++;

			// move payload pointer behind the packet header
			net_buffer.ref.data = net_buffer.buffer.data() + sizeof( UDPPacketHeader );
			net_buffer.ref.size = net_buffer.buffer.size() - sizeof( UDPPacketHeader );

			return &net_buffer.ref;
		}

		Error Write( NetBufferRef* buf, size_t len ) override {
			assert( buf != nullptr );
			buf->size = len;
			return Error::Ok;
		}

		Error Send( bool failed, const FnOnSent& onsent ) override {
			if( failed ) {
				// do nothing, forgot about this message
				return Error::Ok;
			}

			if( m_Data.empty() ) {
				return Error::Fatal;
			}

			m_OnSent = onsent;

			auto& net_buffer = m_Data.back();

			UDPPacketHeader* ph = reinterpret_cast<UDPPacketHeader*>( net_buffer.buffer.data() );
			ph->flags |= static_cast<byte>( UDPPacketFlag::Last );

			auto sthis = shared_from_this();
			m_SendingQueue->Send( m_MessageID, m_Data, [sthis]( size_t sent_size, const Error& status ) {
				sthis->OnSentReport( sent_size, status );
			} );

			return Error::Ok;
		}

		void Close() override {
			m_OnSent = nullptr;
		}

		void OnSentReport( size_t sent_size, const Error& status ) {
			//printf( "OnReport( %i, %i )\n", (int)sent_size, (int)status );
			if( m_OnSent ) {
				m_OnSent( status );
			}
		}

	};


	/**
	*	Receivied message for UDP transport
	*/
	class MsgReceivedUDP : public IMsgReceived {
	public:
		using Ptr = std::shared_ptr<MsgReceivedUDP>;

	protected:
		/// reference to packets store
		NetBuffersStore::Ptr m_BuffersStoreRef;
		/// message id
		MessageID m_MessageID;
		/// message data
		std::list<NetBuffer> m_Data;

	public:
		MsgReceivedUDP( const NetBuffersStore::Ptr& store, MessageID msg_id, std::list<NetBuffer>& buffers )
			: m_BuffersStoreRef( store )
			, m_MessageID( msg_id ) {
			m_Data.splice( m_Data.end(), buffers );
		}

		~MsgReceivedUDP() override {
			m_BuffersStoreRef->Release( m_Data );
		}

		MessageID GetMessageID() const override {
			return m_MessageID;
		}

		ConstNetBufferSeq GetBuffers() const override {
			ConstNetBufferSeq result;
			for( const auto& buf : m_Data ) {
				result.push_back( &buf.ref );
			}
			return result;
		}
	};


	/***************************************************************************
	*	                         UDP transport methods
	***************************************************************************/

	Error TransportUDP::Open( const std::string& uri, EOpen mode, OnReceiveMsg onmsg ) {
		std::vector<net::SocketAddress::Ptr> addresses = net::SocketAddress::Parse( uri, 30000 );
		if( addresses.empty() ) {
			return Error::InvalidSettings;
		}

		m_Socket = net::SocketUDP::Create();
		if( m_Socket == nullptr ) {
			return Error::Fatal;
		}

		if( mode == EOpen::Listen ) {
			Error err = m_Socket->Bind( addresses[ 0 ] );
			if( err != Error::Ok ) {
				return err;
			}
		} else if( mode == EOpen::Connect ) {
			m_RemoteAddress = addresses[ 0 ];
		}

		m_BuffersStore = std::make_shared<NetBuffersStore>();
		m_SendingQueue = std::make_shared<SendingQueue>();

		auto fn_onreceive = &TransportUDP::OnReceive;
		m_ReceivingQueue = std::make_shared<ReceivingQueue>(
			[=]( MessageID msg_id, std::list<NetBuffer>& buffers ) { ( this->*fn_onreceive )( msg_id, buffers ); } );

		m_OnNewMessage = onmsg;

		m_IsRunning = true;
		m_NetworkThread = std::make_unique<std::thread>( &TransportUDP::NetworkWork, this );

		return Error::Ok;
	}

	IMsgCompose::Ptr TransportUDP::ComposeMsg() {
		assert( m_BuffersStore != nullptr );
		assert( m_SendingQueue != nullptr );
		return std::make_shared<MsgComposeUDP>( m_MessageID++, m_BuffersStore, m_SendingQueue );
	}

	Error TransportUDP::Close() {
		m_IsRunning = false;

		if( m_NetworkThread != nullptr ) {
			if( m_NetworkThread->joinable() ) {
				m_NetworkThread->join();
			}
			m_NetworkThread = nullptr;
		}

		if( m_Socket != nullptr ) {
			m_Socket->Close();
			m_Socket = nullptr;
		}
		
		m_SendingQueue = nullptr;
		m_BuffersStore = nullptr;
		return Error::Ok;
	}

	void TransportUDP::NetworkWork() {
		using namespace std::chrono_literals;

		FD_SET read_set;
		FD_SET write_set;
		FD_SET err_set;

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;

		while( m_IsRunning && m_Socket != nullptr ) {
			FD_ZERO( &read_set );
			FD_ZERO( &write_set );
			FD_ZERO( &err_set );

			net::basesocket socket = m_Socket->GetSocket();
			FD_SET( socket, &read_set );
			FD_SET( socket, &write_set );
			FD_SET( socket, &err_set );

			int res = ::select( 1, &read_set, &write_set, &err_set, &timeout );
			if( !m_IsRunning ) {
				// stop thread asap
				break;
			}

			//printf( "%p::NetworkWork() - select()=%i\n", this, res );
			if( res != SOCKET_ERROR ) {
				if( FD_ISSET( socket, &err_set ) ) {
					// TODO: handle it
					printf( "%p::NetworkWork() - error set!\n", this );
				}

				// TODO: refactor: split to functions
				if( FD_ISSET( socket, &read_set ) ) {
					std::list<NetBuffer> read_list;
					if( m_BuffersStore->Alloc( read_list, m_MTUSize ) ) {
						auto& buf = read_list.back();
						// NOTE: IPV4 only
						::sockaddr from;
						int fromlen = sizeof( from );
						res = ::recvfrom( socket, buf.GetBuffer(), buf.GetBufferSize(), 0, &from, &fromlen );
						printf( "%p::NetworkWork() - recvfrom()=%i/%i\n", this, res, ::WSAGetLastError() );
						if( res != -1 ) {
							if( m_RemoteAddress == nullptr ) {
								m_RemoteAddress = std::make_shared<net::SocketAddress>( from );
							}

							UDPPacketHeader* ph = reinterpret_cast<UDPPacketHeader*>( buf.GetBuffer() );
							buf.ref.data = buf.buffer.data() + sizeof( UDPPacketHeader );
							buf.ref.size = res - sizeof( UDPPacketHeader );

							if( ( ph->flags & static_cast<byte>( UDPPacketFlag::Response ) ) != 0 ) {
								// reponse
								m_SendingQueue->ProcessResponse( ph->msg_id, read_list );
							} else {
								// payload
								m_ReceivingQueue->ProcessBuffer( ph->msg_id, read_list );
							}
						} else {
							// TODO: handle it
						}
						m_BuffersStore->Release( read_list );
					} else {
						// TODO: handle it
						printf( "%p::NetworkWork() - unable allocate NetBuffer!\n", this );
					}
				}
				if( FD_ISSET( socket, &write_set ) && m_RemoteAddress != nullptr ) {
					const NetBuffer* net_buffer = m_SendingQueue->GetNextBufferPacket();
					if( net_buffer != nullptr ) {
						Error err;
						const auto addr = &m_RemoteAddress->GetSockAddress();
						int addr_len = sizeof( sockaddr );
						auto data = net_buffer->GetData();
						int data_len = net_buffer->GetDataSize();
						res = ::sendto( socket, data, data_len, 0, addr, addr_len );
						printf( "%p::NetworkWork() - sendto()=%i/%i\n", this, res, ::WSAGetLastError() );
						if( res != -1 ) {
							err = Error::Ok;
						} else {
							err = Error::SentError;
						}
						auto ph = net_buffer->GetPacketHeader();
						if( ( ph.flags & static_cast<byte>( UDPPacketFlag::Last ) ) != 0 ) {
							m_SendingQueue->SentReport( ph.msg_id, err );
							printf( "%p::NetworkWork() - SentReport( %u, %i )\n", this, ph.msg_id,
									static_cast<int>( err ) );
						}
					}
				}
			}
		}
	}

	void TransportUDP::OnReceive( MessageID msg_id, std::list<NetBuffer>& buffers ) {
		MsgReceivedUDP::Ptr msg = std::make_shared<MsgReceivedUDP>( m_BuffersStore, msg_id, buffers );
		if( m_OnNewMessage ) {
			m_OnNewMessage( this, std::static_pointer_cast<IMsgReceived>( msg ) );
		}
	}

}  // namespace transports
}  // namespace comm
