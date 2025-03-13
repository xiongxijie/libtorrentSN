// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include "tr-transmission.h"
#include "tr-macros.h"


#include <glibmm/refptr.h>
#include <gtkmm/builder.h>
#include <gtkmm/dialog.h>
#include <gtkmm/window.h>

#include <memory>
#include <vector>


#include "libtorrent/torrent_handle.hpp"
using namespace lt;

class Session;

class DetailsDialog : public Gtk::Dialog
{
    public:
        DetailsDialog(
            BaseObjectType* cast_item,
            Glib::RefPtr<Gtk::Builder> const& builder,
            Gtk::Window& parent,
            Glib::RefPtr<Session> const& core,
            lt::torrent_handle && handle_rvalue
            );

    ~DetailsDialog() override;

    TR_DISABLE_COPY_MOVE(DetailsDialog)

    static std::unique_ptr<DetailsDialog> create(Gtk::Window& parent, Glib::RefPtr<Session> const& core, lt::torrent_handle && handle_rvalue);

    
    void refresh();

    private:
        class Impl;
        std::unique_ptr<Impl> const impl_;

};

