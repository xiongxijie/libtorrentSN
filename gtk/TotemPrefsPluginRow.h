

#pragma once



#include "tr-macros.h"
#include "GtkCompat.h"

#include <gtkmm.h>
#include <giomm.h>
#include <glibmm.h>
#include <glibmm/refptr.h>
#include <glibmm/property.h>
#include <glibmm/propertyproxy.h>


//!!we can still use c lib header without wrap it into the extern "c" , because these Official libs has done this already for us 
//!!such as it will use G_BEGIN_DECLS in those .h headers, it is to adds extern "C" around the header when the compiler in use is a C++ compiler
#include <handy.h>
#include <libpeas/peas-plugin-info.h>
#include <libpeas/peas-engine.h>

#include <memory>



#include "totem-plugins-engine.h"




//Only for non-builtin plugin (here it is variable-rate plugin)
class TotemPrefsPluginRow : public Gtk::ListBoxRow //Actually a HdyExpanderRow who derives from HdyPreferencesRow who derives from GtkListBoxRow
{

public:
  
    TotemPrefsPluginRow(
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const&builder,
        Gtk::ListBox& ourparent,
        TotemPluginsEngine *engine);

    ~TotemPrefsPluginRow() override;
    
    TR_DISABLE_COPY_MOVE(TotemPrefsPluginRow)
    
    static TotemPrefsPluginRow* create(Gtk::ListBox& ourparent, TotemPluginsEngine *engine);

    void set_title(const char* title);
    
    void set_subtitle(const char* subtitle);

    void update_plugin_details(PeasPluginInfo *pi);

private:
    class Impl;
    std::unique_ptr<Impl> const impl_;
};



