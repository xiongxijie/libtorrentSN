


#pragma once

#include "tr-macros.h"
#include "GtkCompat.h"

#include <gtkmm.h>
#include <giomm.h>//for Gio::Settings
#include <glibmm.h>


#include <handy.h>


#include "totem-plugins-engine.h"// for func totem_plugins_engine_get_default() and libpeas includes here

#include <memory>

// class TotemPrefsDialogExtraInit : public Glib::ExtraClassInit
// {
// public:
//     TotemPrefsDialogExtraInit();

// private:
//     static void class_init(void* klass, void* user_data);
//     static void instance_init(GTypeInstance* instance, void* klass);
// };








class TotemPrefsDialog : 
// public TotemPrefsDialogExtraInit,
public Gtk::Window //Actually a HdyPreferencesWindow
{

public:
  
    TotemPrefsDialog(
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const& builder);

    ~TotemPrefsDialog() override;
    
    TR_DISABLE_COPY_MOVE(TotemPrefsDialog)
    
    static TotemPrefsDialog* create();

    void showUs();

    void render_plugins_page(TotemPluginsEngine *engine);

    void load_other_pages(BaconVideoWidget& bvw);
private:

    class Impl;
    std::unique_ptr<Impl> const impl_;
};
