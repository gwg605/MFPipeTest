#include "MFObjects.h"

namespace comm {

MF_BASE_TYPE::Ptr MF_BASE_TYPE::CreateByObjectType( ObjectType ot ) {
	switch( ot ) {
	case ObjectType::Frame:
		return std::make_shared<MF_FRAME>();
	case ObjectType::Buffer:
		return std::make_shared<MF_BUFFER>();
	case ObjectType::Base:
		return std::make_shared<MF_BASE_TYPE>();
	default:
		break;
	}
	return nullptr;
}

}  // namespace comm
