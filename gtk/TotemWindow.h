/*
 * SPDX-License-Identifier: GPL-3-or-later
 */

 #pragma once

 
#include "tr-macros.h"
#include "GtkCompat.h"


#include <gtkmm.h>
#include <giomm.h>
#include <glibmm.h>

#include <memory>
#include <iostream>
#include <cstdint>  // Required for std::int64_t

#include "Session.h"
#include "TotemWindowInterface.h" // for ITotemWindow interface

#include "TotemPluginFoo.h" // just for MenuPlaceHolderType

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;


typedef enum {
	TOTEM_CONTROLS_UNDEFINED,
	TOTEM_CONTROLS_VISIBLE,
	TOTEM_CONTROLS_FULLSCREEN
} ControlsVisibility;


#define SEEK_FORWARD_OFFSET 60
#define SEEK_BACKWARD_OFFSET -15

#define VOLUME_DOWN_OFFSET (-0.08)
#define VOLUME_UP_OFFSET (0.08)

#define VOLUME_DOWN_SHORT_OFFSET (-0.02)
#define VOLUME_UP_SHORT_OFFSET (0.02)


#define DEFAULT_WINDOW_W 650
#define DEFAULT_WINDOW_H 500


#define VOLUME_DOWN_OFFSET (-0.08)
#define VOLUME_UP_OFFSET (0.08)

#define POPUP_HIDING_TIMEOUT 2 /* seconds */
#define REWIND_OR_PREVIOUS 4000


//use interface so TotemWrapper interact with the interface, makes modularity possible (TotemWrapper lives in a lib instead of executable sources)
class MyHdyApplicationWindow : public Gtk::Window ,  public ITotemWindow
{
public:

    MyHdyApplicationWindow(BaseObjectType* cobject, 
                           Glib::RefPtr<Gtk::Builder> const& builder,
                           Gtk::Window& parent,
                           Glib::RefPtr<Session> const& core,
                           std::uint32_t uniq_id);

    ~MyHdyApplicationWindow() override;

    TR_DISABLE_COPY_MOVE(MyHdyApplicationWindow)

    static std::unique_ptr<MyHdyApplicationWindow> create(Gtk::Window& parent, 
                                                    Glib::RefPtr<Session> const& core, 
                                                    std::uint32_t uniq_id);

                       
                                                    

    void init_plugins_system();

    void on_closing_sync_display();
                                                    
    


    

    /*must implement ITotemWindow pure virtual methods*/
    void someMethod() override;
    BvwRotation get_bvw_rotation() override;
    void set_bvw_rotation(BvwRotation rotation) override;
    Glib::RefPtr<Gio::Menu> get_plugin_menu_placeholder (MenuPlaceHolderType type) override;
    void empty_plugin_menu_placeholder (MenuPlaceHolderType type) override;
    void add_plugin_action_to_group(const Glib::RefPtr<Gio::Action>& action) override;
    const char* get_current_fullpath() override;
    GtkWindow* get_main_window_gobj() override;
    bool set_bvw_rate(float rate) override;
    gint64 get_bvw_stream_length() override;
    void get_bvw_metadata(BvwMetadataType type, Glib::ValueBase& value) override;

    
    
private:
    class Impl;
    std::unique_ptr<Impl> const impl_;

};











