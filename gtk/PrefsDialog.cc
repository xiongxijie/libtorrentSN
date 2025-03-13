// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "PrefsDialog.h"
#include "FreeSpaceLabel.h"
#include "GtkCompat.h"
#include "PathButton.h"
#include "Prefs.h"
#include "Session.h"
#include "Utils.h"
#include "tr-transmission.h"
#include "tr-web-utils.h"


#include <glibmm/date.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/timer.h>
#include <glibmm/ustring.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/editable.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/textview.h>
#include <gtkmm/treemodelcolumn.h>
#include <gtkmm/widget.h>

#if GTKMM_CHECK_VERSION(4, 0, 0)
#include <gtkmm/eventcontrollerfocus.h>
#endif

#include <fmt/core.h>

#include <array>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <functional>

#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/settings_pack.hpp"


using namespace lt;
using namespace libtransmission::Values;




namespace
{

/**********BASE  CLASS************/
class PageBase : public Gtk::Box
{
public:
    PageBase(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core);
    ~PageBase() override;

    TR_DISABLE_COPY_MOVE(PageBase)


    Gtk::CheckButton* init_check_button(Glib::ustring const& name, int const key);
    Gtk::SpinButton* init_speed_spin_button(Glib::ustring const& name, int const key, int& cache, int low, int high, int step);
    Gtk::SpinButton* init_integer_spin_button(Glib::ustring const& name, int const key, int& cache, int low, int high, int step);
    PathButton* init_chooser_button(Glib::ustring const& name, int const key);


    using check_spin_sync_func_t = std::function<void(int const, int const)>;

    // for spin button
    check_spin_sync_func_t check_spin_sync_cb_;



    template<typename T>
    T* get_widget(Glib::ustring const& name) const
    {
        return gtr_get_widget<T>(builder_, name);
    }

    template<typename T, typename... ArgTs>
    T* get_widget_derived(Glib::ustring const& name, ArgTs&&... args) const
    {
        return gtr_get_widget_derived<T>(builder_, name, std::forward<ArgTs>(args)...);
    }

    template<typename T, typename... ArgTs>
    static void localize_label(T& widget, ArgTs&&... args)
    {
        widget.set_label(fmt::format(widget.get_label().raw(), std::forward<ArgTs>(args)...));
    }



private:
    bool spin_cb_idle(Gtk::SpinButton& spin, int const key, int& cache, bool isSpeed);
    void spun_cb(Gtk::SpinButton& w, int const key, int& cache, bool isSpeed);
    void chosen_cb(PathButton& w, int const key);


private:

    Glib::RefPtr<Gtk::Builder> const& builder_;

    Glib::RefPtr<Session> const& core_;

