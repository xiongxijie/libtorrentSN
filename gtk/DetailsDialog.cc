// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include "DetailsDialog.h"

#include "Actions.h"
#include "FileList.h"
#include "GtkCompat.h"
#include "HigWorkarea.h" // GUI_PAD, GUI_PAD_BIG, GUI_PAD_SMALL
#include "Prefs.h"
#include "PrefsDialog.h"
#include "Session.h"
#include "Utils.h"


#include "tr-values.h"
#include "tr-web-utils.h"
#include "tr-utils.h"

#include <gdkmm/pixbuf.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/markup.h>
#include <glibmm/quark.h>
#include <glibmm/ustring.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/button.h>
#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/cellrendererprogress.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>
#include <gtkmm/tooltip.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treemodelfilter.h>
#include <gtkmm/treemodelsort.h>
#include <gtkmm/treerowreference.h>
#include <gtkmm/treeview.h>

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib> // abort()
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <chrono>
#include <iomanip> //std::setprecision()


#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/torrent_status.hpp"
#include "libtorrent/peer_info.hpp"
#include "libtorrent/announce_entry.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/peer_id.hpp"
#include "libtorrent/sha1_hash.hpp"
#include "libtorrent/time.hpp"





using namespace lt;

using namespace std::literals;

using namespace libtransmission::Values;


class DetailsDialog::Impl
{
public:
    Impl(DetailsDialog& dialog, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core,  lt::torrent_handle && handle_regular);
    ~Impl();

    TR_DISABLE_COPY_MOVE(Impl)


    void refresh();

private:
    void info_page_init(Glib::RefPtr<Gtk::Builder> const& builder);
    void peer_page_init(Glib::RefPtr<Gtk::Builder> const& builder);
    void tracker_page_init(Glib::RefPtr<Gtk::Builder> const& builder);
    void options_page_init(Glib::RefPtr<Gtk::Builder> const& builder);

    void on_details_window_size_allocated();

    bool onPeerViewQueryTooltip(int x, int y, bool keyboard_tip, Glib::RefPtr<Gtk::Tooltip> const& tooltip);

    void on_tracker_list_selection_changed();
    void on_tracker_list_add_button_clicked();
    void on_edit_trackers();


    void torrent_set_bool(int const key, bool value);
    void torrent_set_int(int const key, int value);

    void refreshInfo(lt::torrent_status const& st);
    void refreshPeers(std::vector<lt::peer_info> const& peers);
    void refreshTracker(std::vector<lt::announce_entry> const& trackers, std::string const& cur_tracker);
    void refreshFiles();
    void refreshOptions();

    void refreshPeerList(std::vector<lt::peer_info> const& peers);


    // lt::torrent_handle tracker_list_get_current_torrent() const;

    lt::torrent_status getTorrentsStatus();

    std::vector<lt::peer_info> getPeersInfo();

    std::vector<lt::announce_entry> getTrackersInfo();


private:
    DetailsDialog& dialog_;

    Glib::RefPtr<Session> const& core_;

    lt::torrent_handle handle_regular_;
  


    /* upload speed limit */
    Gtk::CheckButton* up_limit_check_ = nullptr;
    Gtk::SpinButton* up_limit_spin_ = nullptr;

    /* upload speed limit */
    Gtk::CheckButton* down_limit_check_ = nullptr;
    Gtk::SpinButton* down_limit_spin_ = nullptr;

    /* torent priority */
    Gtk::SpinButton* bandwidth_spin_ = nullptr;

    /* max peer connections */
    Gtk::SpinButton* max_peers_spin_ = nullptr;
    Gtk::CheckButton* max_peers_check_ = nullptr;
      


    // sigc::connection honor_limits_check_tag_;
 


    sigc::connection down_limit_check_tag_;
    sigc::connection down_limit_spin_tag_;

    sigc::connection up_limit_check_tag_;
    sigc::connection up_limit_spin_tag_;
  
    sigc::connection bandwidth_spin_tag_;

    sigc::connection max_peers_check_tag_;
    sigc::connection max_peers_spin_tag_;


    //deafult up/down spinButton initial value 
    int up_limit_cache_ = 256;
    int down_limit_cache_ = 4096;
    int torrent_prio_cache_ = 0;
    int max_peers_cache_ = 200;
    //`1<<24-1` is libtorrent internal for unlimited maximum number of peers connection
    int const unlimit_max_peers = (1 << 24) - 1;
    // sigc::connection ratio_combo_tag_;
    // sigc::connection ratio_spin_tag_;
    // sigc::connection idle_combo_tag_;
    // sigc::connection idle_spin_tag_;

    Gtk::Label* added_lb_ = nullptr;
    Gtk::Label* size_lb_ = nullptr;
    Gtk::Label* state_lb_ = nullptr;
    Gtk::Label* dl_lb_ = nullptr;
    Gtk::Label* ul_lb_ = nullptr;
    Gtk::Label* error_lb_ = nullptr;
    Gtk::Label* date_started_lb_ = nullptr;
    Gtk::Label* eta_lb_ = nullptr;
    Gtk::Label* hash_lb_ = nullptr;
    Gtk::Label* origin_lb_ = nullptr;
    Gtk::Label* destination_lb_ = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> comment_buffer_;



    std::unordered_map<std::string, Gtk::TreeRowReference> peer_hash_;
    Glib::RefPtr<Gtk::ListStore> peer_store_;
    Gtk::TreeView* peer_view_ = nullptr;

    Glib::RefPtr<Gtk::ListStore> tracker_store_;
    std::unordered_map<std::string, Gtk::TreeRowReference> tracker_hash_;
    Gtk::Button* add_tracker_button_ = nullptr;
    Gtk::Button* edit_trackers_button_ = nullptr;
    Gtk::TreeView* tracker_view_ = nullptr;

    // Gtk::Button* replace_tracker_button_ = nullptr;
    // Gtk::CheckButton* scrape_check_ = nullptr;
    // Gtk::CheckButton* all_check_ = nullptr;
    // Glib::RefPtr<Gtk::TreeModelFilter> trackers_filtered_;

    FileList* file_list_ = nullptr;
    Gtk::Label* file_label_ = nullptr;

    // std::vector<std::uint32_t> uniq_ids_;
    std::uint32_t uniq_id_ = {};
    
    sigc::connection periodic_refresh_tag_;

    Glib::Quark const TORRENT_ID_KEY = Glib::Quark("tr-torrent-id-key");
    Glib::Quark const TEXT_BUFFER_KEY = Glib::Quark("tr-text-buffer-key");
    Glib::Quark const URL_ENTRY_KEY = Glib::Quark("tr-url-entry-key");

    static guint last_page_;
};

guint DetailsDialog::Impl::last_page_ = 0;



/*get torrent status for Information Tab  return a copy */
lt::torrent_status DetailsDialog::Impl::getTorrentsStatus()
{
    if (lt::torrent_status st = core_->find_torrent_status(uniq_id_); st.handle.is_valid())
    {
        return st;
    }

    return lt::torrent_status{}; 
}



/*get peer info vector for Peers Tab*/
std::vector<lt::peer_info> DetailsDialog::Impl::getPeersInfo()
{
    if (Glib::RefPtr<Torrent> const& Tor = core_->find_Torrent(uniq_id_); Tor && Tor->is_Tor_valid())
    {
        auto peers = Tor->get_peers_vec_ref();
        return peers;    
    }

    return {};
}






/*get announce entry vector for Trakckers Tab*/
std::vector<lt::announce_entry> DetailsDialog::Impl::getTrackersInfo()
{
    if (Glib::RefPtr<Torrent> const& Tor = core_->find_Torrent(uniq_id_); Tor && Tor->is_Tor_valid())
    {
        auto trackers = Tor->get_trackers_vec_ref();
        return trackers;    
    }

    return {};
}









/****
*****
*****  OPTIONS TAB *************************
*****
****/

namespace
{

void set_togglebutton_if_different(Gtk::CheckButton* toggle, sigc::connection& tag, bool value)
{
    bool const currentValue = toggle->get_active();

    if (currentValue != value)
    {
        tag.block();
        toggle->set_active(value);
        tag.unblock();
    }
}

void set_int_spin_if_different(Gtk::SpinButton* spin, sigc::connection& tag, int value)
{
    int const currentValue = spin->get_value_as_int();

        // std::cout << "set_int_spin_if_different : " << currentValue << std::endl; 

    if (currentValue != value)
    {
        tag.block();
        spin->set_value(value);
        tag.unblock();
    }
}

// void set_double_spin_if_different(Gtk::SpinButton* spin, sigc::connection& tag, double value)
// {
//     double const currentValue = spin->get_value();

//     if ((int)(currentValue * 100) != (int)(value * 100))
//     {
//         tag.block();
//         spin->set_value(value);
//         tag.unblock();
//     }
// }

// void unset_combo(Gtk::ComboBox* combobox, sigc::connection& tag)
// {
//     tag.block();
//     combobox->set_active(-1);
//     tag.unblock();
// }

} // anonymous namespace







