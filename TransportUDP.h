/*******************************************************************************************************************
*
*	Content:
*	- NetBuffersStore - store for network packets
*	- NetBuffer - single network buffer
*	- SendingQueue - sending queue
*	- ReceivingQueue - receiving queue
*	- TransportUDP - UDP transport itself
*
*	Data/Packets flow:
*
*            +--------------------------------NetBuffersStore<----------------------------------------+
*            !                                  ^         !                                           !
*            V                                  !         V                                           !
*      MsgCompose -> SendingQueue(sync) -> TransportUDP[NetworkThread] -> ReceivingQueue(sync) -> MsgReceived
*
*******************************************************************************************************************/

#include "Transport.h"
#include "SocketUDP.h"
#include <mutex>
#include <list>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <map>
#include <queue>
#include <cassert>

namespace comm {
namespace transports {

	enum class UDPPacketFlag : byte {
		First = 0x1,    // mark packet as first
		Last = 0x2,     // mark packet as last
		Response = 0x4  // make packet as response stats from receiving side for sending side
	};

	/**
	*	UDP packet header
	*/
	struct UDPPacketHeader {
		uint32_t flags : 4;
		uint32_t msg_id : 8;
		uint32_t packet : 20;
	};

	static_assert( sizeof( UDPPacketHeader ) == sizeof( uint32_t ), "UDPPacketHeader should have size of uint32" );

	/**
	*	Network Buffer and helper functions
	*/
	struct NetBuffer {
		/// whole packet data buffer
		std::vector<byte> buffer;
		/// info about packet payload
		NetBufferRef ref;

		char* GetBuffer() {
			return reinterpret_cast<char*>( buffer.data() );
		}

		int GetBufferSize() const {
			return static_cast<int>( buffer.size() );
		}

		const char* GetData() const {
			return reinterpret_cast<const char*>( buffer.data() );
		}

		int GetDataSize() const {
			return static_cast<int>( ref.data + ref.size - buffer.data() );
		}

		/// get reference to packet header
		const UDPPacketHeader& GetPacketHeader() const {
			return *reinterpret_cast<const UDPPacketHeader*>( buffer.data() );
		}
	};

	/**
	*	Store for Network packets
	*/
	class NetBuffersStore {
	public:
		using Ptr = std::shared_ptr<NetBuffersStore>;

	protected:
		std::mutex m_Lock;
		std::list<NetBuffer> m_Data;

	public:
		bool Alloc( std::list<NetBuffer>& out_list, size_t size ) {
			std::unique_lock lock( m_Lock );
			if( !m_Data.empty() ) {
				out_list.splice( out_list.end(), m_Data, std::prev( m_Data.end() ) );
			} else {
				out_list.emplace_back();
			}
			// TODO: Improve packet allocation to avoid possible re-allocations
			out_list.back().buffer.resize( size );
			return true;
		}

		void Release( std::list<NetBuffer>& l ) {
			std::unique_lock lock( m_Lock );
			m_Data.splice( m_Data.end(), l );
		}
	};

	/**
	*	Sending queue:
	*	- contains reference to network buffers for sending
	*	- provides logic to select packets for actual sending
	*	- process response from receiving side (not implemented)
	*	- notify about sending completion
	*/
	class SendingQueue {
	public:
		using Ptr = std::shared_ptr<SendingQueue>;
		using FnSentReport = std::function<void( size_t, const Error& )>;

	protected:
		struct Range {
			uint32_t start;
			uint32_t end;
		};

		struct Record {
			using Ptr = std::shared_ptr<Record>;

			const std::list<NetBuffer>& buffers;
			FnSentReport fn_report;
			std::vector<Range> incomplete;

			Record( const std::list<NetBuffer>& bufs, const FnSentReport& report )
				: buffers( bufs )
				, fn_report( report ) {}
		};

	protected:
		std::mutex m_Lock;
		std::map<MessageID, Record::Ptr> m_Records;
		std::list<const NetBuffer*> m_ToSend;

	public:
		/// Put network buffers of message to sending queue and create control record
		void Send( MessageID msg_id, const std::list<NetBuffer>& buffers, const FnSentReport& report ) {
			assert( !buffers.empty() );

			std::list<const NetBuffer*> send;
			uint32_t min_packet = buffers.front().GetPacketHeader().packet;
			uint32_t max_packet = min_packet;
			auto record = std::make_shared<Record>( buffers, report );
			for( const auto& el : buffers ) {
				const NetBuffer* pn = &el;
				send.push_back( pn );
			}
			record->incomplete.push_back( { min_packet, max_packet } );

			std::unique_lock lock( m_Lock );
			m_Records[ msg_id ] = record;
			m_ToSend.splice( m_ToSend.end(), send );
		}

