#include "MFPipeImpl.h"
#include "ChunkReaderWriter.h"
#include <iostream>
#include <atomic>

#if defined( WIN32 )
#include <WinSock2.h>
#endif

#define PACKETS_COUNT ( 8 )

#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY( arr ) ( sizeof( arr ) / sizeof( ( arr )[ 0 ] ) )
#endif  // SIZEOF_ARRAY

using namespace comm;

int TestMethod1() {
	Error err;

	std::shared_ptr<MF_BUFFER> arrBuffersIn[ PACKETS_COUNT ];
	for( int i = 0; i < SIZEOF_ARRAY( arrBuffersIn ); ++i ) {
		size_t cbSize = 128 * 1024 + rand() % ( 256 * 1024 );
		arrBuffersIn[ i ] = std::make_shared<MF_BUFFER>();

		arrBuffersIn[ i ]->flags = eMFBF_Buffer;
		// Fill buffer
		// TODO: fill by test data here
	}

	std::string pstrEvents[] = { "event1", "event2", "event3", "event4", "event5", "event6", "event7", "event8" };
	std::string pstrMessages[] = { "message1", "message2", "message3", "message4",
								   "message5", "message6", "message7", "message8" };

	//////////////////////////////////////////////////////////////////////////
	// Pipe creation

	// Write pipe
	MFPipeImpl MFPipe_Read;
	MFPipe_Read.PipeCreate( "udp://127.0.0.1:12345", "" );

	// Read pipe
	MFPipeImpl MFPipe_Write;
	MFPipe_Write.PipeOpen( "udp://127.0.0.1:12345", 32, "" );

	//////////////////////////////////////////////////////////////////////////
	// Test code (single-threaded)
	// TODO: multi-threaded

	// Note: channels ( "ch1", "ch2" is optional)

	for( int i = 0; i < 8; ++i ) {
		// Write to pipe
		MFPipe_Write.PipePut( "ch1", arrBuffersIn[ i % PACKETS_COUNT ], 0, "" );
		MFPipe_Write.PipePut( "ch2", arrBuffersIn[ ( i + 1 ) % PACKETS_COUNT ], 0, "" );
		MFPipe_Write.PipeMessagePut( "ch1", pstrEvents[ i % PACKETS_COUNT ], pstrMessages[ i % PACKETS_COUNT ], 100 );
		MFPipe_Write.PipeMessagePut( "ch2", pstrEvents[ ( i + 1 ) % PACKETS_COUNT ],
									 pstrMessages[ ( i + 1 ) % PACKETS_COUNT ], 100 );

		MFPipe_Write.PipePut( "ch1", arrBuffersIn[ i % PACKETS_COUNT ], 0, "" );
		MFPipe_Write.PipePut( "ch2", arrBuffersIn[ ( i + 1 ) % PACKETS_COUNT ], 0, "" );

		std::string strPipeName;
		MFPipe_Read.PipeInfoGet( &strPipeName, "", NULL );

		MFPipe::MF_PIPE_INFO mfInfo = {};
		MFPipe_Read.PipeInfoGet( NULL, "ch2", &mfInfo );
		MFPipe_Read.PipeInfoGet( NULL, "ch1", &mfInfo );

		// Read from pipe
		std::string arrStrings[ 4 ];

		err = MFPipe_Read.PipeMessageGet( "ch1", &arrStrings[ 0 ], &arrStrings[ 1 ], 100 );
		assert( err == Error::Ok );
		err = MFPipe_Read.PipeMessageGet( "ch2", &arrStrings[ 2 ], &arrStrings[ 3 ], 100 );
		assert( err == Error::Ok );
		err = MFPipe_Read.PipeMessageGet( "ch2", NULL, &arrStrings[ 2 ], 100 );
		assert( err == Error::Timeout );

		std::shared_ptr<MF_BASE_TYPE> arrBuffersOut[ 8 ];
		err = MFPipe_Read.PipeGet( "ch1", arrBuffersOut[ 0 ], 100, "" );
		assert( err == Error::Ok );
		err = MFPipe_Read.PipeGet( "ch2", arrBuffersOut[ 1 ], 100, "" );
		assert( err == Error::Ok );

		err = MFPipe_Read.PipeGet( "ch1", arrBuffersOut[ 4 ], 100, "" );
		assert( err == Error::Ok );
		err = MFPipe_Read.PipeGet( "ch2", arrBuffersOut[ 5 ], 100, "" );
		assert( err == Error::Ok );
		err = MFPipe_Read.PipeGet( "ch2", arrBuffersOut[ 6 ], 100, "" );
		assert( err == Error::Timeout );
	}

	MFPipe_Write.PipeClose();
	MFPipe_Read.PipeClose();

	return 0;
}

