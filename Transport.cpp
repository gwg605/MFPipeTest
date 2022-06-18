#include "Transport.h"
#include "TransportUDP.h"
#include "URL.h"

namespace comm {

ITransport::Ptr TransportFactory::CreateTransport( const std::string& proto ) {
	utils::Uri uri = utils::Uri::Parse( proto );
	if( uri.Protocol == "udp" ) {
		return std::make_shared<transports::TransportUDP>();
	}

	return nullptr;
}

}  // namespace comm