    std::map<int const, std::pair<std::unique_ptr<Glib::Timer>, sigc::connection>> spin_timers_;



};



PageBase::PageBase(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core)
    : Gtk::Box(cast_item)
    , builder_(builder)
    , core_(core)
{
}

PageBase::~PageBase()
{

                    std::cout << "PrefsDialog ~PageBase()" << std::endl;
    for (auto& [key, info] : spin_timers_)
    {
        info.second.disconnect();
    }
}



/********************CHCECK BOTTON*********************/
Gtk::CheckButton* PageBase::init_check_button(Glib::ustring const& name, int const key)
{
    auto* button = get_widget<Gtk::CheckButton>(name);
    //init check/unchcked
    button->set_active(core_->get_sett().get_bool(key));
    button->signal_toggled().connect([this, button, key]() 
    { 
        core_->set_pref(key, button->get_active()); 
    }
    );
    return button;
}




bool PageBase::spin_cb_idle(Gtk::SpinButton& spin, int const key, int& cache, bool isSpeed)
{
    auto const last_change_it = spin_timers_.find(key);
    g_assert(last_change_it != spin_timers_.end());

    /* user may click very fast, do not modify now, avoid fluctuation */
    if (last_change_it->second.first->elapsed() < 0.33)
    {
        /*tiemr will call again*/
        return true;
    }


    /* update cache value */
    cache = spin.get_value_as_int();

    //for up/down speed limit
    if(isSpeed)
    {
        //convert from KBps to Byps
        auto speed_tmp = Speed{cache, Speed::Units::KByps };
        int byps_val = speed_tmp.count(Speed::Units::Byps);
                std::cout << "up/down litmit is " << byps_val << std::endl;

        /* update the core */
        core_->set_pref(key, byps_val);
    }
    //for max peer connections
    else
    {
                std::cout << "max peer cache is " << cache << std::endl;
        /* update the core */
        core_->set_pref(key, cache);
    }

    /* cleanup */
    spin_timers_.erase(last_change_it);


    /* stop the timer */
    return false;
}

void PageBase::spun_cb(Gtk::SpinButton& w, int const key, int& cache, bool isSpeed)
{
    /* user may be spinning through many values, so let's hold off
       for a moment to keep from flooding the core with changes */

    //find by `settings_pack key` such as lt::TR_KEY_speed_limit_up
    auto last_change_it = spin_timers_.find(key);

    //if already exist, dont add new
    //only if it not exist, we create one
    if (last_change_it == spin_timers_.end())
    {
        auto timeout_tag = Glib::signal_timeout().connect_seconds(
            [this, &w, key, &cache, isSpeed]() {                 
                    return spin_cb_idle(w, key, cache, isSpeed); 
            },
            1);
        last_change_it = spin_timers_.emplace(key, std::pair(std::make_unique<Glib::Timer>(), timeout_tag)).first;
    }

    //start the Timer
    last_change_it->second.first->start();
}



/********************UP/Down Limit SpinButton*********************/
Gtk::SpinButton* PageBase::init_speed_spin_button(Glib::ustring const& name, int const key, int& cache, int low, int high, int step)
{
    auto* button = get_widget<Gtk::SpinButton>(name);

    //set SpinButton initial value

    auto speed_tmp = Speed{core_->get_sett().get_int(key), Speed::Units::Byps };
    //convert from byps integer to (kbytes per second) Integer 
    int kbyps_val = static_cast<int>(speed_tmp.count(Speed::Units::KByps));


    button->set_adjustment(Gtk::Adjustment::create(kbyps_val, low, high, step));
    button->set_digits(0);

    button->signal_value_changed().connect([this, button, key, &cache]() 
    { 
        spun_cb(*button, key, cache, true); 
    }
    );

    return button;
}
/**********************Max Peer Connections SpinButton******************/
Gtk::SpinButton* PageBase::init_integer_spin_button(Glib::ustring const& name, int const key, int& cache, int low, int high, int step)
{
    auto* button = get_widget<Gtk::SpinButton>(name);

    //set SpinButton initial value
    button->set_adjustment(Gtk::Adjustment::create(core_->get_sett().get_int(key), low, high, step));
                
    button->set_digits(0);

    button->signal_value_changed().connect([this, button, key, &cache]() 
    { 
        spun_cb(*button, key, cache, false); 
    }
    );

    return button;
}
/***************************************************/










/*****************PathButton***********************/
void PageBase::chosen_cb(PathButton& w, int const key)
{

            // std::cout << "111 Pathbutton chose cb : " << w.get_filename() << std::endl;

    core_->set_pref(key, w.get_filename());
}

PathButton* PageBase::init_chooser_button(Glib::ustring const& name, int const key)
{
    auto* const button = get_widget_derived<PathButton>(name);

    if (auto const path = core_->get_sett().get_str(key); !path.empty())
    {
        button->set_filename(path);
    }

    button->signal_selection_changed().connect([this, button, key]() { chosen_cb(*button, key); });
    return button;
}
/****************************************************/







/*******************************************************
*****  Download Tab
        --watch dir (Path Button)
        --Show Torrent Options Dialog (Checkbox)
        -- start when add torrent (checkbox)
        -- Trash torrent file after adding (checkbox)
        -- Save to Location (Path Button)
****/



class DownloadingPage : public PageBase
{
public:
    DownloadingPage(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core);
    
    ~DownloadingPage() override;

    TR_DISABLE_COPY_MOVE(DownloadingPage)

private:

    void on_core_prefs_changed(int const key);

private:
    Glib::RefPtr<Session> const& core_;

    FreeSpaceLabel* freespace_label_ = nullptr;