int TestMethod2() {
	// Multi-threading test
	// multiple threads comminucates through the single connection

	// Write pipe
	MFPipeImpl MFPipe_Read;
	MFPipe_Read.PipeCreate( "udp://127.0.0.1:12345", "" );

	// Read pipe
	MFPipeImpl MFPipe_Write;
	MFPipe_Write.PipeOpen( "udp://127.0.0.1:12345", 32, "" );

	const auto processor_count = std::thread::hardware_concurrency();
	const int iterations = 32;

	std::vector<std::unique_ptr<std::thread>> threads;
	threads.resize( processor_count );
	std::atomic<int> channel{ 0 };
	for( auto& t : threads ) {
		t.reset( new std::thread( [&]() {
			Error err;
			int ch_num = channel++;
			std::string ch = "channel#" + std::to_string( ch_num );
			printf( "Thread[%s] - Started\n", ch.c_str() );

			for( int i = 0; i < iterations; i++ ) {
				std::string event_name = "name#" + std::to_string( i );
				err = MFPipe_Write.PipeMessagePut( ch, event_name, "Parameters!!!!!", 100 );
				assert( err == Error::Ok );

				std::string ren;
				std::string rep;
				err = MFPipe_Read.PipeMessageGet( ch, &ren, &rep, 100 );
				assert( err == Error::Ok );

				assert( ren == event_name );
			}

			printf( "Thread[%s] - Finished\n", ch.c_str() );
		} ) );
	}

	// wait for completion
	for( auto& t : threads ) {
		if( t->joinable() ) {
			t->join();
		}
	}

	MFPipe_Write.PipeClose();
	MFPipe_Read.PipeClose();

	return 0;
}

void TestChunkReaderAndWriter() {
	using namespace comm::utils;

	struct Buf {
		std::vector<comm::byte> buffer;
		comm::NetBufferRef ref;
	};

	std::string str{ "string6789ABCDEF0123" };
	std::string emptystr;
	std::string zerostr{ "" };
	std::vector<comm::byte> bb{ 0x00, 0x55, 0xAA };

	for( size_t sz = 1; sz <= 50; sz++ ) {
		std::vector<std::unique_ptr<Buf>> buffers;
		auto allocator = [&]( size_t size ) -> comm::NetBufferRef* {
			buffers.emplace_back( new Buf() );
			auto& buf = buffers.back();
			buf->buffer.resize( sz );
			buf->ref.data = buf->buffer.data();
			buf->ref.size = buf->buffer.size();
			return &buf->ref;
		};

		auto writer = []( comm::NetBufferRef* buf, size_t len ) { buf->size = len; };

		ChunkWriter chunk_writer( allocator, writer );
		bool res = chunk_writer.Write( (uint32_t)1000 );
		res &= chunk_writer.Write( (char)'a' );
		res &= chunk_writer.Write( (char)'b' );
		res &= chunk_writer.Write( str );
		res &= chunk_writer.Write( emptystr );
		res &= chunk_writer.Write( zerostr );
		res &= chunk_writer.Write( bb );
		chunk_writer.Flush();

		size_t total_size = 0;
		std::vector<comm::byte> output;
		std::for_each( buffers.begin(), buffers.end(), [&]( const auto& el ) {
			total_size += el->ref.size;
			output.insert( output.end(), el->buffer.begin(), el->buffer.begin() + el->ref.size );
		} );

		assert( total_size == 64 );

		size_t num_buffers = ( total_size + sz - 1 ) / sz;
		assert( num_buffers == buffers.size() );

		ConstNetBufferSeq seq;
		for( const auto& el : buffers ) {
			seq.push_back( &el->ref );
		}
		ChunkReader chunk_reader( seq );

		uint32_t read_uint32;
		res = chunk_reader.Read( read_uint32 );
		assert( read_uint32 == 1000 );

		char read_c0;
		res &= chunk_reader.Read( read_c0 );
		assert( read_c0 == 'a' );

		char read_c1;
		res &= chunk_reader.Read( read_c1 );
		assert( read_c1 == 'b' );

		std::string read_str;
		res &= chunk_reader.Read( read_str );
		assert( read_str == str );

		std::string read_strempty;
		res &= chunk_reader.Read( read_strempty );
		assert( read_strempty == emptystr );

		std::string read_strzero;
		res &= chunk_reader.Read( read_strzero );
		assert( read_strzero == zerostr );

		std::vector<byte> read_vec;
		res &= chunk_reader.Read( read_vec );
		assert( read_vec == bb );
	}
}

int main( void ) {
#if defined( WIN32 )
	// TODO: Move this stuff to Transport implementation
	WSADATA wsa_data;
	int res = ::WSAStartup( MAKEWORD( 2, 2 ), &wsa_data );
	if( res == 0 )
#endif
	{
		TestChunkReaderAndWriter();
		if( TestMethod1() ) {
			std::cerr << "TestMethod1: Failed" << std::endl;
			return 1;
		}
		if( TestMethod2() ) {
			std::cerr << "TestMethod2: Failed" << std::endl;
			return 1;
		}

#if defined( WIN32 )
		::WSACleanup();
#endif
	}

	return 0;
}
