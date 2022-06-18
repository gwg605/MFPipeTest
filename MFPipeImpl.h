#pragma once

#include "MFTypes.h"
#include "MFPipe.h"
#include "Transport.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <map>

namespace comm {

/**
*	Implements comminication between two instances of MFPipeImpl
*/
class MFPipeImpl : public MFPipe {
public:
	enum class ERecordType : byte {
		Unparsed = 255,
		Data = 0,
		Message = 1,
	};

	struct Record {
		using Ptr = std::shared_ptr<Record>;

		IMsgReceived::Ptr msg;
		ERecordType type{ ERecordType::Unparsed };
		std::string channel;
		MF_BASE_TYPE::Ptr object;
		std::string msg_name;
		std::string msg_value;
	};

protected:
	/// transport
	comm::ITransport::Ptr m_Transport;
	/// lock for receiving queue/records list
	std::mutex m_ReceivingLock;
	/// synchronization
	std::condition_variable m_ReceivingVariable;
	/// receiving queue/records list
	std::vector<Record::Ptr> m_ReceivedRecords;

public:
	Error PipeInfoGet( /*[out]*/ std::string *pStrPipeName, /*[in]*/ const std::string &strChannel,
					   MF_PIPE_INFO *_pPipeInfo ) override {
		return Error::NotImplemented;
	}

	Error PipeCreate( /*[in]*/ const std::string &strPipeID, /*[in]*/ const std::string &strHints ) override;

	Error PipeOpen( /*[in]*/ const std::string &strPipeID, /*[in]*/ int _nMaxBuffers,
					/*[in]*/ const std::string &strHints ) override;

	Error PipePut( /*[in]*/ const std::string &strChannel, /*[in]*/ const std::shared_ptr<MF_BASE_TYPE> &pBufferOrFrame,
				   /*[in]*/ int _nMaxWaitMs, /*[in]*/ const std::string &strHints ) override;

	Error PipeGet( /*[in]*/ const std::string &strChannel, /*[out]*/ std::shared_ptr<MF_BASE_TYPE> &pBufferOrFrame,
				   /*[in]*/ int _nMaxWaitMs, /*[in]*/ const std::string &strHints ) override;

	Error PipePeek( /*[in]*/ const std::string &strChannel, /*[in]*/ int _nIndex,
					/*[out]*/ std::shared_ptr<MF_BASE_TYPE> &pBufferOrFrame, /*[in]*/ int _nMaxWaitMs,
					/*[in]*/ const std::string &strHints ) override {
		return Error::NotImplemented;
	}

	Error PipeMessagePut(
		/*[in]*/ const std::string &strChannel,
		/*[in]*/ const std::string &strEventName,
		/*[in]*/ const std::string &strEventParam,
		/*[in]*/ int _nMaxWaitMs ) override;

	Error PipeMessageGet(
		/*[in]*/ const std::string &strChannel,
		/*[out]*/ std::string *pStrEventName,
		/*[out]*/ std::string *pStrEventParam,
		/*[in]*/ int _nMaxWaitMs ) override;

	Error PipeFlush( /*[in]*/ const std::string &strChannel, /*[in]*/ eMFFlashFlags _eFlashFlags ) override;

	Error PipeClose() override;

protected:
	void OnNewMessage( const IMsgReceived::Ptr &msg );
	Record::Ptr CheckReceived( const std::string channel, ERecordType type );
	bool ByteToRecordType( byte msg_type, ERecordType &type );
};

}  // namespace comm
