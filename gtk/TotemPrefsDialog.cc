

#include "Utils.h"

#include "TotemPrefsPluginRow.h" //libpeas includes here
#include "TotemPrefsDialog.h"

#include "BaconVideoWidget.h"


#ifdef __cplusplus
extern "C" {
#endif
#include "bacon-video-widget-enums.h" //for bvw_audio_output_type_get_type()
#ifdef __cplusplus
}
#endif

#include <iostream>
#include <optional>




namespace { //anonymous namespace begin

    using namespace Gio;


//Actually holding HdyPreferencesPage (although it is in 3rd-party lib (libhandy-1), it still derives from GtkBin)
class HdyPrefsPageBase : public Gtk::Bin
{

public:
    HdyPrefsPageBase(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder);
    ~HdyPrefsPageBase() override;

    TR_DISABLE_COPY_MOVE(HdyPrefsPageBase)



    //for those native gtk widgets
    template<typename T>
    T* get_widget(Glib::ustring const& name) const
    {
        return gtr_get_widget<T>(builder_, name);
    }

    //for those from customize widgets
    template<typename T, typename... ArgTs>
    T* get_widget_derived(Glib::ustring const& name, ArgTs&&... args) const
    {
        return gtr_get_widget_derived<T>(builder_, name, std::forward<ArgTs>(args)...);
    }

    //for non-widget object in .ui file, such as GtkAdjustment
    template<typename T>
    Glib::RefPtr<T> get_object(Glib::ustring const& name) const
    {
        Glib::RefPtr<Glib::Object> base_obj = builder_->get_object(name);
        if(!base_obj)
        {
            return {};
            //return an empty Glib::RefPtr<T>
        }

        //cast to detailed type (such as Gtk::Adjustment)
        Glib::RefPtr<T> derived_obj = Glib::RefPtr<T>::cast_dynamic(base_obj);
        if (!derived_obj) {
            return {};
            //return an empty Glib::RefPtr<T>
        }
    
        return derived_obj;
    }


private:
    Glib::RefPtr<Gtk::Builder> const& builder_;
};


HdyPrefsPageBase::HdyPrefsPageBase(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder)
    : Gtk::Bin(cast_item)
    , builder_(builder)
{

}

HdyPrefsPageBase::~HdyPrefsPageBase()
{
                    std::cout << "HdyPrefsPageBase ~HdyPrefsPageBase()" << std::endl;
}

    




/*for g_settings_bind_with_mapping*/
static gboolean
int_enum_get_mapping (GValue *value, GVariant *variant, GEnumClass *enum_class)
{
	GEnumValue *enum_value;
	const gchar *nick;

	g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), FALSE);

	nick = g_variant_get_string (variant, NULL);
	enum_value = g_enum_get_value_by_nick (enum_class, nick);

	if (enum_value == NULL)
		return FALSE;

	g_value_set_int (value, enum_value->value);

	return TRUE;
}

static GVariant *
int_enum_set_mapping (const GValue *value, const GVariantType *expected_type, GEnumClass *enum_class)
{
	GEnumValue *enum_value;

	g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);

	enum_value = g_enum_get_value (enum_class, g_value_get_int (value));

	if (enum_value == NULL)
		return NULL;

	return g_variant_new_string (enum_value->value_nick);
}



/****Audio Page ****/
class AudioPrefsPage : public HdyPrefsPageBase
{
public:
    AudioPrefsPage(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Gio::Settings>& settings);
    
    ~AudioPrefsPage() override;

    TR_DISABLE_COPY_MOVE(AudioPrefsPage)

    void load_audio_page_state(BaconVideoWidget& bvw);


// private:

//     std::optional<Glib::ustring> map_from_int_to_ustring(const unsigned int& setting_value);

//     std::optional<unsigned int> map_from_ustring_to_int(const Glib::ustring& property_value); 

private:

    Glib::RefPtr<Gio::Settings>& settings_;

    Gtk::ComboBox *tpw_sound_output_combobox_ = nullptr;

};


AudioPrefsPage::AudioPrefsPage(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Gio::Settings>& settings)
    : HdyPrefsPageBase(cast_item, builder),
    settings_(settings),
    tpw_sound_output_combobox_(get_widget<Gtk::ComboBox>("tpw_sound_output_combobox"))
{
    std::cout << "AudioPrefsPage::AudioPrefsPage ctor " << std::endl;

}


