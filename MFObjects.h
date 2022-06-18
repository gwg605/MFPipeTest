#pragma once

#include "MFTypes.h"
#include "ChunkReaderWriter.h"

namespace comm {

enum class ObjectType : uint32_t {
	Base = 0,
	Frame = 1,
	Buffer = 2,
};

typedef struct MF_BASE_TYPE {
	using Ptr = std::shared_ptr<MF_BASE_TYPE>;

	virtual ~MF_BASE_TYPE() {}

	virtual ObjectType GetObjectType() const {
		return ObjectType::Base;
	}

	virtual bool Write( utils::ChunkWriter& writer ) const {
		return true;
	}

	virtual bool Load( utils::ChunkReader& reader ) {
		return true;
	}

	static MF_BASE_TYPE::Ptr CreateByObjectType( ObjectType ot );
} MF_BASE_TYPE;

typedef struct MF_FRAME : public MF_BASE_TYPE {
	typedef std::shared_ptr<MF_FRAME> TPtr;

	M_TIME time = {};
	M_AV_PROPS av_props = {};
	std::string str_user_props;
	std::vector<uint8_t> vec_video_data;
	std::vector<uint8_t> vec_audio_data;

	ObjectType GetObjectType() const override {
		return ObjectType::Frame;
	}

	bool Write( utils::ChunkWriter& writer ) const override {
		// TODO: implement all types & all values
		bool res = writer.Write( str_user_props );
		res &= writer.Write( vec_video_data );
		res &= writer.Write( vec_audio_data );
		return res;
	}

	bool Load( utils::ChunkReader& reader ) override {
		// TODO: implement all types & all values
		bool res = reader.Read( str_user_props );
		res &= reader.Read( vec_video_data );
		res &= reader.Read( vec_audio_data );
		return res;
	}

} MF_FRAME;

typedef enum eMFBufferFlags {
	eMFBF_Empty = 0,
	eMFBF_Buffer = 0x1,
	eMFBF_Packet = 0x2,
	eMFBF_Frame = 0x3,
	eMFBF_Stream = 0x4,
	eMFBF_SideData = 0x10,
	eMFBF_VideoData = 0x20,
	eMFBF_AudioData = 0x40,
} eMFBufferFlags;

typedef struct MF_BUFFER : public MF_BASE_TYPE {
	typedef std::shared_ptr<MF_BUFFER> TPtr;

	eMFBufferFlags flags;
	std::vector<uint8_t> data;

	ObjectType GetObjectType() const override {
		return ObjectType::Buffer;
	}

	bool Write( utils::ChunkWriter& writer ) const override {
		bool res = writer.Write( static_cast<uint32_t>( flags ) );
		res &= writer.Write( data );
		return res;
	}

	bool Load( utils::ChunkReader& reader ) override {
		uint32_t fl;
		bool res = reader.Read( fl );
		flags = static_cast<eMFBufferFlags>( fl );
		res &= reader.Read( data );
		return res;
	}
} MF_BUFFER;

}  // namespace comm
