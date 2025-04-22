/*
 * Copyright (C) 2022 Red Hat Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */


#include "Utils.h"
#include "TotemPlayerHeader.h"
#include <iostream>



class TotemPlayerHeader::Impl
{
    public:
        Impl(TotemPlayerHeader& widget);
        ~Impl();

        TR_DISABLE_COPY_MOVE(Impl)


        void set_title(const char  *title);
        Glib::ustring get_title() const;
    
        void set_fullscreen_mode(bool is_fullscreen);
        Gtk::MenuButton* get_player_button();


        Glib::Property<int>& property_player_menu_model();
        void set_player_menu_button_model(Glib::RefPtr<Gio::MenuModel>& model);
        
    private:
        template<typename T>
        T* get_template_child(char const* name) const;
        



    private:
        
        TotemPlayerHeader& widget_;
        
        // Gtk widgets loaded from template
        Gtk::Widget* header_bar_ = nullptr; //a HdyHeaderBar 
        Gtk::Button* fullscreen_button_ = nullptr;
        Gtk::Button* unfullscreen_button_ = nullptr;
        Gtk::MenuButton* player_menu_button_ = nullptr;
        
        
        // Glib::Property<int> property_player_menu_model_;

};










TotemPlayerHeaderExtraInit::TotemPlayerHeaderExtraInit()
    : ExtraClassInit(&TotemPlayerHeaderExtraInit::class_init, nullptr, &TotemPlayerHeaderExtraInit::instance_init)
{
}


void TotemPlayerHeaderExtraInit::class_init(void* klass, void* /*user_data*/)
{
    // printf("(TotemPlayerHeaderExtraInit::class_init)\n");

    auto* const widget_klass = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_klass, gtr_get_full_resource_path("totem-player-toolbar.ui").c_str());
    gtk_widget_class_bind_template_child_full(widget_klass, "header_bar", FALSE, 0);
    gtk_widget_class_bind_template_child_full(widget_klass, "fullscreen_button", FALSE, 0);
    gtk_widget_class_bind_template_child_full(widget_klass, "unfullscreen_button", FALSE, 0);
    gtk_widget_class_bind_template_child_full(widget_klass, "player_menu_button", FALSE, 0);
}


void TotemPlayerHeaderExtraInit::instance_init(GTypeInstance* instance, void* /*klass*/)
{
    // printf("(TotemPlayerHeaderExtraInit::instance_init)\n");

    gtk_widget_init_template(GTK_WIDGET(instance));
}






TotemPlayerHeader::TotemPlayerHeader()
    : Glib::ObjectBase(typeid(TotemPlayerHeader))
{
    // printf("TotemPlayerHeader::TotemPlayerHeader()\n");

}






TotemPlayerHeader::TotemPlayerHeader(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder
)
    : Glib::ObjectBase(typeid(TotemPlayerHeader))
    , Gtk::Bin(cast_item)
    , impl_(std::make_unique<Impl>(*this))
{
    // get menu written in totem.ui file rather than use property (dont know how to handling case when
    // specify property in .ui in gtkmm)
    Glib::RefPtr<Glib::Object> obj = builder->get_object("playermenu");
    
    if (obj) {
        Glib::RefPtr<Gio::MenuModel> model = Glib::RefPtr<Gio::MenuModel>::cast_static(obj);
        if(model){
            impl_->set_player_menu_button_model(model);
        } else {
            std::cout << "Failed to cast_dynamic on player-menu-model " << std::endl;
        }

    }
}





// Constructor that loads its own .ui file using its own Gtk::Builder
TotemPlayerHeader::Impl::Impl(TotemPlayerHeader& widget) 
    :
    widget_(widget),
    header_bar_ (get_template_child<Gtk::Widget>("header_bar")),
    fullscreen_button_ (get_template_child<Gtk::Button>("fullscreen_button")),
    unfullscreen_button_ (get_template_child<Gtk::Button>("unfullscreen_button")),
    player_menu_button_ (get_template_child<Gtk::MenuButton>("player_menu_button"))
    // property_player_menu_model_ (widget, "player-menu-model", 0)
{
    if (header_bar_ && fullscreen_button_ && unfullscreen_button_ && player_menu_button_) 
    {
        // property_player_menu_model_.get_proxy().signal_changed().connect(sigc::mem_fun(*this, &TotemPlayerHeader::Impl::set_player_menu_button_model));
        widget_.show_all_children();
    } 
    else 
    {
        std::cerr << "TotemPlayerHeader:Failed to load widgets from UI file!" << std::endl;
    }
}







