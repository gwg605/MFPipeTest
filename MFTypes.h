#pragma once

#include <string>
#include <vector>

namespace comm {

using byte = unsigned char;

enum class Error : uint32_t {
	Ok,               // Successful
	Fatal,            // Unexpected thig
	InvalidSettings,  // user input settings are incorrrect
	NotImplemented,   // not implemented
	SentError,        // sent data error
	Timeout,          // Timeout
};

typedef long long int REFERENCE_TIME;

typedef struct M_TIME {
	REFERENCE_TIME rtStartTime;
	REFERENCE_TIME rtEndTime;
} M_TIME;

typedef enum eMFCC {
	eMFCC_Default = 0,
	eMFCC_I420 = 0x30323449,
	eMFCC_YV12 = 0x32315659,
	eMFCC_NV12 = 0x3231564e,
	eMFCC_YUY2 = 0x32595559,
	eMFCC_YVYU = 0x55595659,
	eMFCC_UYVY = 0x59565955,
	eMFCC_RGB24 = 0xe436eb7d,
	eMFCC_RGB32 = 0xe436eb7e,
} eMFCC;

typedef struct M_VID_PROPS {
	eMFCC fccType;
	int nWidth;
	int nHeight;
	int nRowBytes;
	short nAspectX;
	short nAspectY;
	double dblRate;
} M_VID_PROPS;

typedef struct M_AUD_PROPS {
	int nChannels;
	int nSamplesPerSec;
	int nBitsPerSample;
	int nTrackSplitBits;
} M_AUD_PROPS;

typedef struct M_AV_PROPS {
	M_VID_PROPS vidProps;
	M_AUD_PROPS audProps;
} M_AV_PROPS;

}  // namespace comm
