#include "ChunkReaderWriter.h"

namespace comm {
namespace utils {
namespace traits {

	template<>
	byte TypeToByte<uint32_t>() {
		return static_cast<byte>( ETypes::UInt32 );
	}

	template<>
	size_t ValueToSize( const uint32_t& val ) {
		return sizeof( val );
	}

	template<>
	const byte* ValueToData( const uint32_t& val ) {
		return reinterpret_cast<const byte*>( &val );
	}

	template<>
	Output GetValueBuffer( uint32_t& val, uint32_t size ) {
		return Output{ reinterpret_cast<byte*>( &val ), sizeof( uint32_t ) };
	}

	template<>
	byte TypeToByte<byte>() {
		return static_cast<byte>( ETypes::Byte );
	}

	template<>
	size_t ValueToSize( const byte& val ) {
		return sizeof( val );
	}

	template<>
	const byte* ValueToData( const byte& val ) {
		return reinterpret_cast<const byte*>( &val );
	}

	template<>
	Output GetValueBuffer( byte& val, uint32_t size ) {
		return Output{ reinterpret_cast<byte*>( &val ), sizeof( byte ) };
	}

	template<>
	byte TypeToByte<char>() {
		return static_cast<byte>( ETypes::Char );
	}

	template<>
	size_t ValueToSize( const char& val ) {
		return sizeof( val );
	}

	template<>
	const byte* ValueToData( const char& val ) {
		return reinterpret_cast<const byte*>( &val );
	}

	template<>
	Output GetValueBuffer( char& val, uint32_t size ) {
		return Output{ reinterpret_cast<byte*>( &val ), sizeof( char ) };
	}

	template<>
	byte TypeToByte<std::string>() {
		return static_cast<byte>( ETypes::String );
	}

	template<>
	size_t ValueToSize( const std::string& val ) {
		return val.size();
	}

	template<>
	const byte* ValueToData( const std::string& val ) {
		return reinterpret_cast<const byte*>( val.data() );
	}

	template<>
	Output GetValueBuffer( std::string& val, uint32_t size ) {
		val.resize( size );
		return Output{ reinterpret_cast<byte*>( val.data() ), size };
	}

	template<>
	byte TypeToByte<std::vector<byte>>() {
		return static_cast<byte>( ETypes::BytesArray );
	}

	template<>
	size_t ValueToSize( const std::vector<byte>& val ) {
		return val.size();
	}

	template<>
	const byte* ValueToData( const std::vector<byte>& val ) {
		return reinterpret_cast<const byte*>( val.data() );
	}

	template<>
	Output GetValueBuffer( std::vector<byte>& val, uint32_t size ) {
		val.resize( size );
		return Output{ reinterpret_cast<byte*>( val.data() ), size };
	}

}  // namespace traits
}  // namespace utils
}  // namespace comm