    sigc::connection core_prefs_tag_;
};



DownloadingPage::DownloadingPage(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Session> const& core)
    : PageBase(cast_item, builder, core),
    core_(core),
    freespace_label_(get_widget_derived<FreeSpaceLabel>("download_dir_stats_label", core))
{
    core_prefs_tag_ = core_->signal_prefs_changed().connect(sigc::mem_fun(*this, &DownloadingPage::on_core_prefs_changed));

    init_check_button("watch_dir_check", settings_pack::TR_KEY_watch_dir_enabled);
    init_chooser_button("watch_dir_chooser", settings_pack::TR_KEY_watch_dir);

    
    init_check_button("show_options_dialog_check", settings_pack::TR_KEY_show_options_window);
    init_check_button("start_on_add_check", settings_pack::TR_KEY_start_added_torrents);
    init_check_button("trash_on_add_check", settings_pack::TR_KEY_trash_original_torrent_files);

    /* change TR_KEY_download_dir will not affect the previously added torrent task, they are free from this modification */
    init_chooser_button("download_dir_chooser", lt::settings_pack::TR_KEY_download_dir);
    
    on_core_prefs_changed(lt::settings_pack::TR_KEY_download_dir);
}

DownloadingPage::~DownloadingPage()
{
        std::cout << "PrefsDialog  ~DownloadingPage() " << std::endl;

    core_prefs_tag_.disconnect();
}




void DownloadingPage::on_core_prefs_changed(int const key)
{
    if (key == lt::settings_pack::TR_KEY_download_dir)
    {
        //get the download dir
        std::string downloadDir = core_->get_sett().get_str(lt::settings_pack::TR_KEY_download_dir);

        freespace_label_->set_dir(downloadDir.c_str());
    }
}


/*****************************************************************************/






/******************************************************************************
 * SPEED Tab
 *  --(Global) Upload Speed Limits (Spin Button)
 *  -- (Global) Download Speed Limits (Spin Button)
 */

class SpeedPage : public PageBase
{
public:

    SpeedPage(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core);

    ~SpeedPage() override/* = default*/;

    TR_DISABLE_COPY_MOVE(SpeedPage)


    void speed_check_spin_sync_cb(int const keyBool, int const keyInt);

    Gtk::CheckButton* init_speed_limit_check_button(Glib::ustring const& name, int const keyBool, int const keyInt, check_spin_sync_func_t f);



private:

    //const-reference RefPtr dont affect reference count
    Glib::RefPtr<Session> const& core_;


    Gtk::SpinButton* speed_limit_up_spin_ = nullptr;
    Gtk::SpinButton* speed_limit_down_spin_ = nullptr;


    //This IS `GLOBAL` speed limits applying to all torrents which reside in settings_pack, not throught torrent_handle method such as set_upload_limit(), set_download_litmit() ,etc.


    //used to enable disable speed limit, disable means unlimit speed
    Gtk::CheckButton* speed_limit_up_check_ = nullptr;
    Gtk::CheckButton* speed_limit_down_check_ = nullptr;


