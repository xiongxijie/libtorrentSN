/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */


#pragma once

#include "tr-macros.h" // for macro "TR_DISABLE_COPY_MOVE"

#include <gtkmm/window.h>
#include <gtkmm/dialog.h>

#include <memory>
#include <functional>

class TotemPlayer
{
public:
    TotemPlayer(Gtk::Window& parent, std::function<void()> on_close);
    ~TotemPlayer();

    static std::unique_ptr<TotemPlayer> create(Gtk::Window& parent, std::function<void()> on_close);
    void show();
private:
    class Impl;
    std::unique_ptr<Impl> const impl_;
};
