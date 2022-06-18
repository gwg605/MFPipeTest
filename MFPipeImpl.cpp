#include "MFPipeImpl.h"
#include "ChunkReaderWriter.h"
#include <thread>
#include <chrono>
#include <condition_variable>
#include <atomic>

using namespace std::chrono_literals;

namespace comm {

Error MFPipeImpl::PipeCreate( /*[in]*/ const std::string &strPipeID, /*[in]*/ const std::string &strHints ) {
	m_Transport = comm::TransportFactory::CreateTransport( strPipeID );
	if( m_Transport == nullptr ) {
		return Error::InvalidSettings;
	}
	auto onmsg = &MFPipeImpl::OnNewMessage; 
	return m_Transport->Open( strPipeID, comm::ITransport::EOpen::Listen,
							  [=]( ITransport *transport, const IMsgReceived::Ptr &msg ) { ( this->*onmsg )( msg ); } );
}

Error MFPipeImpl::PipeOpen( /*[in]*/ const std::string &strPipeID, /*[in]*/ int _nMaxBuffers,
							/*[in]*/ const std::string &strHints ) {
	m_Transport = comm::TransportFactory::CreateTransport( strPipeID );
	if( m_Transport == nullptr ) {
		return Error::InvalidSettings;
	}

	auto onmsg = &MFPipeImpl::OnNewMessage;
	return m_Transport->Open( strPipeID, comm::ITransport::EOpen::Connect,
							  [=]( ITransport *transport, const IMsgReceived::Ptr &msg ) { ( this->*onmsg )( msg ); } );
}

Error MFPipeImpl::PipePut( /*[in]*/ const std::string &strChannel,
						   /*[in]*/ const std::shared_ptr<MF_BASE_TYPE> &pBufferOrFrame,
			   /*[in]*/ int _nMaxWaitMs, /*[in]*/ const std::string &strHints ) {

	std::atomic<bool> complete{ false };
	std::mutex complete_lock;
	std::condition_variable complete_check;
	Error result = Error::Timeout;

	auto msg = m_Transport->ComposeMsg();

	auto allocator = [=]( size_t size ) -> comm::NetBufferRef * { return msg->AllocBuffer(); };
	auto writer = [=]( comm::NetBufferRef *buf, size_t len ) { msg->Write( buf, len ); };
	utils::ChunkWriter chunk_writer( allocator, writer );

	bool res = chunk_writer.Write( static_cast<byte>( ERecordType::Data ) );
	res &= chunk_writer.Write( strChannel );
	res &= chunk_writer.Write( static_cast<byte>( pBufferOrFrame->GetObjectType() ) );
	res &= pBufferOrFrame->Write( chunk_writer );
	chunk_writer.Flush();
	msg->Send( !res, [&]( const Error &err ) {
		std::unique_lock l( complete_lock );
		complete = true;
		result = err;
		complete_check.notify_all();
	} );

	std::unique_lock lock( complete_lock );
	auto status = complete_check.wait_for( lock, std::max( 100, _nMaxWaitMs ) * 1ms,
										   [&]() { return complete.load(); } );
	if( !status && _nMaxWaitMs != 0 ) {
		// timeout
		printf( "%p::MFPipeImpl - PipePut() - Timeout()\n", this );
	}

	msg->Close();

	printf( "%p::MFPipeImpl - PipePut()=%i\n", this, static_cast<int>( result ) );

	return result;
}

Error MFPipeImpl::PipeGet( /*[in]*/ const std::string &strChannel,
						   /*[out]*/ std::shared_ptr<MF_BASE_TYPE> &pBufferOrFrame,
			   /*[in]*/ int _nMaxWaitMs, /*[in]*/ const std::string &strHints ) {

	Error result = Error::Ok;
	std::unique_lock lock( m_ReceivingLock );

	auto check_received = &MFPipeImpl::CheckReceived;
	auto status = m_ReceivingVariable.wait_for( lock, std::max( 100, _nMaxWaitMs ) * 1ms, [&]() {
		auto record = ( this->*check_received )( strChannel, ERecordType::Data );
		if( record != nullptr ) {
			pBufferOrFrame = record->object;
		}
		return record != nullptr;
	} );

	if( !status && _nMaxWaitMs != 0 ) {
		// timeout
		result = Error::Timeout;
		printf( "%p::MFPipeImpl - PipeGet() - Timeout()\n", this );
	}

	printf( "%p::MFPipeImpl - PipeGet()=%i\n", this, static_cast<int>( result ) );
	return result;
}

Error MFPipeImpl::PipeMessagePut(
	/*[in]*/ const std::string &strChannel,
	/*[in]*/ const std::string &strEventName,
	/*[in]*/ const std::string &strEventParam,
	/*[in]*/ int _nMaxWaitMs ) {

	std::atomic<bool> complete{ false };
	std::mutex complete_lock;
	std::condition_variable complete_check;
	Error result = Error::Timeout;

	auto msg = m_Transport->ComposeMsg();

	auto allocator = [=]( size_t size ) -> comm::NetBufferRef * { return msg->AllocBuffer(); };
	auto writer = [=]( comm::NetBufferRef *buf, size_t len ) { msg->Write( buf, len ); };
	utils::ChunkWriter chunk_writer( allocator, writer );

	bool res = chunk_writer.Write( static_cast<byte>( ERecordType::Message ) );
	res &= chunk_writer.Write( strChannel );
	res &= chunk_writer.Write( strEventName );
	res &= chunk_writer.Write( strEventParam );
	chunk_writer.Flush();
	msg->Send( !res, [&]( const Error &err ) {
		std::unique_lock l( complete_lock );
		complete = true;
		result = err;
		complete_check.notify_all();
	} );

	std::unique_lock lock( complete_lock );
	auto status = complete_check.wait_for( lock, std::max( 100, _nMaxWaitMs ) * 1ms,
										   [&]() { return complete.load(); } );
	if( !status && _nMaxWaitMs != 0 ) {
		// timeout
		printf( "%p::MFPipeImpl - PipeMessagePut() - Timeout()\n", this );
	}

	msg->Close();

	printf( "%p::MFPipeImpl - PipeMessagePut()=%i\n", this, static_cast<int>( result ) );

	return result;
}

Error MFPipeImpl::PipeMessageGet(
	/*[in]*/ const std::string &strChannel,
	/*[out]*/ std::string *pStrEventName,
	/*[out]*/ std::string *pStrEventParam,
	/*[in]*/ int _nMaxWaitMs) {

	Error result = Error::Ok;
	std::unique_lock lock( m_ReceivingLock );

	auto check_received = &MFPipeImpl::CheckReceived;
	auto status = m_ReceivingVariable.wait_for( lock, std::max( 100, _nMaxWaitMs ) * 1ms, [&]() {
		auto record = ( this->*check_received )( strChannel, ERecordType::Message );
		if( record != nullptr ) {
			if( pStrEventName != nullptr ) {
				*pStrEventName = record->msg_name;
			}
			if( pStrEventParam != nullptr ) {
				*pStrEventParam = record->msg_value;
			}
		}
		return record != nullptr;
	} );

	if( !status && _nMaxWaitMs != 0 ) {
		// timeout
		result = Error::Timeout;
		printf( "%p::MFPipeImpl - PipeMessageGet() - Timeout()\n", this );
	}

	printf( "%p::MFPipeImpl - PipeMessageGet()=%i\n", this, static_cast<int>( result ) );
	return result;
}

Error MFPipeImpl::PipeFlush( /*[in]*/ const std::string &strChannel, /*[in]*/ eMFFlashFlags _eFlashFlags ) {
	return Error::Ok;
}

Error MFPipeImpl::PipeClose() {
	m_Transport->Close();
	m_Transport = nullptr;
	return Error::Ok;
}

void MFPipeImpl::OnNewMessage( const IMsgReceived::Ptr &msg ) {
	std::unique_lock lock( m_ReceivingLock );
	m_ReceivedRecords.emplace_back( new Record{ msg } );
	m_ReceivingVariable.notify_all();
}

MFPipeImpl::Record::Ptr MFPipeImpl::CheckReceived( const std::string channel, ERecordType type ) {
	for( auto rec = m_ReceivedRecords.begin(); rec != m_ReceivedRecords.end(); ++rec ) {
		if( (*rec)->type == ERecordType::Unparsed ) {
			ConstNetBufferSeq seq = ( *rec )->msg->GetBuffers();
			utils::ChunkReader chunk_reader( seq );

			byte msg_type;
			bool res = chunk_reader.Read( msg_type );
			res &= chunk_reader.Read( ( *rec )->channel );
			res &= ByteToRecordType( msg_type, ( *rec )->type );
			if( res ) {
				switch( ( *rec )->type ) {
				case ERecordType::Data: {
					byte obj_type;
					res = chunk_reader.Read( obj_type );
					if( res ) {
						(*rec)->object = MF_BASE_TYPE::CreateByObjectType( static_cast<ObjectType>( obj_type ) );
						if( ( *rec )->object != nullptr ) {
							res = ( *rec )->object->Load( chunk_reader );
						} else {
							res = false;
						}
					}
				} break;
				case ERecordType::Message: {
					res = chunk_reader.Read( ( *rec )->msg_name );
					res &= chunk_reader.Read( ( *rec )->msg_value );
				} break;
				default:
					res = false;
				}
			}

			printf( "%p::MFPipeImpl - CheckReceived() - parse msg_id=%u\n", this, ( *rec )->msg->GetMessageID() );

			if( !res ) {
				auto rec_copy = rec--;
				m_ReceivedRecords.erase( rec_copy );
				continue;
			}
		}
		if( (*rec)->type == type && (*rec)->channel == channel ) {
			// found
			auto result = *rec;
			m_ReceivedRecords.erase( rec );
			return result;
		}
	}

	return nullptr;
}

bool MFPipeImpl::ByteToRecordType( byte msg_type, ERecordType &type ) {
	ERecordType rtype = static_cast<ERecordType>( msg_type );
	if( rtype == ERecordType::Data || rtype == ERecordType::Message ) {
		type = rtype;
		return true;
	}
	return false;
}

}  // namespace comm