    //kb/s
    int speed_up_cache_ = 256;
    //kb/s
    int speed_down_cache_ = 4096;

};


/*
    check the checkbox, set speed_litmit_up/down_enabled to true , and sync the spin button value with internal settings
    uncheck the checkbox, set xxx_enabled to false, and set the internal setting items to zero
*/
Gtk::CheckButton* SpeedPage::init_speed_limit_check_button(Glib::ustring const& name, int const keyBool, int const keyInt, check_spin_sync_func_t f)
{
    auto* button = get_widget<Gtk::CheckButton>(name);
    //init checked/unchecked
    button->set_active(core_->get_sett().get_bool(keyBool));


    button->signal_toggled().connect([this, keyBool, keyInt, f]() 
    { 

        //call pass-in function 
        //when speed_limit_up/down_check_ has not been init yet, just return 
        if(!speed_limit_up_check_ || !speed_limit_down_check_)
        {
            return;
        }

        if(f)
        {   
            f(keyBool, keyInt);
        }

    });

    return button;
}




SpeedPage::SpeedPage(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core)
    : PageBase(cast_item, builder, core),
    core_(core),
    speed_limit_up_spin_( init_speed_spin_button("upload_limit_spin", settings_pack::TR_KEY_speed_limit_up, speed_up_cache_, 0, std::numeric_limits<int>::max(), 1)),
    speed_limit_down_spin_(init_speed_spin_button("download_limit_spin", settings_pack::TR_KEY_speed_limit_down, speed_down_cache_, 0, std::numeric_limits<int>::max(), 1))

{


    //callback -- sync spinButton with checkButton
    //   -   checkButton checked -- SpinButton enabled
    //   -   checkButton unchecked -- SpinButton disabled
    check_spin_sync_cb_ = std::bind(&SpeedPage::speed_check_spin_sync_cb, this, std::placeholders::_1, std::placeholders::_2);
    speed_limit_up_check_ = init_speed_limit_check_button("upload_limit_check", settings_pack::TR_KEY_speed_limit_up_enabled, settings_pack::TR_KEY_speed_limit_up, check_spin_sync_cb_);
    speed_limit_down_check_ = init_speed_limit_check_button("download_limit_check", settings_pack::TR_KEY_speed_limit_down_enabled, settings_pack::TR_KEY_speed_limit_down, check_spin_sync_cb_);



    //default unit is KB per second
    auto const speed_units_kbyps_str = Speed::units().display_name(Speed::Units::KByps);

    /*up limit  `speed_units` is defined in .ui file*/
    localize_label(*speed_limit_up_check_, fmt::arg("speed_units", speed_units_kbyps_str));

    /*down limit `speed_units` is defined in .ui file*/
    localize_label(*speed_limit_down_check_, fmt::arg("speed_units", speed_units_kbyps_str));

    

}


//called when CheckButton *toggled*
void SpeedPage::speed_check_spin_sync_cb(int const keyBool, int const keyInt)
{

    //when either spin has not been initialized when first load speed page
    if(!speed_limit_up_spin_ || !speed_limit_down_spin_)
    {
        //do nothing
        return;
    }
    else
    {
        bool is_upload = (keyBool == lt::settings_pack::TR_KEY_speed_limit_up_enabled);
        auto const& target_spin_ref = is_upload ? speed_limit_up_spin_ : speed_limit_down_spin_;
        auto const& target_check_ref = is_upload ? speed_limit_up_check_ : speed_limit_down_check_;
        auto const up_or_down_cache = is_upload ? speed_up_cache_ : speed_down_cache_;
        auto is_checked = target_check_ref->get_active();
       
                        // std::cout << "up or down cache " << up_or_down_cache << std::endl;
            
        //sync spin value with intenral settings
        if(is_checked)
        {
            //update the GUI
            target_spin_ref->set_value(up_or_down_cache);

            //convert from Kbps to Byps
            auto speed_tmp = Speed{up_or_down_cache, Speed::Units::KByps };
            int byps_val = speed_tmp.count(Speed::Units::Byps);

            //update the core
            core_->set_pref(keyInt, byps_val);
        }
        //when unchcked, set speed limit to zero meaning *no limit*
        else
        {
            core_->set_pref(keyInt, 0);
        }

        core_->set_pref(keyBool, is_checked);
    }
}


SpeedPage::~SpeedPage()
{
        std::cout << "PrefsDialog  ~SpeedPage() " << std::endl;
    
}
/******************************************************************************/











/******************************************************************************
*****  Network Tab
        -- Enable Upnp Natpmp checkbox
        -- enable utp checkbox
        -- enable lsd checkbox
        -- inhitbit screen sleep checkbox
        -- set max connection of session (Spin Button)
****/

class NetworkPage : public PageBase
{
public:
    NetworkPage(BaseObjectType* cast_item, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core);
    ~NetworkPage() override;

    TR_DISABLE_COPY_MOVE(NetworkPage)

    void maxconn_check_spin_sync_cb(int const keyBool, int const keyInt);

    Gtk::CheckButton* init_maxconn_limit_check_button(Glib::ustring const& name, int const keyBool, int const keyInt, check_spin_sync_func_t f);


private:

    Glib::RefPtr<Session> const& core_;

    Gtk::SpinButton* max_conn_spin_ = nullptr;

    Gtk::CheckButton* max_conn_check_ = nullptr;




    int const unlimit_max_peers = (1 << 24) - 1;
    int max_conn_cache_ = 200;
};

NetworkPage::NetworkPage(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Glib::RefPtr<Session> const& core)
    : PageBase(cast_item, builder, core)
    , core_(core)
    , max_conn_spin_(init_integer_spin_button("max_conn_spin", settings_pack::TR_KEY_connections_limit, max_conn_cache_, 2, std::numeric_limits<int>::max(), 1))

{


    check_spin_sync_cb_ = std::bind(&NetworkPage::maxconn_check_spin_sync_cb, this, std::placeholders::_1, std::placeholders::_2);

    max_conn_check_ = init_maxconn_limit_check_button("max_conn_check", settings_pack::TR_KEY_connections_limit_enabled, settings_pack::TR_KEY_connections_limit, check_spin_sync_cb_);


    init_check_button("inhibit_hibernation_check", settings_pack::TR_KEY_inhibit_desktop_hibernation);
 
    //..enable both settings_pack::enable_upnp and enable_natpmp two items
    init_check_button("enable_listening_port_forwarding_check", settings_pack::TR_KEY_enable_port_forwarding);





    //..manipulate the settings_pack::enable_lsd
    init_check_button("enable_lsd_check", settings_pack::enable_lsd);



    //..enblle both settings_pack::enable_utp_incoming and enable_utp_outgoing    
    init_check_button("enable_utp_check", settings_pack::TR_KEY_enable_utp);

}




Gtk::CheckButton* NetworkPage::init_maxconn_limit_check_button(Glib::ustring const& name, int const keyBool, int const keyInt, check_spin_sync_func_t f)
{
    auto* button = get_widget<Gtk::CheckButton>(name);

    //init checked/unchecked
    button->set_active(core_->get_sett().get_bool(keyBool));

    button->signal_toggled().connect([this, keyBool, keyInt, f]() 
    { 
        if(!max_conn_check_ )
        {
            return;
        }

        if(f)
        {   
            f(keyBool, keyInt);
        }

    });

    return button;

}





void NetworkPage::maxconn_check_spin_sync_cb(int const keyBool, int const keyInt)
{
      //when either spin has not been initialized when first load speed page
    if(!max_conn_spin_ )
    {
        //do nothing
        return;
    }
    else
    {
        
        auto is_checked = max_conn_check_->get_active();       

        //sync spin value with intenral settings
        if(is_checked)
        {
            //update the GUI
            max_conn_spin_->set_value(max_conn_cache_);

            //update the core
            core_->set_pref(keyInt, max_conn_cache_);
        }
        //when unchcked, set max peer connections to (1<<24)-1 meaning *no limit*
        else
        {
            core_->set_pref(keyInt, unlimit_max_peers/* (1<<24)-1 */);
        }

        core_->set_pref(keyBool, is_checked);
    }
}


NetworkPage::~NetworkPage()
{    
        std::cout << "PrefsDialog  ~NetWorkPage() " << std::endl;
}

/************************************************************************ */



} //anonymous namespace







