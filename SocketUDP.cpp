#include "SocketUDP.h"
#include "URL.h"
#include <WS2tcpip.h>
#define s6_addr16 s6_words

namespace comm {
namespace net {

	std::vector<SocketAddress::Ptr> SocketAddress::Parse( const std::string& address, socket_port port ) {
		std::vector<SocketAddress::Ptr> result;

		utils::Uri uri = utils::Uri::Parse( address );
		if( !uri.Port.empty() ) {
			port = std::stoi( uri.Port );
		}

		socket_address_ipv4 inet_address = ::inet_addr( uri.Host.c_str() );
		if( inet_address != INADDR_NONE ) {
			::sockaddr_in addrin;
			std::memset( &addrin, 0, sizeof( addrin ) );
			addrin.sin_family = AF_INET;
			addrin.sin_addr.s_addr = inet_address;
			addrin.sin_port = htons( port );
			result.emplace_back( new SocketAddress( addrin ) );
		} else {
			::addrinfo hint;
			::addrinfo* records{ NULL };
			::addrinfo* curr;
			memset( &hint, 0, sizeof hint );

			hint.ai_family = AF_INET;    // supported AF_INET, AF_INET6
			hint.ai_flags = AI_PASSIVE;  // For wildcard IP address
			// hint.ai_socktype = 0; // like SOCK_STREAM
			// hints.ai_protocol = 0;  // like IPPROTO_TCP

			int err = ::getaddrinfo( uri.Host.c_str(), NULL, &hint, &records );
			if( records != NULL ) {
				curr = records;

				while( curr != NULL ) {
					// NOTE: Just IPV4
					::sockaddr_in addrin = *reinterpret_cast<const ::sockaddr_in*>( curr->ai_addr );
					addrin.sin_port = port;
					result.emplace_back( new SocketAddress( addrin ) );
					curr = curr->ai_next;
				}

				::freeaddrinfo( records );
			}
		}

		return result;
	}

	SocketUDP::Ptr SocketUDP::Create() {
		basesocket socket = ::socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
		if( socket == INVALID_SOCKET ) {
			return nullptr;
		}
		return std::make_shared<SocketUDP>( socket );
	}

	SocketAddress::Ptr SocketUDP::GetLocalAddress() const {
		struct sockaddr addr;
		socklen_t len = sizeof( addr );
		if(::getsockname( m_Socket, &addr, &len ) != -1 ) {
			return std::make_shared<SocketAddress>( addr );
		}
		return nullptr;
	}

	Error SocketUDP::Bind( const SocketAddress::Ptr& local_addr ) {
		auto res = ::bind( m_Socket, &local_addr->GetSockAddress(), sizeof( sockaddr_in ) );
		if( res != -1 ) {
			return Error::Ok;
		}
		return Error::Fatal;
	}

	Error SocketUDP::ReceiveFrom() {
		return Error::NotImplemented;
	}

	Error SocketUDP::SendTo( const SocketAddress::Ptr& remote_addr, const byte* data, size_t len ) {
		return Error::NotImplemented;
	}

	Error SocketUDP::Close() {
		if( m_Socket != INVALID_SOCKET ) {
			::closesocket( m_Socket );
			m_Socket = INVALID_SOCKET;
		}

		return Error::Ok;
	}

}  // namespace net
}  // namespace comm
