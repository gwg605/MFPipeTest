/**
*	This file contains generic transport stuff
*/
#pragma once

#include "MFTypes.h"
#include <memory>
#include <functional>
#include <vector>

namespace comm {

/// info about payload in buffer
struct NetBufferRef {
	/// pointer to actual data
	byte* data;
	/// size of data in this buffer
	size_t size;
};

using MessageID = uint32_t;

// TODO: implement class to handle buffers seq with automated release buffers into packets store

using NetBufferSeq = std::vector<NetBufferRef*>;
using ConstNetBufferSeq = std::vector<const NetBufferRef*>;

/**
*	Interface to received mesage
*/
class IMsgReceived {
public:
	using Ptr = std::shared_ptr<IMsgReceived>;

	virtual ~IMsgReceived() = default;

	/// get mesage id
	virtual MessageID GetMessageID() const = 0;

	/// get message data as seq of network buffers
	virtual ConstNetBufferSeq GetBuffers() const = 0;
};

/**
*	Interface to composing message
*/
class IMsgCompose {
public:
	using Ptr = std::shared_ptr<IMsgCompose>;
	using FnOnSent = std::function<void( const Error& )>;

	virtual ~IMsgCompose() = default;

	/// Alloc new buffer for data
	virtual NetBufferRef* AllocBuffer() = 0;

	/// specify how many data is written
	virtual Error Write( NetBufferRef* buf, size_t len ) = 0;

	/// send message and provide notifucation handler
	virtual Error Send( bool failed, const FnOnSent& onsent ) = 0;

	/// close message when notification is received or just forgot about the message
	virtual void Close() = 0;
};

/**
*	Transport interface
*
*	Generic use cases:
*	- Sending message:
*		- msg = ComposeMsg() - return message for composing
*		- serialize:
*			- buf = msg->AllocBuffer()
*			- msg->Write( buf, size ) - tell how many data written to the buffer
*		- msg->Send( false, handler ) - start sending message and specify notification handler
*		- Wait for [Handler]
*			- get status
*
*	- Receiving message:
*		- wait for [OnReceiveMsg] handler
*			- deserialize:
*				- GetBuffers()
*				- Read from buffers
*
*/
class ITransport {
public:
	using Ptr = std::shared_ptr<ITransport>;

	enum EOpen { Listen, Connect };

	/// prototype for notification handler
	using OnReceiveMsg = std::function<void( ITransport*, const IMsgReceived::Ptr& )>;

public:
	/// open transport
	virtual Error Open( const std::string& uri, EOpen mode, OnReceiveMsg onmsg ) = 0;

	/// create compoing message
	virtual IMsgCompose::Ptr ComposeMsg() = 0;

	/// close transport
	virtual Error Close() = 0;
};

/**
*	Transport factory - creates transport based on settings(proto)
*	Limitation: 'udp' supported only
*/
class TransportFactory {
public:
	static ITransport::Ptr CreateTransport( const std::string& proto );
};

}  // namespace comm
