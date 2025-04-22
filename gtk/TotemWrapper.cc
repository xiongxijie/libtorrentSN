/*
we ends with .cc so we are treated a c++, we can use c++ type and along with c
*/

#include "TotemWrapper.h"// for TotemWrapper type define and function signatures

#include <stdio.h>

enum {
	FILEIDX_OPENED,
	FILEIDX_CLOSED,
	FILE_HAS_PLAYED,
	METADATA_UPDATED,
	LAST_SIGNAL
};
static int totem_table_signals[LAST_SIGNAL] = { 0 };

enum {
    PROP_0,
    PROP_STREAM_LENGTH
};






struct _TotemWrapper {
    GObject parent_instance;      // GObject parent class
    ITotemWindow *gtkmm_instance; // The wrapped gtkmm instance (only access its ITotemWindow part)

};



// Define the GObject type for TotemWrapper
G_DEFINE_TYPE(TotemWrapper, totem_wrapper, G_TYPE_OBJECT)





static void
totem_wrapper_get_property (GObject * object, guint property_id,
                                 GValue * value, GParamSpec * pspec)
{
  TotemWrapper *self;
  self = TOTEM_WRAPPER (object);

  switch (property_id) {
    case PROP_STREAM_LENGTH:
        g_value_set_int64 (value,  totem_wrapper_get_stream_length(self));
        break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}







// Finalize function to free resources when the object is destroyed
static void 
totem_wrapper_finalize (GObject *object) 
{
    TotemWrapper *self = TOTEM_WRAPPER (object);

    // We do not own the gtkmm instance, so we just nullify it
    self->gtkmm_instance = NULL;

    // Call the parent class' finalize method to complete object destruction
    G_OBJECT_CLASS(totem_wrapper_parent_class)->finalize(object);
}



// Class initialization function (called once when the class is first referenced)
static void 
totem_wrapper_class_init (TotemWrapperClass *klass) 
{

    printf("totem_wrapper_class_init \n");

    // Set the finalize function to handle cleanup
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = totem_wrapper_finalize;


    //  called when a "READABLE" property of a GObject instance is retrieved or read
    gobject_class->get_property = totem_wrapper_get_property;



    /* We Proxy Signals */
    totem_table_signals[FILEIDX_OPENED] =
        g_signal_new ("fileidx-opened",
                G_TYPE_FROM_CLASS (gobject_class),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE, 1, G_TYPE_STRING);


    totem_table_signals[FILEIDX_CLOSED] =
    g_signal_new ("fileidx-closed",
                G_TYPE_FROM_CLASS (gobject_class),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0, G_TYPE_NONE);


    totem_table_signals[METADATA_UPDATED] =
    g_signal_new ("metadata-updated",
                G_TYPE_FROM_CLASS (gobject_class),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);



    /*We Proxy Properties*/
	// /**
	//  * TotemObject:playing:
	//  *
	//  * If %TRUE, Totem is playing an audio or video file.
	//  **/
	// g_object_class_install_property (gobject_class, PROP_PLAYING,
    //     g_param_spec_boolean ("playing", "Playing?", "Whether Totem is currently playing a file.",
    //                   FALSE, G_PARAM_READABLE));

    /**
    * TotemObject:stream-length:
    *
    * The length of the current stream, in milliseconds.
    **/
    g_object_class_install_property (gobject_class, PROP_STREAM_LENGTH,
            g_param_spec_int64 ("stream-length", "Stream length", "The length of the current stream.",
                        G_MININT64, G_MAXINT64, 0,
                        G_PARAM_READABLE));



}







// Instance initialization function (called each time an instance is created)
static void 
totem_wrapper_init (TotemWrapper *self) 
{
    // Initialize the wrapped gtkmm instance to NULL
    self->gtkmm_instance = NULL;    
}




// Function to create a new TotemWrapper instance
TotemWrapper *totem_wrapper_new (void) 
{
    // Create a new TotemWrapper instance using GObject
    TotemWrapper *obj = TOTEM_WRAPPER (g_object_new(TOTEM_TYPE_WRAPPER, NULL));

    // Ensure the object was created successfully
    if (!obj) {
        return NULL;
    }

    // Initialize the wrapped gtkmm instance to NULL
    obj->gtkmm_instance = NULL;

    return obj;
}




// Function to cast an existing gtkmm instance to a TotemWrapper object
/*
because of polymorphism. MyHdyApplicationWindow inherits from ITotemWindow, 
a pointer to MyHdyApplicationWindow can be implicitly converted to a pointer to ITotemWindow. 
This is known as "upcasting"
*/
TotemWrapper *totem_wrapper_from_gtkmm_instance (ITotemWindow *gtkmm_instance) 
{
    g_return_val_if_fail (gtkmm_instance!=nullptr, nullptr);

    // Ensure we have a valid gtkmm instance
    if (!gtkmm_instance) 
    {
        return NULL;
    }

    // Create a new TotemWrapper wrapper instance
    TotemWrapper *obj = totem_wrapper_new();
    
    if (!obj) 
    {
        return NULL;  // Handle allocation failure gracefully
    }

    // Set the existing gtkmm instance into the wrapper
    obj->gtkmm_instance = gtkmm_instance;

    // printf ("called totem_wrapper_from_gtkmm_instance \n");
    return obj;
}


void totem_wrapper_cleanup (TotemWrapper *wrapper)
{
    g_return_if_fail (wrapper!=NULL);

    g_clear_object(&wrapper);
}









//signal
void totem_fileidx_opened_signal_emit(TotemWrapper* self, const char* fpath)
{
    g_return_if_fail (TOTEM_IS_WRAPPER (self));

    g_signal_emit (G_OBJECT (self),
        totem_table_signals[FILEIDX_OPENED],
        0, fpath);
}


void totem_fileidx_closed_signal_emit(TotemWrapper* self)
{
    g_return_if_fail (TOTEM_IS_WRAPPER (self));

    g_signal_emit (G_OBJECT (self),
		       totem_table_signals[FILEIDX_CLOSED],
		       0);
}


void totem_metadata_updated_signal_emit(TotemWrapper* self, 
                                        const char* artist,
                                        const char* title,
                                        const char* album)
{
    g_return_if_fail (TOTEM_IS_WRAPPER (self));

	g_signal_emit (G_OBJECT (self),
		       totem_table_signals[METADATA_UPDATED],
		       0,
		       artist,
		       title,
		       album);

}




// void totem_prop_playing_notify (TotemWrapper* self)
// {
//     g_return_if_fail (TOTEM_IS_WRAPPER (self));

//     g_object_notify (G_OBJECT (self), "playing");
// }
void totem_prop_stream_length_notify(TotemWrapper* self)
{
    g_return_if_fail (TOTEM_IS_WRAPPER (self));

    g_object_notify (G_OBJECT (self), "stream-length");
}




BvwRotation totem_wrapper_get_rotation(TotemWrapper* self)
{
    g_return_val_if_fail (TOTEM_IS_WRAPPER (self), BvwRotation::BVW_ROTATION_R_ZERO);

    BvwRotation ret = BVW_ROTATION_R_ZERO;

    ITotemWindow* underlying = self->gtkmm_instance;

    if(underlying!=NULL)
    {
        ret = underlying->get_bvw_rotation();
    }

    return ret;
}
void totem_wrapper_set_rotation(TotemWrapper* self, BvwRotation rotation)
{
    g_return_if_fail (TOTEM_IS_WRAPPER (self));

    ITotemWindow* underlying = self->gtkmm_instance;
    if(underlying!=NULL)
    {
        underlying->set_bvw_rotation(rotation);
    }
}


Glib::RefPtr<Gio::Menu> totem_wrapper_get_menu_section (TotemWrapper* self, MenuPlaceHolderType type)
{
    g_return_val_if_fail (TOTEM_IS_WRAPPER (self), Glib::RefPtr<Gio::Menu>());

    Glib::RefPtr<Gio::Menu> ret;
    ITotemWindow* underlying = self->gtkmm_instance;


    if(underlying!=NULL)
    {
        ret = underlying->get_plugin_menu_placeholder(type);
    }

    if(ret)
    {
        printf ("(totem_wrapper_get_menu_section) OK\n");
        return ret;
    }else{
        printf ("(totem_wrapper_get_menu_section) EMPTY\n");

        return Glib::RefPtr<Gio::Menu>();
    }
}


void totem_wrapper_empty_menu_section (TotemWrapper* self, MenuPlaceHolderType type)
{
    g_return_if_fail (TOTEM_IS_WRAPPER (self));

    ITotemWindow* underlying = self->gtkmm_instance;
    if(underlying!=NULL)
    {
        underlying->empty_plugin_menu_placeholder(type);
    }
}


void totem_wrapper_add_plugin_action(TotemWrapper* self, const Glib::RefPtr<Gio::Action>& action)
{
    g_return_if_fail (TOTEM_IS_WRAPPER (self));

    ITotemWindow* underlying = self->gtkmm_instance;
    if(underlying!=NULL)
    {
        underlying->add_plugin_action_to_group(action);
    }
}



const char* totem_wrapper_get_current_full_path (TotemWrapper* self)
{
    g_return_val_if_fail (TOTEM_IS_WRAPPER (self), NULL);

    const char* ret= NULL;
    ITotemWindow* underlying = self->gtkmm_instance;
    if(underlying!=NULL)
    {
        ret = underlying->get_current_fullpath();
    }

    return ret;
}



GtkWindow* totem_wrapper_get_main_window(TotemWrapper* self)
{
    g_return_val_if_fail (TOTEM_IS_WRAPPER (self), NULL);

    GtkWindow* ret= NULL;
    ITotemWindow* underlying = self->gtkmm_instance;
    if(underlying!=NULL)
    {
        ret = underlying->get_main_window_gobj();
    }
 
    return ret;
}



bool totem_wrapper_set_rate(TotemWrapper* self, float rate)
{
    g_return_val_if_fail (TOTEM_IS_WRAPPER (self), FALSE);

    bool ret= false;
    ITotemWindow* underlying = self->gtkmm_instance;
    if(underlying!=NULL)
    {
        ret = underlying->set_bvw_rate(rate);
    }
 
     return ret;
}



gint64 totem_wrapper_get_stream_length (TotemWrapper* self)
{
    g_return_val_if_fail (TOTEM_IS_WRAPPER (self), 0);

     gint64 ret = 0;
     ITotemWindow* underlying = self->gtkmm_instance;
     if(underlying!=NULL)
     {
         ret = underlying->get_bvw_stream_length();
     }
 
     return ret;
}



void totem_wrapper_get_bvw_metadata(TotemWrapper* self ,BvwMetadataType type, Glib::ValueBase& value)
{
    g_return_if_fail (TOTEM_IS_WRAPPER (self));

    ITotemWindow* underlying = self->gtkmm_instance;
    if(underlying!=NULL)
    {
        underlying->get_bvw_metadata(type, value);
    }

}