AudioPrefsPage::~AudioPrefsPage()
{
        std::cout << "AudioPrefsPage  ~AudioPrefsPage() " << std::endl;
}


// std::optional<Glib::ustring> AudioPrefsPage::map_from_int_to_ustring(const unsigned int& setting_value) 
// {

//     GType enum_type = bvw_audio_output_type_get_type();
//     if (!G_TYPE_IS_ENUM(enum_type)) {
//         return std::nullopt;
//     }
//     GEnumClass* enum_class = static_cast<GEnumClass*>(g_type_class_ref(enum_type));
//     if (!enum_class) {
//         return std::nullopt;
//     }

//     const GEnumValue* enum_value = g_enum_get_value(enum_class, static_cast<int>(setting_value));
//     std::optional<Glib::ustring> result = std::nullopt;
//     if (enum_value && enum_value->value_nick) {
//         result = Glib::ustring(enum_value->value_nick);
//     }

//     g_type_class_unref(enum_class);
//     return result;
// }


// std::optional<unsigned int> AudioPrefsPage::map_from_ustring_to_int(const Glib::ustring& property_value) 
// {
//     GType enum_type =bvw_audio_output_type_get_type();
//     if (!G_TYPE_IS_ENUM(enum_type)) {
//         return std::nullopt;
//     }
//     GEnumClass* enum_class = static_cast<GEnumClass*>(g_type_class_ref(enum_type));
//     if (!enum_class) {
//         return std::nullopt;
//     }

//     const GEnumValue* enum_value = g_enum_get_value_by_nick(enum_class, property_value.c_str());
//     std::optional<unsigned int> result = std::nullopt;
//     if (enum_value) {
//         result = static_cast<unsigned int>(enum_value->value);
//     }

//     g_type_class_unref(enum_class);
//     return result;
// }


void AudioPrefsPage::load_audio_page_state(BaconVideoWidget& bvw)
{
    settings_->bind("audio-output-type",
        bvw.property_audio_output_type(), SettingsBindFlags::SETTINGS_BIND_DEFAULT | SettingsBindFlags::SETTINGS_BIND_NO_SENSITIVITY);

    g_settings_bind_with_mapping (settings_->gobj(), "audio-output-type",
    tpw_sound_output_combobox_->gobj(), "active", G_SETTINGS_BIND_DEFAULT,
                    (GSettingsBindGetMapping) int_enum_get_mapping, (GSettingsBindSetMapping) int_enum_set_mapping,
                    g_type_class_ref (BVW_TYPE_AUDIO_OUTPUT_TYPE), (GDestroyNotify) g_type_class_unref);

}








/****Display Page ****/
class DisplayPrefsPage : public HdyPrefsPageBase
{

public:
    DisplayPrefsPage(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Gio::Settings>& settings/*avoid temporary copy*/);
    
    ~DisplayPrefsPage() override;

    TR_DISABLE_COPY_MOVE(DisplayPrefsPage)


    void load_display_page_state(BaconVideoWidget& bvw);


private:
    void tpw_color_reset_clicked_cb();

private:

    Glib::RefPtr<Gio::Settings>& settings_;//just refs, the owner is TotemPrefsDialog::Impl 

    Gtk::Bin *tpw_bright_contr_vbox_ = nullptr;//acutally a HdyPreferencesGroup
    Gtk::Switch *tpw_no_hardware_acceleration_ = nullptr;//a GtkSwitch
    Gtk::Range *tpw_bright_scale_ = nullptr;
    Gtk::Range *tpw_contrast_scale_ = nullptr;
    Gtk::Range *tpw_saturation_scale_ = nullptr;
    Gtk::Range *tpw_hue_scale_ = nullptr;
    Gtk::Button *tpw_color_reset_ = nullptr;

    // Gtk::Adjustment *tpw_bright_adjustment_= nullptr;
    // Gtk::Adjustment *tpw_contrast_adjustment_= nullptr;
    // Gtk::Adjustment *tpw_saturation_adjustment_= nullptr;
    // Gtk::Adjustment *tpw_hue_adjustment_= nullptr;

    Glib::RefPtr<Gtk::Adjustment> tpw_bright_adjustment_;
    Glib::RefPtr<Gtk::Adjustment> tpw_contrast_adjustment_;
    Glib::RefPtr<Gtk::Adjustment> tpw_saturation_adjustment_;
    Glib::RefPtr<Gtk::Adjustment> tpw_hue_adjustment_;


};


