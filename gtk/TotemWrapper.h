#pragma once

/*TotemWrapper:
for c++ Totem(Actually MyHdyApplicationWindow class) interoperates with totem's plugins system
*/

//no need extra effort to wrap extern "c" around c lib header if meets c++ copmiler, cuz those official lib already done this for us
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h> //for those Gtk types

#include <glibmm.h>
#include <giomm.h>

//!! This header is for c++ wrapper, who will be included both by c++ file and c file
//!! We should explicitly specify linkage conventions in the header files
//!! this header contains both have C++(MyHdyApplicaiton class) and C (GObject)







// Define the GObject type for TotemWrapper   
#define TOTEM_TYPE_WRAPPER (totem_wrapper_get_type())
G_DECLARE_FINAL_TYPE(TotemWrapper, totem_wrapper, TOTEM, WRAPPER, GObject)







#include "TotemWindowInterface.h"
#include "BaconVideoWidget.h"


#include "TotemPluginFoo.h" // just for MenuPlaceHolderType





//if this header included by a cpp source file, we should add extern "c" , avoid such undefined reference errors when linking due to c++ name mangling
//when included by c code file, dont let them know below signatures, as they don't know ITotenWindow class type at all
#ifdef __cplusplus

extern "C" 
{

// Function to create a new TotemWrapper instance used by totem_wrapper_from_gtkmm_instance()
TotemWrapper *totem_wrapper_new (void);

// Function to associate an existing gtkmm instance with the wrapper, TotemWrapper interact with MyHdyApplicationWindow through ITotemWindow
// the consumer of TotemWrapper (aka, those plugins,plugin engine) just need to query TotemWrapper
TotemWrapper *totem_wrapper_from_gtkmm_instance (ITotemWindow *gtkmm_instance);

void totem_wrapper_cleanup (TotemWrapper *wrapper);

}
#endif









/*******************Exported functions that C side (mainly those plugins or plugin-system) can use **********************/
//TotemWrapper.cc is in c++ environment, so add extern "c"
#ifdef __cplusplus
extern "C" {
#endif





//Plugin used functions
//used by totem-movie-properties plugin, set trasient parent for its HdyWindow
// GtkWindow* totem_object_get_main_window (TotemWrapper* self, );

const char* totem_wrapper_get_current_full_path (TotemWrapper* self);

//used by properties plugin
gboolean totem_wrapper_is_playing (TotemWrapper* self);


bool totem_wrapper_set_rate(TotemWrapper* self, float rate);

Glib::RefPtr<Gio::Menu> totem_wrapper_get_menu_section (TotemWrapper* self, MenuPlaceHolderType type);
void totem_wrapper_empty_menu_section (TotemWrapper* self, MenuPlaceHolderType type);


void totem_wrapper_add_plugin_action(TotemWrapper* self, const Glib::RefPtr<Gio::Action>& action);


GtkWindow* totem_wrapper_get_main_window(TotemWrapper* self);

//proxying BaconVideoWidget c++ class 
BvwRotation totem_wrapper_get_rotation(TotemWrapper* self);
void totem_wrapper_set_rotation(TotemWrapper* self, BvwRotation rotation);





void totem_wrapper_get_bvw_metadata(TotemWrapper* self, BvwMetadataType type, Glib::ValueBase& value);


//proxying signals from TotemWindow.cc 
void totem_fileidx_opened_signal_emit (TotemWrapper* self, const char* fpath);

void totem_fileidx_closed_signal_emit (TotemWrapper* self);

void totem_metadata_updated_signal_emit (TotemWrapper* self, 
                                        const char* artist,
                                        const char* title,
                                        const char* album);

//proxying property changed signal (notify)
// void totem_prop_playing_notify (TotemWrapper* self);

void totem_prop_stream_length_notify (TotemWrapper* self);

gint64 totem_wrapper_get_stream_length (TotemWrapper* self);



#ifdef __cplusplus
}
#endif





