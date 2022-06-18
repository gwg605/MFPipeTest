#pragma once

#include "MFTypes.h"
#include <WinSock2.h>
#include <memory>

namespace comm {
namespace net {

	using basesocket = SOCKET;
	using socklen_t = int;
	using socket_address_ipv4 = uint32_t;
	using socket_addr = ::sockaddr;
	using socket_port = uint16_t;

	/**
	*	Represent socket address
	*/
	class SocketAddress {
	public:
		using Ptr = std::shared_ptr<SocketAddress>;

	protected:
		// NOTE: This is not correct way to store any type of address, but for IP4 is enough
		socket_addr m_Address{ 0 };

	public:
		SocketAddress( const socket_addr& addr )
			: m_Address( addr ) {}

		SocketAddress( const ::sockaddr_in& addrin )
			: m_Address( *reinterpret_cast<const socket_addr*>( &addrin ) ) {}

		const sockaddr& GetSockAddress() const {
			return m_Address;
		}

	public:
		/// parse string to list of addresses, zero items - means unable to parse or error
		static std::vector<SocketAddress::Ptr> Parse( const std::string& address, socket_port port );
	};

	/**
	*	UDP socket helper
	*/
	class SocketUDP {
	public:
		using Ptr = std::shared_ptr<SocketUDP>;

	protected:
		basesocket m_Socket;

	public:
		SocketUDP( basesocket socket )
			: m_Socket( socket ) {}

		basesocket GetSocket() const {
			return m_Socket;
		}

		static SocketUDP::Ptr Create();

		SocketAddress::Ptr GetLocalAddress() const;

		Error Bind( const SocketAddress::Ptr& local_addr );

		Error ReceiveFrom();

		Error SendTo( const SocketAddress::Ptr& remote_addr, const byte* data, size_t len );

		Error Close();
	};

}  // namespace net
}  // namespace comm
