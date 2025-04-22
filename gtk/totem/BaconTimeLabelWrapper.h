/*
 * SPDX-License-Identifier: GPL-3-or-later
 */

#include <gtkmm.h>
#include <glibmm/refptr.h>
#include <gtkmm/builder.h>
#include <memory>

// extern "C" {
//     #include "bacon-time-label.h"
// }


class BaconTimeLabelWrapper : public Gtk::Bin 
{
    public:

        //constructor of type T in gtr_get_widget_derived(#builder,#name) 
        BaconTimeLabelWrapper(BaseObjectType* object, Glib::RefPtr<Gtk::Builder> const& builder)
        :  Glib::ObjectBase(typeid(BaconTimeLabelWrapper)) 
        {
        
        }   

}