/***
****  Options Page ****************
***/
void DetailsDialog::Impl::refreshOptions()
{
    lt::torrent_status st = core_->find_torrent_status(uniq_id_); 


    if (!st.handle.is_valid())
    {
        return;
    }

    /* down_limit_spin */
    if (st.handle.is_valid())
    {
        //be mindful that this is in byps, and we must convert it to Kbps for GUI display 
        int const baseline = st.handle.download_limit();
        //only update SPinButton when down_limit_check is checked
        if(baseline > 0)
        {
            auto speed_tmp = Speed{ baseline, Speed::Units::Byps };
            //convert queried speed litmit from kbps integer to (bytes per second) Integer 
            int kbyps_val = speed_tmp.count(Speed::Units::KByps);
            //update cache
            down_limit_cache_ = kbyps_val;

                        std::cout << "download queryied is kbyps " << down_limit_cache_ << std::endl;

            set_int_spin_if_different(down_limit_spin_, down_limit_spin_tag_, down_limit_cache_);
        }
    
    }


    /* up_limit_spin */
    if (st.handle.is_valid())
    {
        //be mindful that this is in byps, and we must convert it to Kbps for GUI display 
        int const baseline = st.handle.upload_limit();
        //only update SPinButton when up_limit_check is checked
        if(baseline > 0)
        {
            auto speed_tmp = Speed{ baseline, Speed::Units::Byps };
            //convert queried speed limit from kbps integer to (bytes per second) Integer 
            int kbyps_val = speed_tmp.count(Speed::Units::KByps);
            //update cache
            up_limit_cache_ = kbyps_val;

                        std::cout << "upload queryied is kbyps " << up_limit_cache_ << std::endl;

            set_int_spin_if_different(up_limit_spin_, up_limit_spin_tag_, up_limit_cache_);
        }
    }



    /*torrent priority spin*/
    if (st.handle.is_valid())
    {
        int const baseline = st.priority;       

        //update only when diffent to avoid  fluctuation such as 0->1->0->1 
        if(torrent_prio_cache_!= baseline)
        {
            torrent_prio_cache_ = baseline;

                    // std::cout << "torrent priority  queried is  " << torrent_prio_cache_ << std::endl;

            set_int_spin_if_different(bandwidth_spin_, bandwidth_spin_tag_, torrent_prio_cache_);
        }        
    }




    /* 
    max_peers_spin 
    */
    if (st.handle.is_valid())
    {
        /* 

        when you call `st.handle.max_connections()`

        the default max_connection of torrent is 16,777,215(`(1 << 24) -1`) a very large number means unlitmited 

        pass -1 to h.set_max_connection() will also set the max_conntion to '2<<24-1' which means unlimited

        */ 

        int const baseline = st.handle.max_connections();
        if(!(baseline >= unlimit_max_peers))
        {
            max_peers_cache_ = baseline;

                    std::cout << "max connection queried is  " << max_peers_cache_ << std::endl;

            set_int_spin_if_different(max_peers_spin_, max_peers_spin_tag_, max_peers_cache_);
        }
    }

    
}






void DetailsDialog::Impl::torrent_set_bool(int const key, bool value)
{
    core_->set_pref(key,value);
}

void DetailsDialog::Impl::torrent_set_int(int const key, int value)
{
    core_->set_pref(key,value);
   
}







void DetailsDialog::Impl::options_page_init(Glib::RefPtr<Gtk::Builder> const& /*builder*/)
{

    //UNUSED
    // auto const speed_units_kbyps_str = Speed::units().display_name(Speed::Units::KByps);
  
    //REDUNDANT
    // lt::torrent_status& st = core_->find_torrent_status(uniq_id_); 



    /**************download speed limit (Kbytes per second), keep in mind that 0 means unlimit 

        `Only Apply to current torrent, not global speed limit(which resides settings_pack)`

    */

    //must init the checkButton or SpinButton , use download_limit() which come from resume_data of torrent
    int const down_limit_initial = handle_regular_.download_limit();
    // 0 --- unchecked
    // >= 1 --- checked
    down_limit_check_->set_active(down_limit_initial > 0);

    down_limit_spin_->set_adjustment(Gtk::Adjustment::create(4096, 1, std::numeric_limits<int>::max(), 1));
    down_limit_spin_tag_ = down_limit_spin_->signal_value_changed().connect(
        [this]() { 

            auto speed_tmp = Speed{down_limit_spin_->get_value_as_int(), Speed::Units::KByps };
            //convert from kbps integer to (bytes per second) Integer 
            int byps_val = speed_tmp.count(Speed::Units::Byps);

                    // std::cout << "download kbps convert to byps is " << byps_val << std::endl;

            // It is given as the number of bytes per second the torrent is allowed to download.
            handle_regular_.set_download_limit(byps_val);
        }
    );

    down_limit_check_tag_ = down_limit_check_->signal_toggled().connect(
        [this]() 
        {
            //if unchecked, means unlimited, pass '0' to torrent)handle::set_download_limit()
            if(!down_limit_check_->get_active())
            {
                handle_regular_.set_download_limit(0);
            }
            //when checked, set to default value  or previous value
            else
            {
            
                set_int_spin_if_different(down_limit_spin_, down_limit_spin_tag_, down_limit_cache_);

                auto speed_tmp = Speed{down_limit_cache_ , Speed::Units::KByps };
                //convert from kbps integer to (bytes per second) Integer 
                int byps_val = speed_tmp.count(Speed::Units::Byps);
                handle_regular_.set_download_limit(byps_val);

            }
        }
    );









    /********upload speed limit  (Kbytes per second) , keep in mind that 0 means unlimit 

        `Only Apply to current torrent, not global speed limit(which resides settings_pack)`

    */

    //must init the checkButton or SpinButton , use upload_limit() which come from resume_data of torrent
    int const up_limit_initial = handle_regular_.upload_limit();
    // 0 --- unchecked
    // >= 1 --- checked
    up_limit_check_->set_active(up_limit_initial > 0);

    up_limit_spin_->set_adjustment(Gtk::Adjustment::create(256, 1, std::numeric_limits<int>::max(), 1));
    up_limit_spin_tag_ = up_limit_spin_->signal_value_changed().connect(
        [this]() { 
            auto speed_tmp = Speed{up_limit_spin_->get_value_as_int(), Speed::Units::KByps };
            //convert from kbps integer to (bytes per second) Integer 
            int byps_val = speed_tmp.count(Speed::Units::Byps);

                    // std::cout << "upload kbps convert to byps is " << byps_val << std::endl;

            // It is given as the number of bytes per second the torrent is allowed to upload.
            handle_regular_.set_upload_limit(byps_val);
        }
    );


    up_limit_check_tag_ = up_limit_check_->signal_toggled().connect(
        [this]() 
        {
            //if unchecked, means unlimited, pass '0' to torrent)handle::set_upload_limit()
            if(!up_limit_check_->get_active())
            {
                handle_regular_.set_upload_limit(0);
            }
            //when checked, set to default value  or previous value
            else
            {
                set_int_spin_if_different(up_limit_spin_, up_limit_spin_tag_, up_limit_cache_);
                auto speed_tmp = Speed{up_limit_cache_ , Speed::Units::KByps };
                //convert from kbps integer to (bytes per second) Integer 
                int byps_val = speed_tmp.count(Speed::Units::Byps);
                handle_regular_.set_upload_limit(byps_val);

            }
        }
    );








    /*********torrent priority*/
    bandwidth_spin_->set_adjustment(Gtk::Adjustment::create(0, 0, 255, 1));
    bandwidth_spin_tag_ = bandwidth_spin_->signal_changed().connect(
        [this]() { 

            // The default priority is 0, which is the lowest priority.
            handle_regular_.set_priority(bandwidth_spin_->get_value_as_int());
        }
    );





    /********* max peer connection , 
     
      Only Apply to Current torrent,  not the Global Peer Connection Maximum (settings_pack::connections_limit)
      
    */
    //must init the checkButton or SpinButton , use upload_limit() which come from resume_data of torrent
    int const max_peers_initial = handle_regular_.max_connections();
    // >= 1<<24-1 --- unchecked
    // otherwise --- checked
    max_peers_check_->set_active(!(max_peers_initial >= unlimit_max_peers));


    max_peers_spin_->set_adjustment(Gtk::Adjustment::create(200, 2, std::numeric_limits<int>::max(), 1));
    max_peers_spin_tag_ = max_peers_spin_->signal_value_changed().connect(
        [this]() 
        { 
            int max_peer = max_peers_spin_->get_value_as_int();

                        // std::cout << "max_peers_spin_ changed sinal: " << max_peer << std::endl;

            //This must be at least 2. The default is unlimited number of connections. If
            // -1 is given to the function, it means unlimited
            handle_regular_.set_max_connections(max_peer);
        }
    );


    max_peers_check_tag_ = max_peers_check_->signal_toggled().connect(
        [this]() 
        {
            //if unchecked, means unlimited, pass '0' to torrent)handle::set_upload_limit()
            if(!max_peers_check_->get_active())
            {
                handle_regular_.set_max_connections(-1);
            }
            //when checked, set to default value or previous value
            else
            {
                set_int_spin_if_different(max_peers_spin_, max_peers_spin_tag_, max_peers_cache_);
                handle_regular_.set_max_connections(max_peers_cache_);
            }
        }
    );





 
}





/****
*****
*****  INFO TAB *****************
*****
****/

namespace
{

std::string to_hex(lt::sha1_hash const& s)
{
	std::stringstream ret;
	ret << s;
	return ret.str();
}


Glib::ustring torrent_state_txt(lt::torrent_status const& s)
{

    static char const* state_str[] =
        {"queued for checking", "checking", "ddownload metadata"
        , "downloading", "finished", "seeding", "", "checking resume data"};

	if (s.errc) return s.errc.message();
	std::string ret;
	if ((s.flags & lt::torrent_flags::paused) &&
		(s.flags & lt::torrent_flags::auto_managed))
	{
		ret += "queued for ";
	}

	if (s.state == lt::torrent_status::downloading
		&& (s.flags & lt::torrent_flags::upload_mode))
		ret += "upload mode";
	else
		ret += state_str[s.state];

    if (!(s.flags & lt::torrent_flags::auto_managed))
	{
		if (s.flags & lt::torrent_flags::paused)
			ret += " [Paused]";
        else
			ret += " [Force Start]";
	}

    return _(ret.c_str());

}



// Glib::ustring activityString(int activity, bool finished)
// {
//     switch (activity)
//     {
//     case TR_STATUS_CHECK_WAIT:
//         return _("Queued for verification");

//     case TR_STATUS_CHECK:
//         return _("Verifying local data");

//     case TR_STATUS_DOWNLOAD_WAIT:
//         return _("Queued for download");

//     case TR_STATUS_DOWNLOAD:
//         return C_("Verb", "Downloading");

//     case TR_STATUS_SEED_WAIT:
//         return _("Queued for seeding");

//     case TR_STATUS_SEED:
//         return C_("Verb", "Seeding");

//     case TR_STATUS_STOPPED:
//         return finished ? _("Finished") : _("Paused");

//     default:
//         g_assert_not_reached();
//     }

//     return {};
// }

