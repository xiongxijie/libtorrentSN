// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#pragma once

#include "tr-transmission.h"

#include <glibmm/refptr.h>
#include <gtkmm/window.h>

#include <vector>


#include "libtorrent/torrent_status.hpp"
using namespace lt;

class Session;

/**
 * Prompt the user to confirm removing a torrent.
 */
void gtr_confirm_remove(
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core,
    std::vector<lt::torrent_status> const& st,
    bool delete_files
    );
