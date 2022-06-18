# Overview

This is test task. Implemented by Valery Gordeev (gwg605@gmail.com)

# Requirements

1. Generic:
   1. Implement MFPipe class and the following methods: PipeCreate, PipeOpen, PipePut, PipeGet and PipeClose
   2. Finish TestMethod1 method in unittest_mfpipe.cpp
   3. Create one more unit test for implementation
   4. Data transmittion over UDP or Pipes
2. Implementation:
   1. Pipe may have one client only
   2. Allow adding other protocols (TCP for example)
   3. Allow multiple streams per connection
   4. Allow transmittion of MF_BUFFER objects
   5. Support multi-threading
3. Optional
   1. Strings transmittion
   2. Implement PipeInfoGet, PipePeek, PipeFlush
   3. Implement multi-threading test reading/writing
   4. MF_FRAME object transmittion
   5. Implement more unit tests
   6. Implement cross-platform application

# Architecture
The functionality is split to three layers:
- Connection layer - represented by MFPipeImpl. It implements muxing/demuxing channels traffic, works with objects and messages.
- Transport layer - represented by ITransport and TransportUDP. Transport provides API to send and receive messages. TransportUDP implenets the transport interface based on UDP protocol.
- Envronment layer - some helper layer provides funtionality to serialize/deserialize data, network packets store and so on.

# Implementation notes
PipeImpl supports:
- UDP transport
- P2P communication only
- Support unlimited number of channels
- Bi-directional communication
- UDP transport:
	- may lost packets
	- may not keep packets order
	- one thread per instance
- Written on VS2017 with C++17 standard and STL
- namespaces:
	- comm - primary interfaces and code
	- comm::transport - transport implementations
	- comm::utils - utilities: chunk reader/writer and stuff for serialization/deserialization
	- comm::net - network specific parts (sockets, addresses and so on)

# What is done
- Implemented MFPipeImpl class:
	- PipeCreate - open pipe as receiving/server part
	- PipeOpen - open as streaming/client part
	- PipePut - send object
	- PipeGet - receive object
	- PipeMessagePut - send message
	- PipeMessageGet - receive message
	- PipeClose - close pipe
- Tests:
	- Serialization/deserialization
	- Single-thread test
	- Multi-thread test

# What is not complete
- MF_BUFFER/MF_FRAME - not all data members are seriazable (just need time)
- MFPipeImpl:
	- PipePeek
	- PipeFlush
	- PipeInfoGet
	- Some parameters in implemented methods may be ignored (like strHints, maxBuffers and so on)
- No memory and queue size limitations