    /* Only call gtk_text_buffer_set_text () if the new text differs from the old.
    * This way if the user has text selected, refreshing won't deselect it */
    void gtr_text_buffer_set_text(Glib::RefPtr<Gtk::TextBuffer> const& b, Glib::ustring const& str)
    {
        if (b->get_text() != str)
        {
            b->set_text(str);
        }
    }

    [[nodiscard]] std::string get_date_string(time_t t)
    {
        return t == 0 ? _("N/A") : fmt::format("{:%x}", fmt::localtime(t));
    }

    [[nodiscard]] std::string get_date_time_string(time_t t)
    {
        return t == 0 ? _("N/A") : fmt::format("{:%c}", fmt::localtime(t));
    }

} //anonymous namespace

void DetailsDialog::Impl::refreshInfo(lt::torrent_status const& st)
{
    auto const now = time(nullptr);
    Glib::ustring str;
    Glib::ustring const no_torrent = _("No Torrents Selected");
    // Glib::ustring stateString;


    /* added_lb */
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        str = get_date_time_string(st.added_time);
    }
    added_lb_->set_text(str);


    /* origin_lb who and when create it*/
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        auto const& ti = st.torrent_file.lock();
        auto const& author = ti->creator();
                
        std::time_t const create_date =  ti->creation_date();
        std::string_view creator_sv = tr_strv_strip(std::string_view(author));
        std::string creator_str = std::string(creator_sv);
        auto const datestr = get_date_string(create_date);
     
        bool const empty_creator = std::empty(creator_str);
        bool const empty_date = create_date == 0;

        if (!empty_creator && !empty_date)
        {
            //dont use creator_sv here, cuz it will print weird things such as invalid UTF-8 chararacter 
            str = fmt::format(_("Created by {creator} on {date}"), fmt::arg("creator", creator_str), fmt::arg("date", datestr));
        }
        else if (!empty_creator)
        {
            str = fmt::format(_("Created by {creator}"), fmt::arg("creator", creator_str));
        }
        else if (!empty_date)
        {
            str = fmt::format(_("Created on {date}"), fmt::arg("date", datestr));
        }
        else
        {
            str = _("N/A");
        }
    }

    origin_lb_->set_text(str);


    /* comment_buffer */
    if (st.handle.is_valid()==false)
    {
        str.clear();
    }
    else
    {
        std::shared_ptr<const lt::torrent_info> ti = st.torrent_file.lock();
        std::string const comment_str = ti->comment();

        auto const baseline = Glib::ustring(!comment_str.empty() ? comment_str : "");
        str = baseline;
    }

    gtr_text_buffer_set_text(comment_buffer_, str);


    /* destination_lb */
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        auto const baseline = Glib::ustring(st.save_path.c_str());
        str = baseline;
    }

    destination_lb_->set_text(str);


    /* state_lb */
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        str =  torrent_state_txt(st);
    }

    state_lb_->set_text(str);


    /* date started */
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        std::time_t const baseline = st.added_time;

         if (baseline <= 0 || ((st.flags&lt::torrent_flags::paused) && !(st.flags & lt::torrent_flags::auto_managed)) )
        {
            str = _("Paused");
        }
        else
        {
            str = tr_format_time(now - baseline);
        }
    }

    date_started_lb_->set_text(str);


    /* eta */
    if (st.handle.is_valid() ==false)
    {
        str = no_torrent;
    }
    else
    {
        auto ti = st.torrent_file.lock();

        auto const bytes_progress = double(st.progress_ppm)	/ 1000000. * double(ti->total_size());
 
        double eta_num_sec = (st.download_rate != 0) ? ((ti->total_size() - bytes_progress) / double(st.download_rate )) : 0.0;

        if (eta_num_sec == 0.0)
        {
            str = _("Unknown");
        }
        else
        {
            auto eta_durationInMillis = std::chrono::duration_cast<milliseconds>(std::chrono::duration<double>(eta_num_sec));
            double eta_durationInsec = eta_durationInMillis.count() / 1000.0;
            std::time_t eta = static_cast<std::time_t>(eta_durationInsec);

            str = tr_format_time_left(eta);
        }
    }

    eta_lb_->set_text(str);


    /* size_lb */
    if (st.handle.is_valid() ==false)
    {
        str = no_torrent;
    }
    else
    {
        auto ti = st.torrent_file.lock();
        auto const piece_count = ti->num_pieces();
        if (piece_count == 0)
        {
            str.clear();
        }
        int const piece_len = ti->piece_length();
        auto const total_size = ti->total_size();
        
        auto const file_count = ti->num_files();
            
        str = fmt::format(
            ngettext("{total_size} in {file_count:L} file", "{total_size} in {file_count:L} files", file_count),
            fmt::arg("total_size", tr_strlsize(total_size)),
            fmt::arg("file_count", file_count));

            str += ' ';
            str += fmt::format(
                ngettext(
                    "({piece_count} BitTorrent piece @ {piece_size})",
                    "({piece_count} BitTorrent pieces @ {piece_size})",
                    piece_count),
                fmt::arg("piece_count", piece_count),
                fmt::arg("piece_size", Memory{ piece_len, Memory::Units::Bytes }.to_string()));
      
        size_lb_->set_text(str);
    }

  

    // dl_lb - total_downloaded
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        if (st.total_failed_bytes != 0)
        {
            str = fmt::format(
                _("{downloaded_size} (+{failed_size} failed)"),
                fmt::arg("downloaded_size", st.all_time_download),
                fmt::arg("failed_size", tr_strlsize(st.total_failed_bytes))
            );
        }
        else
        {
             str = fmt::format(
                _("{downloaded_size}"),
                fmt::arg("downloaded_size", st.all_time_download)
            );
        }
    }

    dl_lb_->set_text(str);


    /* ul_lb - total uploaded */
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        str = fmt::format(
            _("{uploaded_size} "),
            fmt::arg("uploaded_size", tr_strlsize(st.all_time_upload))
        );
    }

    ul_lb_->set_text(str);


    /* hash_lb */
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        str = to_hex(st.info_hashes.get());
    }
  
    hash_lb_->set_text(str);


    /* error_lb */
    if (st.handle.is_valid()==false)
    {
        str = no_torrent;
    }
    else
    {
        auto const baseline = Glib::ustring(st.errc.message());
        str = baseline;
    }

    if (str.empty())
    {
        str = _("No errors");
    }

    error_lb_->set_text(str);

}



void DetailsDialog::Impl::info_page_init(Glib::RefPtr<Gtk::Builder> const& builder)
{
    comment_buffer_ = Gtk::TextBuffer::create();
    auto* tw = gtr_get_widget<Gtk::TextView>(builder, "comment_value_view");
    tw->set_buffer(comment_buffer_);
}



/****
*****
*****  PEERS TAB ********************
*****
****/

namespace
{


//write flagStr according to peer_info struct
void parseFlags(lt::peer_info const& i,char* flagStr)
{   
    char* pch = flagStr;
    // 15 various flag Str
    if(i.flags & lt::peer_info::interesting)
    {
        *pch++ = 'I';
    }   
    if(i.flags & lt::peer_info::choked)
    {
        *pch++ = 'C';
    }   
    if(i.flags & lt::peer_info::remote_interested)
    {
        *pch++ = 'i';
    }   
    if(i.flags & lt::peer_info::remote_choked)
    {
        *pch++ = 'c';
    }   
    if(i.flags & lt::peer_info::supports_extensions)
    {
        *pch++ = 'x';
    }   
    if(i.flags & lt::peer_info::outgoing_connection)
    {
        *pch++ = 'o';
    }   
    if(i.flags & lt::peer_info::on_parole)
    {
        *pch++ = 'p';
    }   
    if(i.flags & lt::peer_info::optimistic_unchoke)
    {
        *pch++ = 'O';
    }   
    if(i.flags & lt::peer_info::snubbed)
    {
        *pch++ = 'S';
    }   
    if(i.flags & lt::peer_info::upload_only)
    {
        *pch++ = 'U';
    }   
    if(i.flags & lt::peer_info::endgame_mode)
    {
        *pch++ = 'e';
    }   
    if(i.flags & lt::peer_info::handshake)
    {
        *pch++ = 'h';
    }   
    if(i.flags & lt::peer_info::holepunched)
    {
        *pch++ = 'H';
    }   
    if(i.flags & lt::peer_info::seed)
    {
        *pch++ = 's';
    }   
    if(i.flags & lt::peer_info::utp_socket)
    {
        *pch++ = 'u';
    }   


    //read/write state -- 6
    if(i.read_state & lt::peer_info::bw_disk)
    {
        *pch++ = '1';
    }   
    if(i.read_state & lt::peer_info::bw_limit)
    {
        *pch++ = '2';
    }      
    if(i.read_state & lt::peer_info::bw_network)
    {
        *pch++ = '3';
    }   
    if(i.write_state & lt::peer_info::bw_disk)
    {
        *pch++ = '4';
    }       
    if(i.write_state & lt::peer_info::bw_limit)
    {
        *pch++ = '5';
    }   
    if(i.write_state & lt::peer_info::bw_network)
    {
        *pch++ = '6';
    }   


    //peer source -- 5
    if(i.source & lt::peer_info::tracker)
    {
        *pch++ = '7';
    } 
    if(i.source & lt::peer_info::pex)
    {
        *pch++ = '8';
    } 
    if(i.source & lt::peer_info::lsd)
    {
        *pch++ = '9';
    } 
    if(i.source & lt::peer_info::resume_data)
    {
        *pch++ = '0';
    } 
    if(i.source & lt::peer_info::incoming)
    {
        *pch++ = '<';
    } 

    *pch = '\0'; // Null-terminate the string


}  



class PeerModelColumns : public Gtk::TreeModelColumnRecord
{
public:
    PeerModelColumns() noexcept
    {
        add(key);
        add(was_updated);
        add(client);
        add(address);
        add(local_ep);
        add(download_rate_speed);
        add(download_rate_string);
        add(upload_rate_speed);
        add(upload_rate_string);
        add(down_total_size);
        add(up_total_size);
        add(progress);
        add(progress_string);
        add(block_progress);
        add(block_progress_string);
        add(flags);
        add(download_piece_idx);
        add(download_block_idx);
    }