DisplayPrefsPage::DisplayPrefsPage(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Gio::Settings>& settings/*avoid temporary copy*/)
    : HdyPrefsPageBase(cast_item, builder),
    settings_(settings),
    tpw_bright_contr_vbox_(get_widget<Gtk::Bin>("tpw_bright_contr_vbox")),
    tpw_no_hardware_acceleration_(get_widget<Gtk::Switch>("tpw_no_hardware_acceleration")),

    tpw_bright_scale_(get_widget<Gtk::Scale>("tpw_bright_scale")),
    tpw_contrast_scale_(get_widget<Gtk::Scale>("tpw_contrast_scale")),
    tpw_saturation_scale_(get_widget<Gtk::Scale>("tpw_saturation_scale")),
    tpw_hue_scale_(get_widget<Gtk::Scale>("tpw_hue_scale")),

    tpw_color_reset_(get_widget<Gtk::Button>("tpw_color_reset")),

    tpw_bright_adjustment_(get_object<Gtk::Adjustment>("tpw_bright_adjustment")),
    tpw_contrast_adjustment_(get_object<Gtk::Adjustment>("tpw_contrast_adjustment")),
    tpw_saturation_adjustment_(get_object<Gtk::Adjustment>("tpw_saturation_adjustment")),
    tpw_hue_adjustment_(get_object<Gtk::Adjustment>("tpw_hue_adjustment"))
{
    std::cout << "DisplayPrefsPage::DisplayPrefsPage ctor " << std::endl;

    // tpw_bright_adjustment_ =  tpw_bright_scale_->get_adjustment();
    // tpw_contrast_adjustment_ =  tpw_contrast_scale_->get_adjustment();
    // tpw_saturation_adjustment_ =  tpw_saturation_scale_->get_adjustment();
    // tpw_hue_adjustment_ =  tpw_hue_scale_->get_adjustment();


    if(!tpw_bright_adjustment_ || !tpw_contrast_adjustment_ || !tpw_saturation_adjustment_ || !tpw_hue_adjustment_)
    {
        std::cerr << "DisplayPrefsPage, get obj adjustment failed" <<std::endl;
    }

    //Don't forget the reset button
    tpw_color_reset_->signal_clicked().connect(sigc::mem_fun(*this, &DisplayPrefsPage::tpw_color_reset_clicked_cb));

}


void DisplayPrefsPage::tpw_color_reset_clicked_cb()
{
    tpw_bright_scale_->set_value (65535/2);
    tpw_contrast_scale_->set_value (65535/2);
    tpw_hue_scale_->set_value (65535/2);
    tpw_saturation_scale_->set_value (65535/2);
}


DisplayPrefsPage::~DisplayPrefsPage()
{
        std::cout << "DisplayPrefsPage  ~DisplayPrefsPage() " << std::endl;
}




void DisplayPrefsPage::load_display_page_state(BaconVideoWidget& bvw)
{
    /*4 color balance (contrast, saturation, bright, hue)*/
    

    /*contrast*/
    // g_settings_bind (settings_->gobj(), "contrast", tpw_contrast_adjustment_->gobj(), "value", G_SETTINGS_BIND_DEFAULT);
    settings_->bind("contrast",
        tpw_contrast_adjustment_.get(), "value");
    settings_->bind("contrast",
        bvw.property_color_balance_contrast(), SettingsBindFlags::SETTINGS_BIND_DEFAULT
    );

    // /*saturation*/
    // settings_->bind("saturation",
    //     tpw_saturation_adjustment_.get() , "value");
    // settings_->bind("saturation",
    //     bvw.property_color_balance_saturation(), SettingsBindFlags::SETTINGS_BIND_DEFAULT
    // );

    // /* brightness*/
    // settings_->bind("brightness",
    //     tpw_bright_adjustment_.get(), "value");
    // settings_->bind("brightness",
    //     bvw.property_color_balance_brightness(), SettingsBindFlags::SETTINGS_BIND_DEFAULT
    // );

    // /*Hue*/
    // settings_->bind("hue",
    //     tpw_hue_adjustment_.get() , "value");
    // settings_->bind("hue",
    //     bvw.property_color_balance_hue(), SettingsBindFlags::SETTINGS_BIND_DEFAULT
    // );


    // /*force-software-decoder Switcher*/
    // settings_->bind("force-software-decoders", tpw_no_hardware_acceleration_, "value");

}




