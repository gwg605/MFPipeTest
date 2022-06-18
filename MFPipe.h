#pragma once

#include <string>
#include <memory>

#include "MFTypes.h"
#include "MFObjects.h"

namespace comm {

class MFPipe {
public:
	typedef struct MF_PIPE_INFO {
		int nPipeMode;
		int nPipesConnected;
		int nChannels;
		int nObjectsHave;
		int nObjectsMax;
		int nObjectsDropped;
		int nObjectsFlushed;
		int nMessagesHave;
		int nMessagesMax;
		int nMessagesDropped;
		int nMessagesFlushed;
	} MF_PIPE_INFO;

	typedef enum eMFFlashFlags {
		eMFFL_ResetCounters = 0x2,
		eMFFL_FlushObjects = 0x20,
		eMFFL_FlushMessages = 0x40,
		eMFFL_FlushStream = 0x20,
		eMFFL_FlushAll = 0xf0,
		eMFFL_RemoveChannel = 0x100
	} eMFFlashFlags;

	virtual ~MFPipe() {}

	virtual Error PipeInfoGet( /*[out]*/ std::string *pStrPipeName, /*[in]*/ const std::string &strChannel,
							   MF_PIPE_INFO *_pPipeInfo ) = 0;
	virtual Error PipeCreate( /*[in]*/ const std::string &strPipeID, /*[in]*/ const std::string &strHints ) = 0;
	virtual Error PipeOpen( /*[in]*/ const std::string &strPipeID, /*[in]*/ int _nMaxBuffers,
							/*[in]*/ const std::string &strHints ) = 0;
	virtual Error PipePut( /*[in]*/ const std::string &strChannel,
						   /*[in]*/ const std::shared_ptr<MF_BASE_TYPE> &pBufferOrFrame, /*[in]*/ int _nMaxWaitMs,
						   /*[in]*/ const std::string &strHints ) = 0;
	virtual Error PipeGet( /*[in]*/ const std::string &strChannel,
						   /*[out]*/ std::shared_ptr<MF_BASE_TYPE> &pBufferOrFrame, /*[in]*/ int _nMaxWaitMs,
						   /*[in]*/ const std::string &strHints ) = 0;
	virtual Error PipePeek( /*[in]*/ const std::string &strChannel, /*[in]*/ int _nIndex,
							/*[out]*/ std::shared_ptr<MF_BASE_TYPE> &pBufferOrFrame, /*[in]*/ int _nMaxWaitMs,
							/*[in]*/ const std::string &strHints ) = 0;
	virtual Error PipeMessagePut(
		/*[in]*/ const std::string &strChannel,
		/*[in]*/ const std::string &strEventName,
		/*[in]*/ const std::string &strEventParam,
		/*[in]*/ int _nMaxWaitMs ) = 0;
	virtual Error PipeMessageGet(
		/*[in]*/ const std::string &strChannel,
		/*[out]*/ std::string *pStrEventName,
		/*[out]*/ std::string *pStrEventParam,
		/*[in]*/ int _nMaxWaitMs ) = 0;
	virtual Error PipeFlush( /*[in]*/ const std::string &strChannel, /*[in]*/ eMFFlashFlags _eFlashFlags ) = 0;
	virtual Error PipeClose() = 0;
};

}  // namespace comm