    //not display
    Gtk::TreeModelColumn<std::string> key;

    //not display
    Gtk::TreeModelColumn<bool> was_updated;

    //display
    Gtk::TreeModelColumn<Glib::ustring> client;

    //display
    Gtk::TreeModelColumn<Glib::ustring> address;

    //display
    Gtk::TreeModelColumn<Glib::ustring> local_ep;

    //not display
    Gtk::TreeModelColumn<Speed> download_rate_speed; //used for sort

    //display
    Gtk::TreeModelColumn<Glib::ustring> download_rate_string;

    //not display
    Gtk::TreeModelColumn<Speed> upload_rate_speed; //used for sort

    //display
    Gtk::TreeModelColumn<Glib::ustring> upload_rate_string;

    //display
    Gtk::TreeModelColumn<Glib::ustring> down_total_size;

    //display
    Gtk::TreeModelColumn<Glib::ustring> up_total_size;

    //not display
    Gtk::TreeModelColumn<double> progress; //used for sort

    //display
    Gtk::TreeModelColumn<Glib::ustring> progress_string; 

    //not display
    Gtk::TreeModelColumn<double> block_progress; //used for sort

    //display
    Gtk::TreeModelColumn<Glib::ustring> block_progress_string; 

    //display
    Gtk::TreeModelColumn<Glib::ustring> flags;

    //display
    Gtk::TreeModelColumn<int> download_piece_idx;

    //display
    Gtk::TreeModelColumn<int> download_block_idx;
 


};



PeerModelColumns const peer_cols;



std::string print_endpoint(lt::tcp::endpoint const& ep)
{
	using namespace lt;
	char buf[200];
	address const& addr = ep.address();
	if (addr.is_v6())
		std::snprintf(buf, sizeof(buf), "[%s]:%d", addr.to_string().c_str(), ep.port());
	else
		std::snprintf(buf, sizeof(buf), "%s:%d", addr.to_string().c_str(), ep.port());
	return buf;
}


void initPeerRow(
    Gtk::TreeModel::iterator const& iter,
    std::string key,
    lt::peer_info const& peer)
{
    // g_return_if_fail(peer != nullptr);

    //A human readable string describing the software at the other end of the connection, such as Transmission x.x ,qBittorrent x.x.
    std::string client = peer.client;
    if (client.empty() /*|| g_strcmp0(client, "Unknown Client") == 0*/)
    {
        client = "";
    }

    //the infohash of the torrent, DetailsDialog only show info of single torrent, so thie field may be redundant, but it is not much wrong
    (*iter)[peer_cols.key] = key;
    (*iter)[peer_cols.address] = print_endpoint(peer.ip);
    (*iter)[peer_cols.local_ep] = print_endpoint(peer.local_endpoint);
    (*iter)[peer_cols.client] = client;

}

void refreshPeerRow(Gtk::TreeModel::iterator const& iter, lt::peer_info const& peer)
{

    // g_return_if_fail(peer != nullptr);



    //marl current row as *updated*, useful for removing disappeared peer row
    (*iter)[peer_cols.was_updated] = true;



    /*Download Speed Column*/
    auto const down_speed = Speed{ peer.down_speed, Speed::Units::Byps };
    auto down_speed_string = std::string{};
    if(peer.down_speed > 0)
    {
        down_speed_string = down_speed.to_string();
    }
    //the actual number, why use it? redundant? No, it can be used for sorting
    (*iter)[peer_cols.download_rate_speed] = down_speed;
     //the string to be displayed
    (*iter)[peer_cols.download_rate_string] = down_speed_string;



    /*Upload Speed Column*/
    auto const up_speed = Speed{ peer.up_speed, Speed::Units::Byps };
    auto up_speed_string = std::string{};
    if(peer.up_speed > 0)
    {
        up_speed_string = up_speed.to_string();
    }
    //the actual number, why use it? redundant? No, it can be used for sorting
    (*iter)[peer_cols.upload_rate_speed] = up_speed;
    //the string to be displayed
    (*iter)[peer_cols.upload_rate_string] = up_speed_string;



    /*Total Download Size Column*/
    auto const down_total = Storage{peer.total_download, Storage::Units::Bytes};
    auto down_total_string = down_total.to_string();
    //the string to be displayed
    (*iter)[peer_cols.down_total_size] = down_total_string;



    /*Total Upload Size Column*/
    auto const up_total = Storage{peer.total_upload, Storage::Units::Bytes};
    auto up_total_string = up_total.to_string();
    //the string to be displayed
    (*iter)[peer_cols.up_total_size] = up_total_string;



    /*FlagsStr Column*/
    char flagStr[32];
    parseFlags(peer, flagStr);

                // std::cout << "<<<falgs str is " << flagStr << std::endl;

    //the string to be displayed
    (*iter)[peer_cols.flags] = flagStr;




    /*Downloading Piece/Block Column*/
    // This may be set to -1 if there's currently no downloading from this peer
    //the int to be displayed
    (*iter)[peer_cols.download_piece_idx] = peer.downloading_piece_index;
    //the int to be displayed
    (*iter)[peer_cols.download_block_idx] = peer.downloading_block_index;



    /*Block Progress Column*/
    double blk_progress = peer.downloading_total == 0 ? 1000.0: static_cast<double>(peer.downloading_progress* 1000 / peer.downloading_total);
    blk_progress /= 10.0;
    //the actual number, why use it? redundant? No, it can be used for sorting
    (*iter)[peer_cols.block_progress] = blk_progress; 
    //only two bits after decimal point
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << blk_progress;
    //the actual number, why use it? redundant? No, it can be used for sorting
    (*iter)[peer_cols.block_progress_string] = ss.str(); 




    /*Progress Column*/
    auto prgress_tmp = static_cast<double>(peer.progress_ppm/10000.0);
    //the actual number, why use it? redundant? No, it can be used for sorting
    (*iter)[peer_cols.progress] = prgress_tmp; 
    //only two bits after decimal point
    // Clear the stringstream for reuse
    ss.str(""); // Reset the contents
    ss.clear(); // Reset the state
    ss << std::fixed << std::setprecision(2) << prgress_tmp;
    //the actual number, why use it? redundant? No, it can be used for sorting
    (*iter)[peer_cols.progress_string] = ss.str(); 


}



}//anonymous namespace


void DetailsDialog::Impl::refreshPeerList(std::vector<lt::peer_info> const& peers)
{
    auto& hash = peer_hash_;
    auto const& store = peer_store_;


    /* mark all the peers in the list as not-updated */
    for (auto& row : store->children())
    {
        row[peer_cols.was_updated] = false;
    }



    /* add any new peers */
    for (size_t j = 0; j < peers.size(); ++j)
    {
        auto& s = peers[j];
        //key represent info hash of the torrent
        std::string key = to_hex(s.pid);  
        if (hash.find(key) == hash.end())
        {
            auto const iter = store->append();
            initPeerRow(iter, key, s);
            hash.try_emplace(key, Gtk::TreeRowReference(store, store->get_path(iter)));
        }
    }



    /* update the peers */
    // for (size_t i = 0; i < torrents.size(); ++i)
    // {
        // auto const* tor = torrents.at(i);

        for (size_t j = 0; j < peers.size(); ++j)
        {

            auto& s = peers[j];
            std::string key = to_hex(s.pid);  
            refreshPeerRow(store->get_iter(hash.at(key).get_path()), s);
        }

    // }

    /*  remove peers that have disappeared */
    if (auto iter = store->children().begin(); iter)
    {
        while (iter)
        {
            if (iter->get_value(peer_cols.was_updated))
            {
                ++iter;
            }
            else
            {
                auto const key = iter->get_value(peer_cols.key);
                hash.erase(key);
                iter = store->erase(iter);
            }
        }
    }

    /*  cleanup */
    // for (size_t i = 0; i < peers.size(); ++i)
    // {
    //     tr_torrentPeersFree(peers[i], peerCount[i]);
    // }
}



void DetailsDialog::Impl::refreshPeers(std::vector<lt::peer_info> const& peers)
{
    refreshPeerList(peers);
}



