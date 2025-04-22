/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include <iostream>
#include <functional>


/*
When you include a C header file (totem.h) in a C++ file, C++ compilers apply name mangling, 
By wrapping #include "totem.h" in extern "C", you tell the C++ compiler to treat the included C code 
in a "C way," avoiding name mangling and ensuring that the linker can correctly resolve symbols.
*/
extern "C" {
// our needed header full path is /media/pal/E/GnomeApp/totem-gstbt/install/include/totem/1.0/totem.h 
#include "totem.h" //from totem-gstbt install dir
}

#include "TotemPlayer.h"
#include <glib.h>
#include <gtk/gtk.h>
#include "totem.h"
#include <glibmm/main.h>
#include <gtkmm/window.h>



class TotemPlayer::Impl
{
public:
    Impl(TotemPlayer& external, Gtk::Window& parent, std::function<void()> on_close);
    
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)
    
    void show();


private:
    TotemPlayer& wrapper_;
    /*Totem**/GtkWidget* totem_; // Pointer to the C-based Totem object
    std::function<void()> on_close_; // Callback to notify application when dialog is closed
};



TotemPlayer::TotemPlayer(Gtk::Window& parent, std::function<void()> on_close)
    : impl_(std::make_unique<Impl>(*this, parent, on_close))
{
}



TotemPlayer::~TotemPlayer() /*= default;*/
{
    std::cout << " TotemPlayer::~TotemPlayer() " << std::endl;
}



std::unique_ptr<TotemPlayer> TotemPlayer::create(Gtk::Window& parent, std::function<void()> on_close)
{
    return std::unique_ptr<TotemPlayer>(new TotemPlayer(parent, on_close));
}




TotemPlayer::Impl::Impl(TotemPlayer& external, Gtk::Window& parent, std::function<void()> on_close)
    : wrapper_(external), on_close_(std::move(on_close))
{
    // Create the C-based Totem object
    // totem_ = reinterpret_cast<Totem*>(g_object_new(TOTEM_TYPE_OBJECT, NULL));

    totem_ = GTK_WIDGET(gtk_dialog_new());


    // Increment the reference count of the Totem object since we own it now
    // g_object_ref(totem_);
    
    // Set transient for the parent window
    gtk_window_set_transient_for(GTK_WINDOW(totem_), GTK_WINDOW(parent.gobj()));


    gtk_window_set_title(GTK_WINDOW(totem_), "Test Dialog");
    gtk_window_set_default_size(GTK_WINDOW(totem_), 300, 200); 

    
    // Now prevent any automatic dialog creation from the parent
    // gtk_window_set_modal(GTK_WINDOW(totem_), TRUE);  // Ensure Totem is modal and blocks the main window
    // // Hide the parent window when Totem dialog opens, and show it again when Totem closes.
    // parent.hide();


    // Connect to the "delete-event" signal to handle the dialog being closed
    g_signal_connect(totem_, "delete-event", G_CALLBACK(+[](GtkWidget* widget, GdkEvent* event, gpointer user_data) -> gboolean {

        std::cout << "in delete-event of TotemPlayer " << std::endl;

        auto* impl = static_cast<TotemPlayer::Impl*>(user_data);
        // Call the on_close callback to notify the application
        if (impl->on_close_) {
            impl->on_close_();
        }


        // Returning `FALSE` allows the dialog to be destroyed
        return FALSE;
    }), this);  
}




TotemPlayer::Impl::~Impl()
{
    //C-side cleanup will unref totem
    //Already do unref totem in totem_object_exit() 
    // Also Unref the Totem object on our side
    // if (totem_ != NULL)
    // {
    //     g_object_unref(G_OBJECT(totem_));
    //     totem_ = nullptr;
    // }

    // Check if the dialog is still open
    if (totem_) {
        // Destroy the dialog (removes it from the GTK container)
        gtk_widget_destroy(totem_);
        totem_ = nullptr;  // Set the pointer to nullptr to avoid dangling references
    }


    std::cout << "TotemPlayer::Impl::~Impl() - Totem dialog closed via totem_object_exit()" << std::endl;

}






void TotemPlayer::show()
{
    impl_->show();
}

void TotemPlayer::Impl::show()
{
    gtk_widget_show_all (GTK_WIDGET(totem_));
}