TotemPlayerHeader::~TotemPlayerHeader() 
{
    std::cout << "TotemPlayerHeader::~TotemPlayerHeader() " << std::endl;
}





TotemPlayerHeader::Impl::~Impl()
{
}





template<typename T>
T* TotemPlayerHeader::Impl::get_template_child(char const* name) const
{

    auto full_type_name = std::string("gtkmm__CustomObject_");
    Glib::append_canonical_typename(full_type_name, typeid(TotemPlayerHeader).name());

    return Glib::wrap(G_TYPE_CHECK_INSTANCE_CAST(
        gtk_widget_get_template_child(GTK_WIDGET(widget_.gobj()), g_type_from_name(full_type_name.c_str()), name),
        T::get_base_type(),
        typename T::BaseObjectType));
}













void TotemPlayerHeader::set_title(const char  *title)
{
    impl_->set_title(title);
}

void TotemPlayerHeader::Impl::set_title(const char  *title)
{
    if (header_bar_) {
        GtkWidget* cwidget = header_bar_->gobj();
        // Cast GtkWidget* to HdyHeaderBar* and call the C function
        HdyHeaderBar* hdy_header_bar_obj = HDY_HEADER_BAR(cwidget);

        hdy_header_bar_set_title (hdy_header_bar_obj, title);
    }
}



Glib::ustring TotemPlayerHeader::get_title() const
{
    return impl_->get_title();
}

Glib::ustring TotemPlayerHeader::Impl::get_title() const
{
    if (header_bar_) {
        GtkWidget* cwidget = header_bar_->gobj();
        // Cast GtkWidget* to HdyHeaderBar* and call the C function
        HdyHeaderBar* hdy_header_bar_obj = HDY_HEADER_BAR(cwidget);
        return hdy_header_bar_get_title(hdy_header_bar_obj);

        
    }
    return "";
}







void TotemPlayerHeader::set_fullscreen_mode(bool is_fullscreen)
{
    impl_->set_fullscreen_mode(is_fullscreen);
}
void TotemPlayerHeader::Impl::set_fullscreen_mode(bool is_fullscreen)
{
    if (fullscreen_button_ && unfullscreen_button_) {
        fullscreen_button_->set_visible(!is_fullscreen);
        unfullscreen_button_->set_visible(is_fullscreen);

        if (header_bar_) {
            GtkWidget* cwidget = header_bar_->gobj();
            // Cast GtkWidget* to HdyHeaderBar* and call the C function
            HdyHeaderBar* hdy_header_bar_obj = HDY_HEADER_BAR(cwidget);
            hdy_header_bar_set_show_close_button(hdy_header_bar_obj, !is_fullscreen);
        }
    }
}







Gtk::MenuButton* TotemPlayerHeader::get_player_button()
{
    return impl_->get_player_button();
}
Gtk::MenuButton* TotemPlayerHeader::Impl::get_player_button()
{
    return player_menu_button_;
}








void TotemPlayerHeader::Impl::set_player_menu_button_model(Glib::RefPtr<Gio::MenuModel>& model)
{
    // printf ("TotemPlayerHeader::Impl::set_player_menu_button_model()\n");

    if (player_menu_button_)
    {
        player_menu_button_->set_menu_model(model);
    }
}









// Glib::PropertyProxy<int> TotemPlayerHeader::property_player_menu_model()
// {
//     return impl_->property_player_menu_model().get_proxy();
// }



// Glib::Property<int>& TotemPlayerHeader::Impl::property_player_menu_model() 
// { 
//     return property_player_menu_model_; 
// }