bool DetailsDialog::Impl::onPeerViewQueryTooltip(int x, int y, bool keyboard_tip, Glib::RefPtr<Gtk::Tooltip> const& tooltip)
{
    Gtk::TreeModel::iterator iter;
    bool show_tip = false;

    if (peer_view_->get_tooltip_context_iter(x, y, keyboard_tip, iter))
    {
        auto const addr = iter->get_value(peer_cols.address);

        //every letters has special meaning
        auto const flagstr = iter->get_value(peer_cols.flags);

        //used for tooltip text
        /*
            eg:
                23.23.3.2   -- ip addr
                 -- each flag descrption
                I : we are interested in pieces from this peer
                i : the peer is interested in us
        */
        std::ostringstream gstr;
        gstr << /*"<b>" << "</b>\n" <<*/ addr << "\n \n";

        for (char const ch : flagstr)
        {
            char const* s = nullptr;

            switch (ch)
            {
                case '\0':
                            // std::cout << "////we got zero terminaltor" << std::endl;
                    break;

                case 'I':
                    s = _("we are interested in pieces from this peer");
                    break;

                case 'C':
                    s = _("we have choked this peer");
                    break;

                case 'i':
                    s = _("the peer is interested in us");
                    break;

                case 'c':
                    s = _("the peer has choked us");
                    break;

                case 'x':
                    s = _("the peer supports the extension protocal");
                    break;

                case 'o':
                    s = _("the connection was initiated by us");
                    break;

                case 'p':
                    s = _("the peer is now on parole");
                    break;

                case 'O':
                    s = _("the peer is subject to an optimistic unchoke");
                    break;

                case 'S':
                    s = _("the peer is snubbed by us");
                    break;

                case 'U':
                    s = _("the peer is upload-only");
                    break;

                case 'e':
                    s = _("the peer is endgame-mode");
                    break;

                case 's':
                    s = _("the peer is a seed");
                    break;

                case 'u':
                    s = _("this socket is a uTP socket");
                    break;

                case 'h':
                    s = _("the peer waiting for the handshake");
                    break;

                case 'H':
                    s = _("the peer was in holepunch mode");
                    break;

                // read\write state
                case '1':
                    s = _("the peer-read waiting for the disk I/O");
                    break;
                    
                case '2':
                    s = _("the peer-read waiting for the rate limiter");
                    break;
                case '3':
                    s = _("the peer-read waiting for a network r/w operation to complete");
                    break;
                case '4':
                    s = _("the peer-write waiting for the disk I/O");
                    break;
                case '5':
                    s = _("the peer-write waiting for the rate limiter");
                    break;
                case '6':
                    s = _("the peer-write waiting for a network r/w operation to complete");
                    break;
                    
                //peer source
                case '7':
                    s = _("the peer was received from the tracker.");
                    break;
                case '8':
                    s = _("the peer was received from Peer-Exchange");
                    break;
                case '9':
                    s = _("the peer was received from LSD");
                    break;
                case '0':
                    s = _("the peer was received from resume data");
                    break;
                case '<':
                    s = _("we received incoming connection from this peer");
                    break;

                default:
                    g_assert_not_reached();

            }

            //append tooltip to each falgs
            if (s != nullptr)
            {
                gstr << ch << ": " << s << '\n';
            }
        }

        auto str = gstr.str();
        if (!str.empty()) /* remove the last linefeed */
        {
            str.resize(str.size() - 1);
        }

        tooltip->set_markup(str);

        show_tip = true;
    }

    return show_tip;
}




namespace
{

void setPeerViewColumns(Gtk::TreeView* peer_view)
{
    //columns that we want it to display
    std::vector<Gtk::TreeModelColumnBase const*> view_columns;
    Gtk::TreeViewColumn* c = nullptr;

    //ip address of that peer
    view_columns.push_back(&peer_cols.address);

    //local endpooint we use to commnunicate with that peer
    view_columns.push_back(&peer_cols.local_ep);

    //str that represent the overall progress
    view_columns.push_back(&peer_cols.progress_string);

    //str that represent the progress of current block we're downloading
    view_columns.push_back(&peer_cols.block_progress_string);

    //total uploaded size to us
    view_columns.push_back(&peer_cols.up_total_size);

    //total downloaded size from us
    view_columns.push_back(&peer_cols.down_total_size);

    //the current piece index we're downloading
    view_columns.push_back(&peer_cols.download_piece_idx);

    //the current block index we're downloading
    view_columns.push_back(&peer_cols.download_block_idx);

    //Upload rate
    view_columns.push_back(&peer_cols.download_rate_string);

    //Download rate
    view_columns.push_back(&peer_cols.upload_rate_string);

    //client such as Transmission,qBittorrent
    view_columns.push_back(&peer_cols.client);

    //special flags, every bit has special meaning of this peer
    view_columns.push_back(&peer_cols.flags);




    /* remove any existing columns */
    peer_view->remove_all_columns();
    //iterate columns which should be displayed
    for (auto const* const col : view_columns)
    {
        auto const* sort_col = col;

        if (*col == peer_cols.address)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("Adr"), *r);
            c->add_attribute(r->property_text(), *col);
        }
        else if (*col == peer_cols.local_ep)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("Ep"), *r);
            c->add_attribute(r->property_text(), *col);
        }
        else if (*col == peer_cols.progress_string)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererProgress>();
            // % is percent done
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("%"), *r);
            c->add_attribute(r->property_text(), *col);
            sort_col = &peer_cols.progress; //sort 

        }
        else if (*col == peer_cols.block_progress_string)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererProgress>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("blk%"), *r);
            c->add_attribute(r->property_text(), *col);
            sort_col = &peer_cols.block_progress; //sort 

        }
      
        else if (*col == peer_cols.up_total_size)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("UpSz"), *r);
            c->add_attribute(r->property_text(), *col);
        }
        else if (*col == peer_cols.down_total_size)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("DnSz"), *r);
            c->add_attribute(r->property_text(), *col);
        }

        else if (*col == peer_cols.download_piece_idx)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("PIdx"), *r);
            c->add_attribute(r->property_text(), *col);
        }

        else if (*col == peer_cols.download_block_idx)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("BIdx"), *r);
            c->add_attribute(r->property_text(), *col);
        }

        else if (*col == peer_cols.download_rate_string)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            r->property_xalign() = 1.0F;
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("Dn"), *r);
            c->add_attribute(r->property_text(), *col);
            sort_col = &peer_cols.download_rate_speed; //sort 
        }
        else if (*col == peer_cols.upload_rate_string)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            r->property_xalign() = 1.0F;
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("Up"), *r);
            c->add_attribute(r->property_text(), *col);
            sort_col = &peer_cols.upload_rate_speed; //sort
        }
        else if (*col == peer_cols.client)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("Cli"), *r);
            c->add_attribute(r->property_text(), *col);
        }
        else if (*col == peer_cols.flags)
        {
            auto* r = Gtk::make_managed<Gtk::CellRendererText>();
            c = Gtk::make_managed<Gtk::TreeViewColumn>(_("Flg"), *r);
            c->add_attribute(r->property_text(), *col);
        }
        else
        {
            std::abort();
        }

        c->set_resizable(false);
        c->set_sort_column(*sort_col);
        peer_view->append_column(*c);
    }

    /* the 'expander' column has a 10-pixel margin on the left
       that doesn't look quite correct in any of these columns...
       so create a non-visible column and assign it as the
       'expander column. */
    c = Gtk::make_managed<Gtk::TreeViewColumn>();
    c->set_visible(false);
    peer_view->append_column(*c);
    peer_view->set_expander_column(*c);
}

} // anonymous namespace


void DetailsDialog::Impl::peer_page_init(Glib::RefPtr<Gtk::Builder> const& /*builder*/)
{

    /* peers */
    peer_store_ = Gtk::ListStore::create(peer_cols);
    auto m = Gtk::TreeModelSort::create(peer_store_);
    m->set_sort_column(peer_cols.progress, TR_GTK_SORT_TYPE(DESCENDING));

    peer_view_->set_model(m);
    peer_view_->set_has_tooltip(true);
    peer_view_->signal_query_tooltip().connect(sigc::mem_fun(*this, &Impl::onPeerViewQueryTooltip), false);
    setup_item_view_button_event_handling(
        *peer_view_,
        {},
        [this](double view_x, double view_y) { return on_item_view_button_released(*peer_view_, view_x, view_y); });

    setPeerViewColumns(peer_view_);


}





/****
*****
*****  TRACKER **********************
*****
****/

namespace
{

auto constexpr ErrMarkupBegin = "<span color='red'>"sv;
auto constexpr ErrMarkupEnd = "</span>"sv;
auto constexpr LastWorkingMarkupBegin = "<span color='#246'>"sv;//dark blue
auto constexpr LastWorkingMarkupEnd = "</span>"sv;
auto constexpr SuccessMarkupBegin = "<span color='#080'>"sv; //dark green
auto constexpr SuccessMarkupEnd = "</span>"sv;

std::array<std::string_view, 3> const text_dir_mark = { ""sv, "\u200E"sv, "\u200F"sv };


void appendTrackerDetail(lt::announce_entry const& tracker, Gtk::TextDirection direction, std::ostream& gstr)
{
    auto const dir_mark = text_dir_mark.at(static_cast<int>(direction));
    std::uint8_t idx = 0;
    int scrape_seeder = -1;
    int scrape_leecher = -1;
    int scrape_download_times = -1;
    bool has_scrape_info = {};


    //each announce endpoionts within this announce entry
    for (auto const& ep : tracker.endpoints)
    {
        gstr << '\n';
        gstr << dir_mark;

        if (!ep.enabled) continue;
        auto const& av = ep.info_hashes[lt::protocol_version::V1];


        auto const next_announce_countdown = lt::total_seconds(av.next_announce - lt::clock_type::now());
        auto const last_err_msg = av.last_error ? av.last_error.message() : "";
        gstr << fmt::format(
            // {markup_begin} and {markup_end} should surround the peer text
            _("[{index}] time to next annouce: {next_announce} (sec)  , {Errmarkup_strat}\"{last_err_msg}\"{Errmarkup_end}"),
                fmt::arg("index", idx++),
                fmt::arg("next_announce", next_announce_countdown),
                fmt::arg("last_err_msg", last_err_msg),
                fmt::arg("Errmarkup_strat", ErrMarkupBegin),
                fmt::arg("Errmarkup_end", ErrMarkupEnd)
        );


        //scrape_incomplete - current downloader
        //scrape_complete - current seeder
        //scrape_downloaded - cumulative number of completed downloads
        if(av.scrape_incomplete > -1 || av.scrape_complete > -1 || av.scrape_downloaded > -1)
        {
            has_scrape_info = true;
            scrape_seeder = av.scrape_incomplete;
            scrape_leecher = av.scrape_complete;
            scrape_download_times = av.scrape_downloaded;
        }
    }

    if(has_scrape_info)
    {
        gstr << '\n';
        gstr << dir_mark;

        gstr << fmt::format(
        // {markup_begin} and {markup_end} should surround the seeder/leecher text
        //0 seeders and 0 leechers 1 minute ago
        _("Tracker Have {markup_begin}{seeder_count} {seeder_or_seeders} and {leecher_count} {leecher_or_leechers}{markup_end},it has been downloaded {total_downloaded_time} times"),
        fmt::arg("seeder_count", scrape_seeder),
        fmt::arg("seeder_or_seeders", ngettext("seeder", "seeders", scrape_seeder)),
        fmt::arg("leecher_count", scrape_leecher),
        fmt::arg("leecher_or_leechers", ngettext("leecher", "leechers", scrape_leecher)), 
        fmt::arg("total_downloaded_time", scrape_download_times),
        fmt::arg("markup_begin", SuccessMarkupBegin),
        fmt::arg("markup_end", SuccessMarkupEnd)
        );

    }


}






void buildTrackerSummary(
    std::ostream& gstr,
    lt::announce_entry const& tracker,
    Gtk::TextDirection direction,
    bool is_cur_tracker
    )
{

    gstr << text_dir_mark.at(static_cast<int>(direction));

    //find whether this annouce_entry is already announced (aka. consumed)
    auto const& aes = tracker.endpoints;
    auto it = std::find_if(aes.begin(), aes.end(), [](lt::announce_endpoint const& ae){ return ae.info_hashes[lt::protocol_version::V1].is_working();});
    bool tried_already = it != aes.end();
	// we've succeeded to announce to this tracker 
    if(tried_already)
    {
        //bold means already consumed
        gstr <<  "<b>";
    }
    //we haven't tried this tracker yet
    else
    {
        //italic means not tried yet
        gstr <<  "<i>";
    }

    auto url_pased_ret = tr_urlParse(tracker.url);
    std::string_view host = url_pased_ret ? url_pased_ret->host : "";

    if(is_cur_tracker)
    {
        gstr << Glib::Markup::escape_text(
        fmt::format(_("{markup_begin}{:d}  {:s}    {:s}{markup_end}"), tracker.tier, host, tracker.verified? "OK" : "",
            fmt::arg("markup_begin", LastWorkingMarkupBegin),
            fmt::arg("markup_end", LastWorkingMarkupEnd) 
        )
        );
    }
    else
    {
        gstr << Glib::Markup::escape_text(
            fmt::format("{:d}  {:s}    {:s}", tracker.tier, host, tracker.verified? "OK" : "")
        );

    }


    if(tried_already)
    {
        gstr << "</b>";
    }
    else
    {
        gstr <<  "</i>";
    }

    appendTrackerDetail(tracker, direction, gstr);

}



class TrackerModelColumns : public Gtk::TreeModelColumnRecord
{
public:
    TrackerModelColumns() noexcept
    { 
        add(text);
        add(tracker_tier);
        add(favicon);
        add(was_updated);
        add(key);
    }


