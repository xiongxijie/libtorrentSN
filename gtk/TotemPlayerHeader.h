/*
 * Copyright (C) 2022 Red Hat Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */
#pragma once



#include "tr-macros.h"
#include "GtkCompat.h"

#include <glibmm.h>
#include <giomm.h>
#include <glibmm/refptr.h>
#include <glibmm/property.h>
#include <glibmm/propertyproxy.h>

#include <glibmm/extraclassinit.h>
#include <gtkmm.h>

#include <handy.h> // libhandy-1
 



class TotemPlayerHeaderExtraInit : public Glib::ExtraClassInit
{
public:
    TotemPlayerHeaderExtraInit();

private:
    static void class_init(void* klass, void* user_data);
    static void instance_init(GTypeInstance* instance, void* klass);
};





class TotemPlayerHeader : 
public TotemPlayerHeaderExtraInit
, public Gtk::Bin 
{

public:
    TotemPlayerHeader();
    TotemPlayerHeader(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder);
    ~TotemPlayerHeader() override;
    
    TR_DISABLE_COPY_MOVE(TotemPlayerHeader)

    void set_title(const char  *title);
    Glib::ustring get_title() const;

    void set_fullscreen_mode(bool is_fullscreen);
    Gtk::MenuButton* get_player_button();


    // Glib::PropertyProxy<int> property_player_menu_model();


protected:

        void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override
        {
            minimum_width = 600;
            natural_width = 600;
        }

        void get_preferred_height_vfunc(int& minimum_height, int& natural_height) const override
        {
            minimum_height = 600 / 16 * 9 / 4; // maybe header bar is thinner?
            natural_height = 600 / 16 * 9 / 4;
        }



private:

    class Impl;
    std::unique_ptr<Impl> const impl_;


};


