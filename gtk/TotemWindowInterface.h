#pragma once

#include "BaconVideoWidget.h"

#include "TotemPluginFoo.h" // just for MenuPlaceHolderType
class ITotemWindow {

public:
    //in derived class ctor, we no need to call this interface's ctor, since no data field to init
    virtual ~ITotemWindow() = default;

    // Define any methods TotemWrapper needs to interact with (the underlying is call HdyApplication methods )
    virtual void someMethod() = 0; // pure virtual function, which means the method must be overridden by any class that derives from ITotemWindow

    virtual BvwRotation get_bvw_rotation() = 0;

    virtual void set_bvw_rotation(BvwRotation rotation) = 0;

    virtual Glib::RefPtr<Gio::Menu> get_plugin_menu_placeholder (MenuPlaceHolderType type) = 0;

    virtual void empty_plugin_menu_placeholder (MenuPlaceHolderType type) = 0;

    virtual void add_plugin_action_to_group(const Glib::RefPtr<Gio::Action>& action) = 0;

    virtual const char* get_current_fullpath() = 0;

    virtual GtkWindow* get_main_window_gobj() = 0;

    virtual bool set_bvw_rate(float rate) = 0;

    virtual gint64 get_bvw_stream_length() = 0;

    virtual void get_bvw_metadata(BvwMetadataType type, Glib::ValueBase& value) = 0;

};