/****Plugins Page ****/
class PluginsPrefsPage : public HdyPrefsPageBase
{
public:
PluginsPrefsPage(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder);
    
    ~PluginsPrefsPage() override;

    TR_DISABLE_COPY_MOVE(PluginsPrefsPage)

    void update_plugin_rows(TotemPluginsEngine *engine);

private:

    Gtk::ListBox *tpw_plugins_list_ = nullptr; //container for each row
    TotemPluginsEngine *engine_ = nullptr;
};


PluginsPrefsPage::PluginsPrefsPage(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder)
    : HdyPrefsPageBase(cast_item, builder),
    tpw_plugins_list_(get_widget<Gtk::ListBox>("tpw_plugins_list"))
{
    
    // std::cout << "PluginsPrefsPage::PluginsPrefsPage ctor " << std::endl;

}



//Add non-builtin plugin rows (there only have variable-rate)
void PluginsPrefsPage::update_plugin_rows(TotemPluginsEngine *engine)
{
    const GList *plugin_infos, *l;
    if (engine_)
        g_object_unref(engine_);
  
    engine_ = g_object_ref(engine);
    
    //(tranfer null) it is held by the PeasEngine
    plugin_infos = peas_engine_get_plugin_list (PEAS_ENGINE (engine));
    for (l = plugin_infos; l != NULL; l = l->next) 
    { 
        g_autoptr(GError) error = NULL;
        PeasPluginInfo *plugin_info;
        const char *plugin_name;
        TotemPrefsPluginRow* one_row = NULL;
        
        plugin_info = PEAS_PLUGIN_INFO (l->data);
        plugin_name = peas_plugin_info_get_name (plugin_info);

        printf ("(in update_plugin_rows) iterate on plugin %s\n", plugin_name);
        
        if (!peas_plugin_info_is_available (plugin_info, &error)) 
        {
            // g_warning ("The plugin %s is not available : %s", plugin_name, error ? error->message : "no message error");
            printf ("PluginsPrefsPage:The plugin %s is not available : %s \n", plugin_name, error?error->message:"no message error");
            continue;
        }
        
        if (peas_plugin_info_is_hidden (plugin_info) 
        // || peas_plugin_info_is_builtin (plugin_info)
        )
        {
        printf ("(in update_plugin_rows) iterate on plugin %s, which is builtin\n", plugin_name);

            continue;
        }
        
        one_row = TotemPrefsPluginRow::create(*tpw_plugins_list_, engine_);
        
        if(one_row)
        {
            one_row->set_subtitle(peas_plugin_info_get_description (plugin_info));
            one_row->set_title(plugin_name);

            one_row->update_plugin_details(plugin_info);

        }
    }
}



PluginsPrefsPage::~PluginsPrefsPage()
{
    std::cout << "PluginsPrefsPage  ~PluginsPrefsPage() " << std::endl;
    if (engine_)
    {
        g_object_unref(engine_);
    }
}




}//anonymous namespace end






class TotemPrefsDialog::Impl
{

public:
    Impl(TotemPrefsDialog& widget, Glib::RefPtr<Gtk::Builder> const& builder);
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)

    void showUs();

    void render_plugins_page(TotemPluginsEngine *engine);

    void load_other_pages(BaconVideoWidget& bvw);

private:
    TotemPrefsDialog& widget_;


	Glib::RefPtr<Gio::Settings> settings_;

    DisplayPrefsPage* display_page_ = nullptr;

    AudioPrefsPage* audio_page_ = nullptr;

    PluginsPrefsPage* plugins_page_ = nullptr;


};








// /******************************************************************************************************************/
// /************************************************ExtraClassInit ***************************************************/

// TotemPrefsDialogExtraInit::TotemPrefsDialogExtraInit()
//     : ExtraClassInit(&TotemPrefsDialogExtraInit::class_init, nullptr, &TotemPrefsDialogExtraInit::instance_init)
// {
// }


// void TotemPrefsDialogExtraInit::class_init(void* klass, void* /*user_data*/)
// {
//     // printf ("(TotemPrefsDialogExtraInit::class_init)\n");

//     // auto* const widget_klass = GTK_WIDGET_CLASS(klass);

// }


// void TotemPrefsDialogExtraInit::instance_init(GTypeInstance* instance, void* /*klass*/)
// {
//     // printf ("(TotemPrefsDialogExtraInit::instance_init)\n");
// }












