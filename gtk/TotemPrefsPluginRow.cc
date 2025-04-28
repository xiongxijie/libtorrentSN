

#include "Utils.h"

#include "TotemPrefsPluginRow.h"

#include <iostream>





class TotemPrefsPluginRow::Impl
{

public:
    Impl(TotemPrefsPluginRow& widget, Glib::RefPtr<Gtk::Builder> const& builder, TotemPluginsEngine *engine);
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)

    void set_title(const char* title);

    void set_subtitle(const char* subtitle);

    void update_plugin_details(PeasPluginInfo *pi);


private:
    void set_label_text (Gtk::Label *label, Glib::ustring text);

    void display_plugin_details();

    void on_plugin_switch_toggled();
private:
    TotemPrefsPluginRow& widget_;   

    Gtk::Switch *plugin_switch_ = nullptr;//will call func in libpeas
    
    Gtk::Label *copyright_ = nullptr; //get from libpeas 
    
    Gtk::Label *authors_ = nullptr; //get from libpeas 
    
    Gtk::Label *version_ = nullptr; //get from libpeas 
    
    Gtk::LinkButton *website_ = nullptr; //get from libpeas 
    
    TotemPluginsEngine *engine_ = nullptr;//must hold this field so we can enable/disable the plugins
    
	PeasPluginInfo     *plugin_info_ = nullptr;//must hold this field so we can enable/disable the plugins
    
    bool is_loaded_ = false;

    
};








/*******************************************************CTOR DTOR*********************************************************/
// Use raw-pointer prevent issues like premature destruction that can occur with smart pointers
TotemPrefsPluginRow* TotemPrefsPluginRow::create(Gtk::ListBox& ourparent, TotemPluginsEngine *engine)
{
    std::cout << "In TotemPrefsPluginRow* TotemPrefsPluginRow::create " << std::endl;
    //Instantiate from .ui file
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("totem-preferences-plugin-row.ui"));
    return gtr_get_widget_derived<TotemPrefsPluginRow>(builder, "plugins_row", ourparent, engine);
}



TotemPrefsPluginRow::TotemPrefsPluginRow(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::ListBox& ourparent,
    TotemPluginsEngine *engine
)
:   
// Glib::ObjectBase(typeid(TotemPrefsPluginRow)),
    Gtk::ListBoxRow(cast_item),
    impl_(std::make_unique<Impl>(*this, builder, engine))
{
    std::cout << "TotemPrefsPluginRow::TotemPrefsPluginRow ctor" << std::endl;

    // If not add us to `ourparent` in our ctor here but in ctor of PluginsPrefsPage, the run log 
    // shows that the destructor will be called just after we call TotemPrefsPluginRow and its Impl ctor
    // Since the TotemPrefsPluginRow is not a top-level widget, it will destructs by builder 
    // when builder goes out of scope and destructs
    ourparent.append(*this);

    //for test
    // set_title("ABC");
    // set_subtitle("hello");
}





TotemPrefsPluginRow::~TotemPrefsPluginRow() 
{
    std::cout << "TotemPrefsPluginRow::~TotemPrefsPluginRow() " << std::endl;
}



TotemPrefsPluginRow::Impl::Impl(TotemPrefsPluginRow& widget, Glib::RefPtr<Gtk::Builder> const& builder, TotemPluginsEngine *engine) 
:   widget_(widget),
    plugin_switch_(gtr_get_widget<Gtk::Switch>(builder, "plugin_switch")),
    copyright_(gtr_get_widget<Gtk::Label>(builder, "copyright")),
    authors_(gtr_get_widget<Gtk::Label>(builder, "authors")),
    version_(gtr_get_widget<Gtk::Label>(builder, "version")),
    website_(gtr_get_widget<Gtk::LinkButton>(builder, "website")),
    engine_(g_object_ref(engine))
{
    std::cout << "TotemPrefsPluginRow::Impl::Impl ctor " << std::endl;


}



TotemPrefsPluginRow::Impl::~Impl()
{
    std::cout << "TotemPrefsPluginRow::Impl::~Impl() " << std::endl;
    if(engine_)
        g_object_unref(engine_);
}


void TotemPrefsPluginRow::Impl::set_label_text (Gtk::Label *label, Glib::ustring text)
{
	if (!text.empty())
    {
		label->set_text(text);
    }
	else
    {
        label->hide(); // Hide the label if the text is empty
    }
}



