#ifndef TORRENT_PLATFORM_UTIL_HPP
#define TORRENT_PLATFORM_UTIL_HPP

#include <cstdint>

#include "libtorrent/aux_/export.hpp"
namespace libtorrent {

	int max_open_files();

	TORRENT_EXTRA_EXPORT void set_thread_name(char const* name);

}

#endif // TORRENT_PLATFORM_UTIL_HPP
