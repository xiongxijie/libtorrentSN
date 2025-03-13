// This file Copyright © Transmission authors and contributors.
// It may be used under the 3-Clause BSD (SPDX: BSD-3-Clause),
// GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

// This file defines the public API for the libtransmission library.

#pragma once

// --- Basic Types

#include <stddef.h> // size_t
#include <stdint.h> // uintN_t
#include <time.h> // time_t
#include <functional> //std::function
#ifdef __cplusplus
#include <string>
#include <string_view>
#else
#include <stdbool.h> // bool
#endif

#include "tr-macros.h"
#include "tr-error.h"
#include "tr-error-types.h"
#include "libtorrent/error_code.hpp"
using namespace lt;



struct tr_error;



// --- Startup & Shutdown



#ifdef __cplusplus
[[nodiscard]] std::string tr_getDefaultConfigDir(std::string_view appname);
#endif



size_t tr_getDefaultConfigDirToBuf(char const* appname, char* buf, size_t buflen);



#ifdef __cplusplus
[[nodiscard]] std::string tr_getDefaultDownloadDir();
#endif



size_t tr_getDefaultDownloadDirToBuf(char* buf, size_t buflen);
















using rename_done_func = std::function<void(void*, lt::error_code)>;




//state (activity) of a torrent 
enum tr_torrent_activity
{
    TR_STATUS_PAUSED = 0,

    TR_STATUS_CHECKING_FILES = 1, 
    TR_STATUS_DOWNLOAD_METADATA = 2, 
    TR_STATUS_DOWNLOAD = 3, 
    TR_STATUS_FINISHED = 4, 
    TR_STATUS_SEED = 5, 
    TR_STATUS_UPLOAD_MODE = 6,
    TR_STATUS_CHECKING_RESUME_DATA = 7,

    TR_STATUS_QUEUE_FOR_CHECKING_FILES =11,
    TR_STATUS_QUEUE_FOR_DOWNLOAD_METADATA =12,
    TR_STATUS_QUEUE_FOR_DOWNLOAD =13,
    TR_STATUS_QUEUE_FOR_SEED =15,
    TR_STATUS_QUEUE_FOR_CHECKING_RESUME_DATA =17,

    TR_STATUS_NULL=18
};


   
