/*
 * SPDX-License-Identifier: GPL-3-or-later
 */

 #include <gtkmm.h>
 #include <glibmm/refptr.h>
#include <gtkmm/builder.h>
 #include <memory>
 
extern "C" {
    #include "totem-playlist.h"
}

 class TotemPlaylistWrapper : public Gtk::Bin 
 {
     public:
 
         //constructor of type T in gtr_get_widget_derived(#builder,#name) 
         TotemPlaylistWrapper(BaseObjectType* object, Glib::RefPtr<Gtk::Builder> const& builder)
         :  Glib::ObjectBase(typeid(TotemPlaylistWrapper)) 
         {
         
         }   
 
 }
 
 
 
 
 