/*******************************************************CTOR DTOR*********************************************************/
// Use raw-pointer prevent issues like premature destruction that can occur with smart pointers
TotemPrefsDialog* TotemPrefsDialog::create()
{
    //Instantiate by get from our own .ui file (not the way of bind template .ui file)
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("totem-preferences-dialog.ui"));
    return gtr_get_widget_derived<TotemPrefsDialog>(builder, "TotemPrefsDlg");
}



TotemPrefsDialog::TotemPrefsDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder
)
:   
// Glib::ObjectBase(typeid(TotemPrefsDialog)),
    Gtk::Window(cast_item),
    impl_(std::make_unique<Impl>(*this, builder))
{

    set_modal(true);
    
    std::cout << "TotemPrefsDialog::TotemPrefsDialog ctor" << std::endl;
      
    // hide by default, opened when you press "Preferences", the action handler will unset its hidding 
    hide();

}



TotemPrefsDialog::~TotemPrefsDialog() 
{
    std::cout << "TotemPrefsDialog::~TotemPrefsDialog() " << std::endl;
}



TotemPrefsDialog::Impl::Impl(TotemPrefsDialog& widget, Glib::RefPtr<Gtk::Builder> const& builder) 
:   widget_(widget)
{
    
    std::cout << "TotemPrefsDialog::Impl::Impl ctor" << std::endl;

    //create a GSettings instance to interact with the gschema.compiled, unrefing it using g_clear_object() in our destructor
    settings_ = Gio::Settings::create(TOTEM_GSETTINGS_SCHEMA);

    //local test
    // if(settings_)
    // {
    //     Glib::VariantBase value;
    //     settings_->get_value("contrast", value);
    //     Glib::Variant<int> contrast_variant = Glib::VariantBase::cast_dynamic<Glib::Variant<int>>(value);

    //     int contrast = contrast_variant.get();

    //     std::cout << "Contrast value: " << contrast << std::endl;

    //     // std::cout << "TotemPrefsDialog::Impl, settings_ is ok " << std::endl;
    // }


    //pass settings_ to them, so we can bind, the lifetime of settings_?
    // gtr_get_widget_derived will forawrd (no copy will be made) settings_ to each Page's Constructor
    //---1. pass settings_ by reference and receive side's settings is declared as 
    // a class field "Glib::RefPtr<Gio::Settings>&", which doesn't change the reference count of the underlying object. 

    //---2. pass settings_ by value (Safer,no dangling ref) and receive side's settings is declared as 
    // a class field "Glib::RefPtr<Gio::Settings>", which increases the reference count of the underlying Gio::Settings
    display_page_ = gtr_get_widget_derived<DisplayPrefsPage>(builder, "display-page", settings_);
    audio_page_ = gtr_get_widget_derived<AudioPrefsPage>(builder, "audio-page", settings_);
    plugins_page_ = gtr_get_widget_derived<PluginsPrefsPage>(builder, "plugins-page");//plugin-page no need to use GSettings for storing the status

}



TotemPrefsDialog::Impl::~Impl()
{
    std::cout << "TotemPrefsDialog::Impl::~Impl() " << std::endl;

    /// if (settings_ != nullptr)
    /// {
    ///     g_clear_object (&settings_);
    /// }

    // if(display_page_){
    //     delete display_page_;
    // }
    // if(audio_page_){
    //     delete audio_page_;
    // }
    // if(plugins_page_){
    //     delete plugins_page_;
    // }
}








/******************************************************PUBLIC METHODS**********************************************************/
void TotemPrefsDialog::showUs()
{
    impl_->showUs();
}
void TotemPrefsDialog::Impl::showUs()
{
    widget_.show();
}


void TotemPrefsDialog::render_plugins_page(TotemPluginsEngine *engine)
{
    impl_->render_plugins_page(engine);
}
void TotemPrefsDialog::Impl::render_plugins_page(TotemPluginsEngine *engine)
{
    if(plugins_page_)
    {
        plugins_page_->update_plugin_rows(engine);
    }
}


//Audio, Display page loading
void TotemPrefsDialog::load_other_pages(BaconVideoWidget& bvw)
{
    impl_->load_other_pages(bvw);
}
void TotemPrefsDialog::Impl::load_other_pages(BaconVideoWidget& bvw)
{
    if(display_page_)
        display_page_->load_display_page_state(bvw);

    if(audio_page_)
        audio_page_->load_audio_page_state(bvw);
}