    Gtk::TreeModelColumn<Glib::ustring> text;  
    Gtk::TreeModelColumn<uint8_t> tracker_tier;
    Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> favicon;
    Gtk::TreeModelColumn<bool> was_updated;
    Gtk::TreeModelColumn<std::string> key;
};

TrackerModelColumns const tracker_cols;

} // anonymous namespace



// lt::torrent_handle DetailsDialog::Impl::tracker_list_get_current_torrent() const
// {
//     return core_->find_torrent(uniq_id_);
// }


namespace
{

void favicon_ready_cb(Glib::RefPtr<Gdk::Pixbuf> const* pixbuf, Gtk::TreeRowReference& reference)
{
    if (pixbuf != nullptr && *pixbuf != nullptr)
    {
        auto const path = reference.get_path();
        auto const model = reference.get_model();

        if (auto const iter = model->get_iter(path); iter)
        {
            (*iter)[tracker_cols.favicon] = *pixbuf;
        }
    }
}

} //anonymous namespace

void DetailsDialog::Impl::refreshTracker(std::vector<lt::announce_entry> const& trackers, std::string const& cur_tracker)
{
    std::ostringstream gstr;
    auto& hash = tracker_hash_;
    auto const& store = tracker_store_;
    

    /* mark all the trackers in the list as not-updated */
    for (auto& row : store->children())
    {
        row[tracker_cols.was_updated] = false;
    }


    /*  add / update trackers */
    for (lt::announce_entry  const& tracker : trackers)
    {           

                // std::cout << "after Add/replace tracker: " << tracker.url << std::endl;


        // build the key to find the row
        gstr.str({});
        
        gstr << tracker.url;
        if (hash.find(gstr.str()) == hash.end())
        {
            // if we didn't have that row, add it
            auto const iter = store->append();
        
            (*iter)[tracker_cols.tracker_tier] = tracker.tier;
            (*iter)[tracker_cols.key] = gstr.str();

            auto const p = store->get_path(iter);
            hash.try_emplace(gstr.str(), Gtk::TreeRowReference(store, p));
            core_->favicon_cache().load(
                tracker.url,
                [ref = Gtk::TreeRowReference(store, p)](auto const* pixbuf_refptr) mutable
                { favicon_ready_cb(pixbuf_refptr, ref); });
        }
    }


    /* update the rows */
    for (auto const&tracker : trackers)
    {
        // build the key to find the row
        gstr.str({});
        gstr << tracker.url;
        auto const iter = store->get_iter(hash.at(gstr.str()).get_path());

        // update the row
        gstr.str({});
        bool is_current_tracker = {};
        if(!cur_tracker.empty())
        {
            is_current_tracker =  cur_tracker == tracker.url;
        }
        buildTrackerSummary(gstr, tracker, dialog_.get_direction(), is_current_tracker);


        (*iter)[tracker_cols.text] = gstr.str();
        (*iter)[tracker_cols.tracker_tier] = tracker.tier;
        (*iter)[tracker_cols.was_updated] = true;
    }




    /* 
        remove trackers that have disappeared , for example, we call torrent_handle::replace_tracker(), 
        although the EditTrackerDialog::Entry will copy the original tracker list
        and we must delete original tracker row which is not list in our new trakcer list Entry

        eg.
            original tracker list:
                    http://tracker.etree.org:6969/announce
                    udp://bt.firebit.org:2710/announce
                    udp://tracker.internetwarriors.net:1337
                    http://tracker2.itzmx.com:6961/announce
            we input in EditTrackerDualog's Entry
                    http://aaa.bbb.ccc:6969/announce
                    http://aaa.bbb.ddd:6969/announce

                    http://aaa.bbb.eee:6969/announce
                    http://aaa.bbb.fff:6969/announce
            the oringnal all 4 tracker row must be cleard, cuz they are not in our new list 
        
    */

    if (auto iter = store->children().begin(); iter)
    {
        while (iter)
        {
            if (iter->get_value(tracker_cols.was_updated))
            {
                ++iter;
            }
            else
            {
                auto const key = iter->get_value(tracker_cols.key);
                hash.erase(key);
                iter = store->erase(iter);
            }
        }
    }

    //REDUNDANT 
    // edit_trackers_button_->set_sensitive(uniq_id_ != std::uint32_t{});
}




//FileList 
void DetailsDialog::Impl::refreshFiles()
{
    //only if we have metadata, can we show the FileList
    if(uniq_id_ != std::uint32_t{} && handle_regular_.is_valid() && handle_regular_.torrent_file())
    {
        file_list_->set_torrent();
        file_list_->show();
        file_label_->hide();
    }

    else
    {
        file_list_->clear();
        file_list_->hide();
        file_label_->show();
    }
}



namespace
{



//According to TrackerDialog Tracker List Entry content,  build a  vector of announce_entry 
std::vector<lt::announce_entry> url_txt_to_announce_entry_vec(std::string_view text)
{   
    std::vector<lt::announce_entry> aes_copy;
        
    
    auto current_tier = std::uint8_t{ 0 };
    auto line = std::string_view{};

    //In EditTracker Entry : *000 url following a blank line means next tier, no blank line means same tier 000*
    /* 
    In Linux, you typically don't encounter '\r' (carriage return) unless the file originated from a system like Windows, which uses "\r\n" for line endings.
        Linux/Unix: Uses \n (newline) as the line ending character.
        Windows: Uses \r\n (carriage return + newline) as the line ending.
    */
    while (tr_strv_sep(&text, &line, '\n'))
    {
                    // std::cout << "line : " << line << std::endl;
        

        if (tr_strv_ends_with(line, '\r'))
        {
            line = line.substr(0, std::size(line) - 1); 
        }

        //remove leading or trailing space
        line = tr_strv_strip(line);

        //if this line is empty, means we increase the current_tier
        if (std::empty(line))
        {
            ++current_tier;
         
        }
        //otherwise, insert this line of same tier
        else
        {
            lt::announce_entry ae{};
            ae.url = std::string(line);
            ae.tier = current_tier;
            aes_copy.push_back(ae);
        }    
    }

    return aes_copy;
}



/*

--tier 1: http://sukebei.tracker.wf:8888/announce
--tier 1: udp://tracker.openbittorrent.com:6969

--tier 2: udp://tracker.leech.ie:1337/announce

*/
std::string announce_entry_vec_to_txt(std::vector<lt::announce_entry> const& vec)
{

    auto text = std::string{};
    auto current_tier = std::optional<std::uint8_t>{};

    for (auto const& trackerAE : vec)
    {
        if (current_tier && *current_tier != trackerAE.tier)
        {
            text += '\n';
        }

        text += trackerAE.url;
        text += '\n';

        current_tier = trackerAE.tier;
    }
    return text;
}


bool is_valid_url_txt(std::string_view text)
{
    // auto current_tier = std::uint8_t{ 0 };
    // auto current_tier_size = std::size_t{ 0 };
    auto line = std::string_view{};
    while (tr_strv_sep(&text, &line, '\n'))
    {
        //same tier
        if (tr_strv_ends_with(line, '\r'))
        {
            line = line.substr(0, std::size(line) - 1);
          
        }

        line = tr_strv_strip(line);

        if (std::empty(line))
        {
            continue;
        }
        else if (!tr_urlIsValidTracker(line))
        {
            return false;
        }

   
    }

    return true;

}



/*******************************************
 ************Edit Tracker Dialog************
 *******************************************/
class EditTrackersDialog : public Gtk::Dialog
{
public:
    EditTrackersDialog(
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const& builder,
        DetailsDialog& parent,
        Glib::RefPtr<Session> const& core,
        lt::torrent_handle const& handle_ref,
        std::uint32_t uniqid
    
    );

