// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include "tr-transmission.h"
#include "tr-macros.h"



#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/builder.h>
#include <gtkmm/scrolledwindow.h>

#include <memory>



#include "libtorrent/info_hash.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/torrent_handle.hpp"
using namespace lt;


class Session;

class FileList : public Gtk::ScrolledWindow
{
public:
    //For DetailsDialog 
    FileList
    (
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const& builder,
        Glib::ustring const& view_name,/*name of that widget defined in .ui-builder file*/
        Glib::RefPtr<Session> const& core,
        lt::torrent_handle const& handle_ref
    );

    //For OptionsDialog
    FileList
    (
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const& builder,
        Glib::ustring const& view_name,/*name of that widget defined in .ui-builder file*/
        Glib::RefPtr<Session> const& core,
        lt::add_torrent_params & atp_ref
    );

    ~FileList() override;

    TR_DISABLE_COPY_MOVE(FileList)


    void set_torrent();

    void set_torrent_when_add(lt::add_torrent_params const& new_atp);

    void clear();

    //empty, just ignore
    static lt::torrent_handle empty_handle_;

    static lt::add_torrent_params empty_atp_;



private:
    class Impl;
    std::unique_ptr<Impl> const impl_;
    
};