class PrefsDialog::Impl
{
public:
    Impl(PrefsDialog& dialog, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core);
    ~Impl() /*= default*/;

    TR_DISABLE_COPY_MOVE(Impl)

private:
    void response_cb(int response);

private:
    PrefsDialog& dialog_;
    //const-reference of RefPtr dont affect reference count
    Glib::RefPtr<Session> const& core_;

    // Glib::RefPtr<Gtk::Builder> builder_;

    //their lifetime is managed by PrefSDialog::Impl
    // SpeedPage* speed_pg_ = nullptr;
    // DownloadingPage* down_pg_ = nullptr;
    // NetworkPage* network_pg_ = nullptr;
};



/**
***
**/

void PrefsDialog::Impl::response_cb(int response)
{
    if (response == TR_GTK_RESPONSE_TYPE(HELP))
    {
        gtr_open_uri(gtr_get_help_uri() + "/html/preferences.html");
    }

    if (response == TR_GTK_RESPONSE_TYPE(CLOSE))
    {
        dialog_.close();
    }
}











/***********
****CTOR****
************/


PrefsDialog::Impl::Impl(PrefsDialog& dialog, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core)
    : dialog_(dialog)
    , core_(core)
    // , builder_(builder)
    // , speed_pg_(gtr_get_widget_derived<SpeedPage>(builder, "speed_page_layout", core_))
    // , down_pg_(gtr_get_widget_derived<DownloadingPage>(builder, "downloading_page_layout", core_))
    // , network_pg_(gtr_get_widget_derived<NetworkPage>(builder, "network_page_layout", core_))
{

    // no field to receivce the returned widget ptr,
    // automactically destructed as long as copying builder as a field  
    gtr_get_widget_derived<SpeedPage>(builder, "speed_page_layout", core_);
    gtr_get_widget_derived<DownloadingPage>(builder, "downloading_page_layout", core_);
    gtr_get_widget_derived<NetworkPage>(builder, "network_page_layout", core_);

    dialog_.signal_response().connect(sigc::mem_fun(*this, &Impl::response_cb));

}


std::unique_ptr<PrefsDialog> PrefsDialog::create(Gtk::Window& parent, Glib::RefPtr<Session> const& core)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("PrefsDialog.ui"));
    return std::unique_ptr<PrefsDialog>(gtr_get_widget_derived<PrefsDialog>(builder, "PrefsDialog", parent, core));
}

PrefsDialog::PrefsDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core)
    : Gtk::Dialog(cast_item)
    , impl_(std::make_unique<Impl>(*this, builder, core))
{
    set_transient_for(parent);
}

PrefsDialog::~PrefsDialog() /*= default;*/
{
                            std::cout << "PrefsDialog::~PrefsDialog() " << std::endl;
}

PrefsDialog::Impl::~Impl()
{
                            std::cout << "PrefsDialog::Impl::~Impl() " << std::endl;

    // builder_->unreference();
    // builder_.reset();

    //call PageBase::builder_ field unreference() instead
    // delete speed_pg_;
    // delete down_pg_;
    // delete network_pg_;

}