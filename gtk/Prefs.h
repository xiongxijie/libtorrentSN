// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#pragma once

#include "tr-transmission.h" 
#include "Session.h"

#include <cstdint> // int64_t
#include <string>
#include <string_view>
#include <vector>


#include "libtorrent/settings_pack.hpp"/* lt::settings_pack */
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/info_hash.hpp"


using namespace lt;

/*  

    Prefs.h -- using libtorrent::settings_pack to represent each setting items  

*/

lt::session_params tr_sessionLoadSettings();

void tr_sessionSaveSettings(lt::session_params const& sp);

std::vector<lt::add_torrent_params> tr_torrentLoadResume();

void tr_torrentSaveResume(lt::add_torrent_params& params);

void gtr_pref_init(std::string_view config_dir);


extern size_t const max_recent_dirs;
std::list<std::string> gtr_get_recent_dirs(lt::settings_pack sett, char key);
void gtr_save_recent_dir(char key, Glib::RefPtr<Session> const& core, std::string const& dir);


std::string FetchTorResumePath(lt::info_hash_t const& ih);



