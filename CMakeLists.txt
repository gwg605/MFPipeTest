project(MFPipe_Test)

set(CMAKE_CXX_STANDARD 17)

set(SOURCES
	unittest_mfpipe.cpp
	Transport.cpp
	TransportUDP.cpp
	MFPipeImpl.cpp
	ChunkReaderWriter.cpp
	URL.cpp
	SocketUDP.cpp
	MFObjects.cpp
)

set(HEADERS
	MFPipe.h
	MFPipeImpl.h
	MFTypes.h
	MFObjects.h
	Transport.h
	TransportUDP.h
	ChunkReaderWriter.h
	URL.h
	SocketUDP.h
)

add_executable(MFPipe_Test ${SOURCES} ${HEADERS})

if(WIN32)
  target_link_libraries(MFPipe_Test wsock32 ws2_32)
endif()
