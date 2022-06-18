/**
*	Contains stuff for reading/writing data from/to NetworkBufferSeq
*/
#pragma once

#include "Transport.h"
#include <functional>
#include <cassert>
#include <algorithm>

namespace comm {
namespace utils {
	namespace traits {
		enum class ETypes : byte {
			UInt32 = 1,
			Byte = 2,
			Char = 3,
			String = 4,
			BytesArray = 5,
		};

		template<typename TYPE>
		byte TypeToByte();

		template<typename TYPE>
		size_t ValueToSize( const TYPE& val );

		template<typename TYPE>
		const byte* ValueToData( const TYPE& val );

		struct Output {
			byte* data;
			uint32_t size;
		};

		template<typename TYPE>
		Output GetValueBuffer( TYPE& val, uint32_t size );

	}  // namespace traits

	class ChunkWriter {
	public:
		using Allocator = std::function<NetBufferRef*( size_t )>;
		using Writer = std::function<void( NetBufferRef*, size_t )>;

	protected:
		Allocator m_Allocator;
		Writer m_Writer;
		NetBufferSeq m_Buffers;
		size_t m_Buffer{ 0 };
		size_t m_PosInBuffer{ 0 };

	public:
		ChunkWriter( Allocator allocator, Writer writer )
			: m_Allocator( allocator )
			, m_Writer( writer ) {}

		bool CheckAndWrite( byte type, const byte* data, size_t len ) {
			size_t size = len + sizeof( type ) + sizeof( uint32_t );
			if( !CheckAndAlloc( size ) ) {
				return false;
			}

			uint32_t size32 = static_cast<uint32_t>( size );
			WriteSafe( reinterpret_cast<const byte*>( &size32 ), sizeof( size32 ) );
			WriteSafe( reinterpret_cast<const byte*>( &type ), sizeof( byte ) );
			WriteSafe( data, len );

			return true;
		}

		template<typename TYPE>
		bool Write( const TYPE& val ) {
			return CheckAndWrite( traits::TypeToByte<TYPE>(), traits::ValueToData( val ), traits::ValueToSize( val ) );
		}

		void Flush() {
			NetBufferRef* buffer = m_Buffers[ m_Buffer ];
			m_Writer( buffer, m_PosInBuffer );
		}

	protected:
		bool CheckAndAlloc( size_t size ) {
			size_t available = m_Buffers.empty() ? 0 : m_Buffers[ m_Buffer ]->size - m_PosInBuffer;
			while( available < size ) {
				size_t req_size = size - available;
				NetBufferRef* net_buffer = m_Allocator( req_size );
				if( net_buffer == nullptr ) {
					return false;
				}
				m_Buffers.push_back( net_buffer );
				available += net_buffer->size;
			}
			return true;
		}

		void WriteSafe( const byte* data, size_t size ) {
			while( size > 0 ) {
				NetBufferRef* buffer = m_Buffers[ m_Buffer ];
				size_t available = buffer->size - m_PosInBuffer;
				if( available == 0 ) {
					m_Writer( buffer, m_PosInBuffer );
					buffer = m_Buffers[ ++m_Buffer ];
					available = buffer->size;
					m_PosInBuffer = 0;
				}
				size_t copy_size = std::min( available, size );
				std::memcpy( buffer->data + m_PosInBuffer, data, copy_size );
				size -= copy_size;
				data += copy_size;
				m_PosInBuffer += copy_size;
			}
		}
	};

	/**
	*	Read data from chunked stream
	*/
	class ChunkReader {
	protected:
		struct ReadContext {
			size_t buffer{ 0 };
			size_t pos_in_buffer{ 0 };
		};

		const ConstNetBufferSeq& m_Buffers;
		ReadContext m_Current;

	public:
		ChunkReader( const ConstNetBufferSeq& buffers )
			: m_Buffers( buffers ) {}

		/**
		*	Read data of specified type from stream
		*	@param val - output for data
		*	@return - true - readed, false - error
		*/
		template<typename TYPE>
		bool Read( TYPE& val ) {
			uint32_t size;
			if( !CheckTypeAndSize( traits::TypeToByte<TYPE>(), size ) ) {
				return false;
			}

			auto output = traits::GetValueBuffer<TYPE>( val, size );
			if( output.size != size ) {
				ReadUnSafe( nullptr, size );
				return false;
			}

			ReadUnSafe( output.data, size );

			return true;
		}

	protected:
		/**
		*	Read data and monitor for EOS
		*	@param buffer - pointer to put read data, nullptr - means skip data
		*	@param size - specifies amount of read data
		*	@return true - if all data is copied/skipped, otherwise stream does not have enough data
		*/
		bool ReadUnSafe( byte* buffer, size_t size ) {
			while( size > 0 ) {
				auto buf = m_Buffers[ m_Current.buffer ];
				size_t available = buf->size - m_Current.pos_in_buffer;
				if( available == 0 ) {
					if( ( m_Current.buffer + 1 ) >= m_Buffers.size() ) {
						// no more data
						return false;
					}
					m_Current.buffer++;
					m_Current.pos_in_buffer = 0;
					buf = m_Buffers[ m_Current.buffer ];
					available = buf->size - m_Current.pos_in_buffer;
					if( available == 0 ) {
						// no more data
						return false;
					}
				}
				size_t copy_bytes = std::min( size, available );
				if( buffer != nullptr ) {
					std::memcpy( buffer, buf->data + m_Current.pos_in_buffer, copy_bytes );
					buffer += copy_bytes;
				}
				m_Current.pos_in_buffer += copy_bytes;
				size -= copy_bytes;
			}
			return true;
		}

		/**
		*	Read size and type from stream and check them
		*	@param type - expected type
		*	@param size - [output] size of block
		*	@return true - if everyting is ok, otherwise false (stream is broken or expected type is different)
		*/
		bool CheckTypeAndSize( byte type, uint32_t& size ) {
			ReadContext save{ m_Current };

			if( !ReadUnSafe( reinterpret_cast<byte*>( &size ), sizeof( size ) ) ) {
				return false;
			}

			if( size < ( sizeof( uint32_t ) + sizeof( byte ) ) ) {
				return false;
			}

			byte read_type;

			if( !ReadUnSafe( &read_type, sizeof( read_type ) ) ) {
				return false;
			}

			if( read_type != type ) {
				m_Current = save;
				return false;
			}

			size -= sizeof( uint32_t ) + sizeof( byte );

			return true;
		}
	};

}  // namespace utils
}  // namespace comm