void TotemPrefsPluginRow::Impl::display_plugin_details ()
{
	Glib::ustring authors;
	Glib::ustring  plugin_copyright;
	const char **plugin_authors;
	Glib::ustring plugin_version;
	const char  * plugin_website;

    if(plugin_info_ == nullptr)
    {
        return;
    }

    /*Copyright*/
	plugin_copyright = peas_plugin_info_get_copyright (plugin_info_);
	set_label_text (copyright_, plugin_copyright);


    /*Authors list*/
	plugin_authors = peas_plugin_info_get_authors (plugin_info_);
    if (plugin_authors != nullptr) {
        for (int i = 0; plugin_authors[i] != nullptr; ++i) {
            if (i != 0) {
                authors += ", ";
            }
            authors += plugin_authors[i];  // Make sure this isn't null either!
        }
    }
	set_label_text (copyright_, authors);   


    /*Version*/
    const char* version_cstr = peas_plugin_info_get_version(plugin_info_);
    plugin_version = version_cstr ? version_cstr : "";
	set_label_text (version_, plugin_version);

    /*Website*/
    const char* website_cstr = peas_plugin_info_get_website (plugin_info_);
	plugin_website = website_cstr ? website_cstr : "";
	if (plugin_website != NULL)
    {
        website_->set_uri (plugin_website);
    }
	else
    {
        website_->hide();
    }

    /*Loaded the switch */
	is_loaded_ = peas_plugin_info_is_loaded (plugin_info_);

    plugin_switch_->set_active(is_loaded_);
}







//load or unload will change loaded-plugins property of PeasEngine which bind to active-plugins in gschemas
void TotemPrefsPluginRow::Impl::on_plugin_switch_toggled()
{
    std::cout << " TotemPrefsPluginRow::Impl::on_plugin_switch_toggled " << std::endl;

    // bool ret = FALSE;
    if(engine_== nullptr || plugin_info_ == nullptr)
    {
    std::cout << " engine_== nullptr || plugin_info_ == nullptr " << std::endl;

        return; /*ret;*/
    }

    PeasEngine* pe_obj = PEAS_ENGINE (engine_);
    if(pe_obj == nullptr)
    {
    std::cout << " pe_obj == nullptr " << std::endl;

        return; /*ret;*/
    }

    // ret = TRUE;

    bool state = plugin_switch_->get_active();

    if(state)
    {
            std::cout << "on_plugin_switch_toggled, Plugin enabled " << std::endl;
		peas_engine_load_plugin (pe_obj, plugin_info_);

    }
    else
    {
            std::cout << "on_plugin_switch_toggled, Plugin disabled " << std::endl;

		peas_engine_unload_plugin (pe_obj, plugin_info_);
    }

    // return ret;
}




/*******************************************************PUBLIC METHODS*********************************************************/

void TotemPrefsPluginRow::set_title(const char* title)
{
    impl_->set_title(title);
}

void TotemPrefsPluginRow::Impl::set_title(const char* title)
{
    GtkListBoxRow* cwidget = widget_.gobj();
    if (cwidget)
    {
        // Cast GtkWidget* to HdyPreferencesRow* and call the C function
        HdyPreferencesRow* hdy_prefs_row_obj = HDY_PREFERENCES_ROW(cwidget);
        hdy_preferences_row_set_title (hdy_prefs_row_obj, title);

        // const char* get_title = hdy_preferences_row_get_title(hdy_prefs_row_obj);
        // printf ("(get title after set is %s) \n", get_title);
    }
}





void TotemPrefsPluginRow::set_subtitle(const char* subtitle)
{
    impl_->set_subtitle(subtitle);
}

void TotemPrefsPluginRow::Impl::set_subtitle(const char* subtitle)
{

    GtkListBoxRow* cwidget = widget_.gobj();
    if (cwidget)
    {
        // Cast GtkWidget* to HdyExpanderRow* and call the C function
        HdyExpanderRow* hdy_expander_row_obj = HDY_EXPANDER_ROW(cwidget);
        hdy_expander_row_set_subtitle (hdy_expander_row_obj, subtitle);

        // const char* get_title = hdy_expander_row_get_subtitle(cwidget);
        // printf ("(get subtitle after set is %s) \n", get_title);
    }
}




void TotemPrefsPluginRow::update_plugin_details(PeasPluginInfo *pi)
{
    impl_->update_plugin_details(pi);
}

void TotemPrefsPluginRow::Impl::update_plugin_details(PeasPluginInfo *pi)
{
    if(pi)
    {
        if(plugin_info_==nullptr)
            plugin_info_ = pi;
        display_plugin_details();
    }

    //Also connect signal callabck when plugin_switch_ toggled;
    if(plugin_switch_)
    {
        plugin_switch_->property_active().signal_changed().connect(sigc::mem_fun(*this, &TotemPrefsPluginRow::Impl::on_plugin_switch_toggled));
    }

}





