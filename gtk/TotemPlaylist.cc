
// #include "Utils.h"


// #include "TotemPlaylist.h"
// #include <iostream>




// class TotemPlaylist::Impl
// {

// public:
//     Impl(TotemPlaylist& widget);
//     ~Impl();

//     TR_DISABLE_COPY_MOVE(Impl)

 

// private:
//     TotemPlaylist& widget_;

// 	Glib::RefPtr<Gio::Settings> settings_;

// };











// /*******************************************************CTOR DTOR*********************************************************/
// Glib::RefPtr<TotemPlaylist> TotemPlaylist::create()
// {
//     Glib::make_refptr_for_instance(new TotemPlaylist());
// }



// TotemPlaylist::TotemPlaylist(

// )
// :   

//     Gtk::Window(cast_item),
//     impl_(std::make_unique<Impl>(*this, builder))
// {

    
//     std::cou

// }



// TotemPlaylist::~TotemPlaylist() 
// {
//     std::cout << "TotemPlaylist::~TotemPlaylist() " << std::endl;
// }



// TotemPlaylist::Impl::Impl(TotemPlaylist& widget, Glib::RefPtr<Gtk::Builder> const& builder) 
// :   widget_(widget)
// {
    
//     std::cout << "TotemPlaylist::Impl::Impl ctor" << std::endl;

//     //create a GSettings instance to interact with the gschema.compiled, unrefing it using g_clear_object() in our destructor
//     settings_ = Gio::Settings::create(TOTEM_GSETTINGS_SCHEMA);
    
// }




// TotemPlaylist::Impl::~Impl()
// {
//     std::cout << "TotemPlaylist::Impl::~Impl() " << std::endl;

//     /// if (settings_ != nullptr)
//     /// {
//     ///     g_clear_object (&settings_);
//     /// }

//     // if(display_page_){
//     //     delete display_page_;
//     // }
//     // if(audio_page_){
//     //     delete audio_page_;
//     // }
//     // if(plugins_page_){
//     //     delete plugins_page_;
//     // }
// }

