    ~EditTrackersDialog() override /*= default*/;

    TR_DISABLE_COPY_MOVE(EditTrackersDialog)

    static std::unique_ptr<EditTrackersDialog> create(
        DetailsDialog& parent,
        Glib::RefPtr<Session> const& core,
        lt::torrent_handle const& handle_ref,
        std::uint32_t uniqid
    );

private:
    void on_response(int response) override;

private:
    DetailsDialog& parent_;

    Glib::RefPtr<Session> const core_;

    //for single torrent
    std::uint32_t uniq_id_;

    lt::torrent_handle const& handle_ref_;

    Gtk::TextView* const urls_view_;
};

std::unique_ptr<EditTrackersDialog> EditTrackersDialog::create
(
    DetailsDialog& parent,
    Glib::RefPtr<Session> const& core,
    lt::torrent_handle const& handle_ref,
    std::uint32_t uniqid
)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("EditTrackersDialog.ui"));
    return std::unique_ptr<EditTrackersDialog>(
        gtr_get_widget_derived<EditTrackersDialog>(builder, "EditTrackersDialog", parent, core, handle_ref, uniqid));
}

EditTrackersDialog::~EditTrackersDialog()
{
                            std::cout << "EditTrackersDialog::~EditTrackersDialog() " << std::endl;
}

EditTrackersDialog::EditTrackersDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    DetailsDialog& parent,
    Glib::RefPtr<Session> const& core,
    lt::torrent_handle const& handle_ref,
    std::uint32_t uniqid
    )
    : Gtk::Dialog(cast_item)
    , parent_(parent)
    , core_(core)
    , uniq_id_(uniqid)
    , handle_ref_(handle_ref)
    , urls_view_(gtr_get_widget<Gtk::TextView>(builder, "urls_view"))
{
    set_title(fmt::format(_(" Edit Trackers") ));
    set_transient_for(parent);

    Glib::RefPtr<Torrent> const& Tor = core_->find_Torrent(uniq_id_);
    if(Tor->is_Tor_valid())
    {
        auto& trackers = Tor->get_trackers_vec_ref();
        std::string urls_view_text = announce_entry_vec_to_txt(trackers);
        urls_view_->get_buffer()->set_text(urls_view_text);
    }
}


void EditTrackersDialog::on_response(int response)
{
    bool do_destroy = true;

    if (response == TR_GTK_RESPONSE_TYPE(ACCEPT))
    {
        auto const text_buffer = urls_view_->get_buffer();

        //DetailsDialog must attached to a valid torrent(torrent_handle)
        if (handle_ref_.is_valid())
        {
            //we must check validity of trackers list before we apply torrent_handle::replace_trackers()
            if (is_valid_url_txt(text_buffer->get_text(false).c_str()))
            {
                //In EditTracker Entry : url following a blank line means next tier, no blank line means same tier
                auto aes = url_txt_to_announce_entry_vec(text_buffer->get_text(false).c_str());


                        // std::cout << "********in EditTracker on_response***********" << std::endl;
                        // for(lt::announce_entry ae: aes)
                        // {
                        //     std::uint8_t tr = ae.tier;
                        //     //must static cast, or it may print nothing
                        //     std::cout << static_cast<int>(tr) << std::endl;
                        //     std::cout << ": " << ae.url << std::endl;
                        // }
                        // std::cout << "********in EditTracker on_response DOT***********" << std::endl;


                handle_ref_.replace_trackers(aes);
                handle_ref_.save_resume_data(lt::torrent_handle::if_metadata_changed);
                core_->inc_num_outstanding_resume_data();
            }
            else
            {
                auto w = std::make_shared<Gtk::MessageDialog>(
                    *this,
                    _("Tracker List contains invalid URLs"),
                    false,
                    TR_GTK_MESSAGE_TYPE(ERROR),
                    TR_GTK_BUTTONS_TYPE(CLOSE),
                    true);
                w->set_secondary_text(_("Please correct the errors and try again."));
                w->signal_response().connect([w](int /*response*/) mutable { w.reset(); });
                w->show();

                do_destroy = false;
            }
        }
    }

    if (do_destroy)
    {
        close();
    }
}

} //anonymous namespace


void DetailsDialog::Impl::on_edit_trackers()
{
    if (handle_regular_.is_valid())
    {
        auto d = std::shared_ptr<EditTrackersDialog>(EditTrackersDialog::create(dialog_, core_, handle_regular_, uniq_id_));
        gtr_window_on_close(*d, [d]() mutable { d.reset(); });
        d->show();
    }
}


void DetailsDialog::Impl::on_tracker_list_selection_changed()
{
    //REDUNDANT
    // int const n = tracker_view_->get_selection()->count_selected_rows();
    // auto const& tor = tracker_list_get_current_torrent();


    add_tracker_button_->set_sensitive(handle_regular_.is_valid());
    edit_trackers_button_->set_sensitive(handle_regular_.is_valid());
}



/*****************************************
 ***********ADD TRACKER DIALOG************
 *****************************************/
namespace
{

class AddTrackerDialog : public Gtk::Dialog
{
public:
    AddTrackerDialog(
        BaseObjectType* cast_item,
        Glib::RefPtr<Gtk::Builder> const& builder,
        DetailsDialog& parent,
        Glib::RefPtr<Session> const& core,
        lt::torrent_handle const& handle,
        std::uint32_t uniq_id
    );
    ~AddTrackerDialog() override /*= default*/;

    TR_DISABLE_COPY_MOVE(AddTrackerDialog)

    static std::unique_ptr<AddTrackerDialog> create(
        DetailsDialog& parent,
        Glib::RefPtr<Session> const& core,
        lt::torrent_handle const& handle,
        std::uint32_t uniq_id
        );

private:
    void on_response(int response) override;

private:
    DetailsDialog& parent_;
    Glib::RefPtr<Session> const core_;
    lt::torrent_handle const& handle_ref_;
    std::uint32_t uniq_id_;
    //Tracker List we want to apply
    Gtk::Entry* const url_entry_;
};

AddTrackerDialog::~AddTrackerDialog()
{
            std::cout << "AddTrackerDialog::~AddTrackerDialog() " << std::endl;
}

AddTrackerDialog::AddTrackerDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    DetailsDialog& parent,
    Glib::RefPtr<Session> const& core,
    lt::torrent_handle const& handle,
    std::uint32_t uniq_id
     )
    : Gtk::Dialog(cast_item)
    , parent_(parent)
    , core_(core)
    , handle_ref_(handle)
    , uniq_id_(uniq_id)
    , url_entry_(gtr_get_widget<Gtk::Entry>(builder, "url_entry"))
{
    set_title(fmt::format(_(" Add Tracker")));
    set_transient_for(parent);

    gtr_paste_clipboard_url_into_entry(*url_entry_);
}

std::unique_ptr<AddTrackerDialog> AddTrackerDialog::create(
    DetailsDialog& parent,
    Glib::RefPtr<Session> const& core,
    lt::torrent_handle const& handle,
    std::uint32_t uniq_id
)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("AddTrackerDialog.ui"));
    return std::unique_ptr<AddTrackerDialog>(
        gtr_get_widget_derived<AddTrackerDialog>(builder, "AddTrackerDialog", parent, core, handle, uniq_id));
}

void AddTrackerDialog::on_response(int response)
{
    bool destroy = true;

    if (response == TR_GTK_RESPONSE_TYPE(ACCEPT))
    {
        //get content of the text entry
        auto const url = gtr_str_strip(url_entry_->get_text());

        //before apply torrent_handle::add_tracker()
        if (!url.empty() && handle_ref_.is_valid())
        {
            Glib::RefPtr<Torrent> const& Tor = core_->find_Torrent(uniq_id_);
            //check validity of tracker url
            if (tr_urlIsValidTracker(url.c_str()) && Tor->is_Tor_valid())
            {

                // iterate to last tracker, 
                // such as the original trakcer list tier is:0 , 1 , 2
                // the newly inserted tier is 2+1 = 3
                auto& trackers = Tor->get_trackers_vec_ref();
                std::uint8_t lastTier = 0;
                for(auto t : trackers)
                {
                    if(t.tier > lastTier)
                        lastTier = t.tier;
                }


                auto ae = lt::announce_entry(url.c_str());
                ae.tier = lastTier;

                // If it's not in the current set of trackers, it will insert it in the tier specified in the announce_entry.
                handle_ref_.add_tracker(ae);
                
                handle_ref_.save_resume_data(lt::torrent_handle::if_metadata_changed);
                core_->inc_num_outstanding_resume_data();

                parent_.refresh();
            }
            else
            {
                gtr_unrecognized_url_dialog(*this, url);
                destroy = false;
            }
        }
    }

    if (destroy)
    {
        close();
    }
}


} //anonymous namespace



void DetailsDialog::Impl::on_tracker_list_add_button_clicked()
{
    if ( handle_regular_.is_valid())
    {
        auto d = std::shared_ptr<AddTrackerDialog>(AddTrackerDialog::create(dialog_, core_, handle_regular_, uniq_id_));
        gtr_window_on_close(*d, [d]() mutable { d.reset(); });
        d->show();
    }
}



