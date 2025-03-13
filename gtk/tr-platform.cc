// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <algorithm>
#include <array>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef __HAIKU__
#include <limits.h> /* PATH_MAX */
#endif

#ifdef _WIN32
#include <process.h> /* _beginthreadex(), _endthreadex() */
#include <windows.h>
#include <shlobj.h> /* SHGetKnownFolderPath(), FOLDERID_... */
#ifdef small // workaround name collision between libsmall and rpcndr.h
#undef small
#endif
#else
#include <pwd.h>
#include <unistd.h> /* getuid() */
#endif

#ifdef BUILD_MAC_CLIENT
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef __HAIKU__
#include <FindDirectory.h>
#endif

#include <fmt/core.h>


#include "tr-transmission.h"
#include "tr-file.h"
#include "tr-log.h"
#include "tr-platform.h"
#include "tr-strbuf.h"
#include "tr-utils.h"

using namespace std::literals;

namespace
{
#ifdef _WIN32
std::string win32_get_known_folder_ex(REFKNOWNFOLDERID folder_id, DWORD flags)
{
    if (PWSTR path = nullptr; SHGetKnownFolderPath(folder_id, flags | KF_FLAG_DONT_UNEXPAND, nullptr, &path) == S_OK)
    {
        auto ret = tr_win32_native_to_utf8(path);
        CoTaskMemFree(path);
        return ret;
    }

    return {};
}

auto win32_get_known_folder(REFKNOWNFOLDERID folder_id)
{
    return win32_get_known_folder_ex(folder_id, KF_FLAG_DONT_VERIFY);
}
#endif

std::string getHomeDir()
{
    if (auto dir = tr_env_get_string("HOME"sv); !std::empty(dir))
    {
        return dir;
    }

#ifdef _WIN32

    if (auto dir = win32_get_known_folder(FOLDERID_Profile); !std::empty(dir))
    {
        return dir;
    }

#else

    struct passwd pwent = {};
    struct passwd* pw = nullptr;
    auto buf = std::array<char, 4096>{};
    getpwuid_r(getuid(), &pwent, std::data(buf), std::size(buf), &pw);
    if (pw != nullptr)
    {
        return pw->pw_dir;
    }

#endif

    return {};
}

std::string xdgConfigHome()
{
    if (auto dir = tr_env_get_string("XDG_CONFIG_HOME"sv); !std::empty(dir))
    {
        return dir;
    }

    return fmt::format("{:s}/.config"sv, getHomeDir());
}

std::string getXdgEntryFromUserDirs(std::string_view key)
{
    auto content = std::vector<char>{};
    if (auto const filename = fmt::format("{:s}/{:s}"sv, xdgConfigHome(), "user-dirs.dirs"sv);
        !tr_sys_path_exists(filename) || !tr_file_read(filename, content) || std::empty(content))
    {
        return {};
    }

    // search for key="val" and extract val
    auto const search = fmt::format("{:s}=\"", key);
    auto begin = std::search(std::begin(content), std::end(content), std::begin(search), std::end(search));
    if (begin == std::end(content))
    {
        return {};
    }
    std::advance(begin, std::size(search));
    auto const end = std::find(begin, std::end(content), '"');
    if (end == std::end(content))
    {
        return {};
    }
    auto val = std::string{ begin, end };

    // if val contains "$HOME", replace that with getHomeDir()
    auto constexpr Home = "$HOME"sv;
    if (auto const it = std::search(std::begin(val), std::end(val), std::begin(Home), std::end(Home)); it != std::end(val))
    {
        val.replace(it, it + std::size(Home), getHomeDir());
    }

    return val;
}


} //anonymous namespace

// ---

std::string tr_getDefaultConfigDir(std::string_view appname)
{
    if (std::empty(appname))
    {
        appname = "Transmission"sv;
    }

    if (auto dir = tr_env_get_string("TRANSMISSION_HOME"sv); !std::empty(dir))
    {
        return dir;
    }

#ifdef __APPLE__

    return fmt::format("{:s}/Library/Application Support/{:s}"sv, getHomeDir(), appname);

#elif defined(_WIN32)

    auto const appdata = win32_get_known_folder(FOLDERID_LocalAppData);
    return fmt::format("{:s}/{:s}"sv, appdata, appname);

#elif defined(__HAIKU__)

    char buf[PATH_MAX];
    find_directory(B_USER_SETTINGS_DIRECTORY, -1, true, buf, sizeof(buf));
    return fmt::format("{:s}/{:s}"sv, buf, appname);

#else

    return fmt::format("{:s}/{:s}"sv, xdgConfigHome(), appname);

#endif
}

size_t tr_getDefaultConfigDirToBuf(char const* appname, char* buf, size_t buflen)
{
    return tr_strv_to_buf(tr_getDefaultConfigDir(appname != nullptr ? appname : ""), buf, buflen);
}



/* defaults to /home/pal/Downloads */
std::string tr_getDefaultDownloadDir()
{
    if (auto dir = getXdgEntryFromUserDirs("XDG_DOWNLOAD_DIR"sv); !std::empty(dir))
    {
        return dir;
    }

#ifdef _WIN32
    if (auto dir = win32_get_known_folder(FOLDERID_Downloads); !std::empty(dir))
    {
        return dir;
    }
#endif

#ifdef __HAIKU__
    return fmt::format("{:s}/Desktop"sv, getHomeDir());
#endif

    return fmt::format("{:s}/Downloads"sv, getHomeDir());
}



size_t tr_getDefaultDownloadDirToBuf(char* buf, size_t buflen)
{
    return tr_strv_to_buf(tr_getDefaultDownloadDir(), buf, buflen);
}

// ---

std::string tr_getSessionIdDir()
{
#ifndef _WIN32

    return std::string{ "/tmp"sv };

#else

    auto const program_data_dir = win32_get_known_folder_ex(FOLDERID_ProgramData, KF_FLAG_CREATE);
    auto result = fmt::format("{:s}/Transmission"sv, program_data_dir);
    tr_sys_dir_create(result, 0, 0);
    return result;

#endif
}
