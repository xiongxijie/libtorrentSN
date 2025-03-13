// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include "tr-macros.h"

#include <glibmm/refptr.h>
#include <gtkmm/builder.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/filechoosernative.h>
#include <gtkmm/window.h>

#include <memory>

#include "libtorrent/add_torrent_params.hpp"
using namespace lt;


class Session;



class TorrentUrlChooserDialog : public Gtk::Dialog
{
public:
    TorrentUrlChooserDialog(
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const& builder,
        Gtk::Window& parent,
        Glib::RefPtr<Session> const& core);
    ~TorrentUrlChooserDialog() override /*= default*/;

    TR_DISABLE_COPY_MOVE(TorrentUrlChooserDialog)

    static std::unique_ptr<TorrentUrlChooserDialog> create(Gtk::Window& parent, Glib::RefPtr<Session> const& core);

private:
    void onOpenURLResponse(int response, Gtk::Entry const& entry, Glib::RefPtr<Session> const& core);
};

class TorrentFileChooserDialog : public Gtk::FileChooserNative
{
public:
    ~TorrentFileChooserDialog() override = default;

    TR_DISABLE_COPY_MOVE(TorrentFileChooserDialog)

    static std::unique_ptr<TorrentFileChooserDialog> create(Gtk::Window& parent, Glib::RefPtr<Session> const& core);

protected:
    TorrentFileChooserDialog(Gtk::Window& parent, Glib::RefPtr<Session> const& core);

private:
    void onOpenDialogResponse(int response, Glib::RefPtr<Session> const& core);
};



class OptionsDialog : public Gtk::Dialog
{
public:
    OptionsDialog(
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const& builder,
        Gtk::Window& parent,
        Glib::RefPtr<Session> const& core,
        lt::add_torrent_params && atp,
        std::string dottor_filename,
        std::string download_dir);
        
    ~OptionsDialog() override;

    TR_DISABLE_COPY_MOVE(OptionsDialog)

    static std::unique_ptr<OptionsDialog> create(
        Gtk::Window& parent,
        Glib::RefPtr<Session> const& core,
        lt::add_torrent_params && atp,
        std::string dottor_filename,
        std::string download_dir
    );

private:
    class Impl;
    //pimpl : contain pointer to implementation
    std::unique_ptr<Impl> const impl_;
};