void DetailsDialog::Impl::tracker_page_init(Glib::RefPtr<Gtk::Builder> const& /*builder*/)
{
    int const pad = (GUI_PAD + GUI_PAD_BIG) / 2;

    tracker_store_ = Gtk::ListStore::create(tracker_cols);


    //NOT USED
    // trackers_filtered_ = Gtk::TreeModelFilter::create(tracker_store_);
    // trackers_filtered_->set_visible_func(sigc::mem_fun(*this, &Impl::trackerVisibleFunc));
    // tracker_view_->set_model(trackers_filtered_);


    tracker_view_->set_model(tracker_store_);

    


    setup_item_view_button_event_handling(
        *tracker_view_,
        [this](guint /*button*/, TrGdkModifierType /*state*/, double view_x, double view_y, bool context_menu_requested)
        { return on_item_view_button_pressed(*tracker_view_, view_x, view_y, context_menu_requested); },
        [this](double view_x, double view_y) { return on_item_view_button_released(*tracker_view_, view_x, view_y); });

    auto sel = tracker_view_->get_selection();
    sel->signal_changed().connect(sigc::mem_fun(*this, &Impl::on_tracker_list_selection_changed));

    auto* c = Gtk::make_managed<Gtk::TreeViewColumn>();
    c->set_title(_("Trackers"));
    tracker_view_->append_column(*c);

    {
        auto* r = Gtk::make_managed<Gtk::CellRendererPixbuf>();
        r->property_width() = 20 + (GUI_PAD_SMALL * 2);
        r->property_xpad() = GUI_PAD_SMALL;
        r->property_ypad() = pad;
        r->property_yalign() = 0.0F;
        c->pack_start(*r, false);
        c->add_attribute(r->property_pixbuf(), tracker_cols.favicon);
    }

    {
        auto* r = Gtk::make_managed<Gtk::CellRendererText>();
        r->property_ellipsize() = TR_PANGO_ELLIPSIZE_MODE(END);
        r->property_xpad() = GUI_PAD_SMALL;
        r->property_ypad() = pad;
        c->pack_start(*r, true);
        c->add_attribute(r->property_markup(), tracker_cols.text);
    }

    add_tracker_button_->signal_clicked().connect(sigc::mem_fun(*this, &Impl::on_tracker_list_add_button_clicked));
    edit_trackers_button_->signal_clicked().connect(sigc::mem_fun(*this, &Impl::on_edit_trackers));
}





/****
*****  DIALOG
****/

void DetailsDialog::Impl::refresh()
{
    auto const tor_status_copy = getTorrentsStatus();

    /*Torrent has been removed from Session::Impl::m_all_torrents_, 
    its internal torrent_handle or torrent_status will also become invalid after a few moments (not instantly)
    so we need to close the DetailsDialog here*/
    if (!tor_status_copy.handle.is_valid())
    {   
        dialog_.response(TR_GTK_RESPONSE_TYPE(CLOSE));
    }




    refreshInfo(tor_status_copy);



    refreshFiles();



    auto const peers_copy = getPeersInfo();
    refreshPeers(peers_copy);




    auto const trackers_copy = getTrackersInfo();
    // If tracker request has not been successful yet  aka. always err message , this field will be an empty string
    std::string const& cur_tracker = tor_status_copy.current_tracker; /* last working tracker */
        // std::cout << "cur tracker : " << cur_tracker << std::endl;
    refreshTracker(trackers_copy, cur_tracker);




    refreshOptions();

}

void DetailsDialog::Impl::on_details_window_size_allocated()
{
    int w = 0;
    int h = 0;
#if GTKMM_CHECK_VERSION(4, 0, 0)
    dialog_.get_default_size(w, h);
#else
    dialog_.get_size(w, h);
#endif

    torrent_set_int(lt::settings_pack::TR_KEY_details_window_width, w);
    torrent_set_int(lt::settings_pack::TR_KEY_details_window_height, h);

}

DetailsDialog::Impl::~Impl()
{
                    std::cout << "DetailsDialog::Impl::~Impl()" << std::endl;
    periodic_refresh_tag_.disconnect();
}

std::unique_ptr<DetailsDialog> DetailsDialog::create(Gtk::Window& parent, Glib::RefPtr<Session> const& core, lt::torrent_handle && handle_rvalue)
{
    auto const builder = Gtk::Builder::create_from_resource(gtr_get_full_resource_path("DetailsDialog.ui"));
    return std::unique_ptr<DetailsDialog>(gtr_get_widget_derived<DetailsDialog>(builder, "DetailsDialog", parent, core, std::move(handle_rvalue)));
}

DetailsDialog::DetailsDialog(
    BaseObjectType* cast_item,
    Glib::RefPtr<Gtk::Builder> const& builder,
    Gtk::Window& parent,
    Glib::RefPtr<Session> const& core,
    lt::torrent_handle && handle_rvalue
    )
    : Gtk::Dialog(cast_item)
    , impl_(std::make_unique<Impl>(*this, builder, core, std::move(handle_rvalue)))
{
    set_transient_for(parent);
}

DetailsDialog::~DetailsDialog()/* = default;*/
{
                    std::cout << "DetailsDialog::~DetailsDialog()" << std::endl;

}

DetailsDialog::Impl::Impl(DetailsDialog& dialog, Glib::RefPtr<Gtk::Builder> const& builder, Glib::RefPtr<Session> const& core, lt::torrent_handle && handle_rvalue)
    : dialog_(dialog)
    , core_(core)

    , handle_regular_(std::move(handle_rvalue))

    , up_limit_check_(gtr_get_widget<Gtk::CheckButton>(builder, "up_limit_check"))
    , up_limit_spin_(gtr_get_widget<Gtk::SpinButton>(builder, "up_limit_spin"))

    , down_limit_check_(gtr_get_widget<Gtk::CheckButton>(builder, "down_limit_check"))
    , down_limit_spin_(gtr_get_widget<Gtk::SpinButton>(builder, "down_limit_spin"))

    , bandwidth_spin_(gtr_get_widget<Gtk::SpinButton>(builder, "priority_spin"))    

    , max_peers_check_(gtr_get_widget<Gtk::CheckButton>(builder, "max_peers_enabled_check"))
    , max_peers_spin_(gtr_get_widget<Gtk::SpinButton>(builder, "max_peers_spin"))



    , added_lb_(gtr_get_widget<Gtk::Label>(builder, "added_value_label"))
    , size_lb_(gtr_get_widget<Gtk::Label>(builder, "torrent_size_value_label"))
    , state_lb_(gtr_get_widget<Gtk::Label>(builder, "state_value_label"))
    , dl_lb_(gtr_get_widget<Gtk::Label>(builder, "downloaded_value_label"))
    , ul_lb_(gtr_get_widget<Gtk::Label>(builder, "uploaded_value_label"))
    , error_lb_(gtr_get_widget<Gtk::Label>(builder, "error_value_label"))
    , date_started_lb_(gtr_get_widget<Gtk::Label>(builder, "running_time_value_label"))
    , eta_lb_(gtr_get_widget<Gtk::Label>(builder, "remaining_time_value_label"))
    , hash_lb_(gtr_get_widget<Gtk::Label>(builder, "hash_value_label"))
    , origin_lb_(gtr_get_widget<Gtk::Label>(builder, "origin_value_label"))
    , destination_lb_(gtr_get_widget<Gtk::Label>(builder, "location_value_label"))


    // , more_peer_details_check_(gtr_get_widget<Gtk::CheckButton>(builder, "more_peer_details_check"))
    // , replace_tracker_button_(gtr_get_widget<Gtk::Button>(builder, "replace_tracker_button"))
    // , webseed_view_(gtr_get_widget<Gtk::ScrolledWindow>(builder, "webseeds_view_scroll"))
    // , scrape_check_(gtr_get_widget<Gtk::CheckButton>(builder, "more_tracker_details_check"))
    // , all_check_(gtr_get_widget<Gtk::CheckButton>(builder, "backup_trackers_check"))


    , peer_view_(gtr_get_widget<Gtk::TreeView>(builder, "peers_view"))
    , add_tracker_button_(gtr_get_widget<Gtk::Button>(builder, "add_tracker_button"))
    , edit_trackers_button_(gtr_get_widget<Gtk::Button>(builder, "edit_tracker_button"))
    , tracker_view_(gtr_get_widget<Gtk::TreeView>(builder, "trackers_view"))
    , file_list_(gtr_get_widget_derived<FileList>(builder, "files_view_scroll", "files_view", core, handle_regular_))
    , file_label_(gtr_get_widget<Gtk::Label>(builder, "files_label"))
{

    uniq_id_ = handle_regular_.id();

    /* return saved window size though lt::settings_pack */
    auto const width = core_->get_sett().get_int(settings_pack::TR_KEY_details_window_width);
    auto const height = core_->get_sett().get_int(settings_pack::TR_KEY_details_window_height);

#if GTKMM_CHECK_VERSION(4, 0, 0)
    dialog_.set_default_size(width, height);
    dialog_.property_default_width().signal_changed().connect(sigc::mem_fun(*this, &Impl::on_details_window_size_allocated));
    dialog_.property_default_height().signal_changed().connect(sigc::mem_fun(*this, &Impl::on_details_window_size_allocated));
#else
    dialog_.resize(width, height);
    dialog_.signal_size_allocate().connect(sigc::hide<0>(sigc::mem_fun(*this, &Impl::on_details_window_size_allocated)));
#endif

    dialog_.signal_response().connect(sigc::hide<0>(sigc::mem_fun(dialog_, &DetailsDialog::close)));

    info_page_init(builder);
    peer_page_init(builder);
    tracker_page_init(builder);
    options_page_init(builder);

    periodic_refresh_tag_ = Glib::signal_timeout().connect_seconds(
        [this]() {  refresh(); return true; },
        SECONDARY_WINDOW_REFRESH_INTERVAL_SECONDS);

    auto* const n = gtr_get_widget<Gtk::Notebook>(builder, "dialog_pages");
    n->set_current_page(last_page_);
    n->signal_switch_page().connect([](Gtk::Widget* /*page*/, guint page_number) { last_page_ = page_number; });



    /*set title*/
    Glib::ustring title;

    title = fmt::format(_("{torrent_name} Properties"), fmt::arg("torrent_name", handle_regular_.torrent_file()->name()));

    dialog_.set_title(title);

    refresh();
}


void DetailsDialog::refresh()
{
    impl_->refresh();
}