		/// select next packet for sending
		const NetBuffer* GetNextBufferPacket() {
			std::unique_lock lock( m_Lock );
			if( m_ToSend.empty() ) {
				return nullptr;
			}
			auto result = m_ToSend.front();
			m_ToSend.pop_front();
			return result;
		}

		/// process response stats from receiving side (incomplete)
		void ProcessResponse( MessageID msg_id, std::list<NetBuffer>& buffer ) {
			assert( !buffer.empty() );

			std::unique_lock lock( m_Lock );
			auto found = m_Records.find( msg_id );
			if( found != m_Records.end() ) {
				// found
				Record::Ptr record = found->second;
				auto& ranges = record->incomplete;

				const Range* cur = reinterpret_cast<const Range*>( buffer.front().ref.data );
				const Range* end = reinterpret_cast<const Range*>( buffer.front().ref.data + buffer.front().ref.size );
				while( cur < end ) {
					//TODO: Remove completed ranges
					cur++;
				}

				if( ranges.empty() ) {
					// all data received
					m_Records.erase( found );
					lock.unlock();
					// notify
					record->fn_report( 0, Error::Ok );
				}
			}
		}

		/// Network thread notify the sending queue that last packet sent
		/// This is temporal solution (just to run the tests)
		void SentReport( MessageID msg_id, const Error& err ) {
			std::unique_lock lock( m_Lock );
			auto found = m_Records.find( msg_id );
			if( found != m_Records.end() ) {
				Record::Ptr record = found->second;
				m_Records.erase( found );
				lock.unlock();
				// notify
				record->fn_report( 0, err );
			}
		}
	};

	/**
	*	ReceivingQueue:
	*	- contains receivied network packets grouped by mesage_id
	*	- generate notification about received message
	*/
	class ReceivingQueue {
	public:
		using Ptr = std::shared_ptr<ReceivingQueue>;
		using FnReceiveMessage = std::function<void( MessageID, std::list<NetBuffer>& )>;

	protected:
		struct Record {
			using Ptr = std::shared_ptr<Record>;

			std::list<NetBuffer> buffers;
		};

		FnReceiveMessage m_OnReceiveMessage;
		std::map<MessageID, Record::Ptr> m_Records;

	public:
		ReceivingQueue( const FnReceiveMessage& onreceive )
			: m_OnReceiveMessage( onreceive ) {}

		/// process received network packet from the network thread
		void ProcessBuffer( MessageID msg_id, std::list<NetBuffer>& buffer ) {
			assert( !buffer.empty() );

			auto& record = m_Records[ msg_id ];
			if( record == nullptr ) {
				record = std::make_shared<Record>();
			}
			record->buffers.splice( record->buffers.end(), buffer );

			if( ( record->buffers.back().GetPacketHeader().flags & static_cast<byte>( UDPPacketFlag::Last ) ) != 0 ) {
				m_OnReceiveMessage( msg_id, record->buffers );
				m_Records.erase( msg_id );
			}
		}
	};

	/**
	*	Transport implementation for UDP protocol
	*/
	class TransportUDP : public comm::ITransport, public std::enable_shared_from_this<TransportUDP> {
	protected:
		OnReceiveMsg m_OnNewMessage;
		NetBuffersStore::Ptr m_BuffersStore;
		std::atomic<MessageID> m_MessageID{ 0 };
		net::SocketUDP::Ptr m_Socket;
		std::unique_ptr<std::thread> m_NetworkThread;
		bool m_IsRunning{ false };
		uint32_t m_MTUSize{ 1500 };
		net::SocketAddress::Ptr m_RemoteAddress;
		SendingQueue::Ptr m_SendingQueue;
		ReceivingQueue::Ptr m_ReceivingQueue;

	public:
		/// open transport
		Error Open( const std::string& uri, EOpen mode, OnReceiveMsg onmsg ) override;

		/// create message for sending
		IMsgCompose::Ptr ComposeMsg() override;

		/// close transport
		Error Close() override;

	protected:
		/// working function of the network thread
		void NetworkWork();

		/// new message handler from ReceivingQueue
		void OnReceive( MessageID msg_id, std::list<NetBuffer>& buffers );
	};

}  // namespace transports
}  // namespace comm
