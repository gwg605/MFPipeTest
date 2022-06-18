#pragma once

#include <string>

// based on https://stackoverflow.com/questions/2616011/easy-way-to-parse-a-url-in-c-cross-platform

namespace comm {
namespace utils {

	struct Uri {
	public:
		std::string QueryString, Path, Protocol, Host, Port;

		static Uri Parse( const std::string &uri );
	};  // uri
}  // namespace utils
}  // namespace comm
