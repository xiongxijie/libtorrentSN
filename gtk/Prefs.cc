// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#include "Prefs.h"
#include "GtkCompat.h"
#include "PrefsDialog.h"
#include "tr-transmission.h"
#include "tr-utils.h"
#include "tr-strbuf.h"
#include "tr-log.h"
#include "tr-error.h"
#include "tr-file.h"



#include <glibmm/miscutils.h>

#include <string>
#include <string_view>



#include "libtorrent/alert_types.hpp"
#include "libtorrent/info_hash.hpp"
#include "libtorrent/sha1_hash.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/settings_pack.hpp"
#include "libtorrent/write_resume_data.hpp"
#include "libtorrent/read_resume_data.hpp"
#include "libtorrent/error_code.hpp"
#include "libtorrent/aux_/session_settings.hpp"




using namespace lt;
using namespace std::literals;

/* config_dir destination path, fetch settings file in this directory ,equals to  `tr_getDefaultConfigDir()`

gl_confdir about located at /home/pal/.config/transmissionSN

STRUCTURE:
    gl_configdir_____                       
                    |
                    |---session.sett
                    |
                    |---resume/
                            |
                            |------<info_hash>.resume
                            |------<info_hash>.resume
                            |------    ...  of each torrents
*/
std::string gl_confdir;



void gtr_pref_init(std::string_view config_dir)
{
    gl_confdir = config_dir;
}



namespace
{

    [[nodiscard]] std::string get_default_download_dir()
    {
        if (auto dir = Glib::get_user_special_dir(TR_GLIB_USER_DIRECTORY(DOWNLOAD)); !std::empty(dir))
        {
            return dir;
        }

        if (auto dir = Glib::get_user_special_dir(TR_GLIB_USER_DIRECTORY(DESKTOP)); !std::empty(dir))
        {
            return dir;
        }

        return tr_getDefaultDownloadDir();
    }


    /*settings pack defaults value may not what we need, so overwrite them*/
    lt::settings_pack ctor_pack_settings()
    {
        lt::alert_category_t const mask =
            alert_category::error
            // | alert_category::peer
            // | alert_category::port_mapping
            | alert_category::storage
            | alert_category::tracker
            // | alert_category::connect
            | alert_category::status
            // | alert_category::ip_block
            | alert_category::session_log
            | alert_category::torrent_log
            | alert_category::peer_log
            // | alert_category::incoming_request
            // | alert_category::port_mapping_log
            | alert_category::file_progress
            | alert_category::piece_progress;

        lt::settings_pack pack;
        pack.set_int(settings_pack::alert_mask, mask);

        /* GTK related settings , basically they are str settings, their defaults are empty string, so require init*/
        
        //start point path of FileChooseDialog
        pack.set_str(settings_pack::TR_KEY_open_dialog_dir, Glib::get_home_dir());
        //torrent cell sort-criteria
        pack.set_str(settings_pack::TR_KEY_sort_mode, std::string("sort-by-name"));

        auto dir = get_default_download_dir();
        pack.set_str(settings_pack::TR_KEY_watch_dir, dir);
        //relates to `add_torrent_params::save_path`
        pack.set_str(settings_pack::TR_KEY_download_dir, dir);
        pack.set_int(settings_pack::TR_KEY_message_level, TR_LOG_INFO);


        return pack;
    }


    bool is_this_resume_file(std::string const& s)
    {
        static std::string const hex_digit = "0123456789abcdef";
        if (s.size() != 40 + 7) return false;
        if (s.substr(40) != ".resume") return false;
        for (char const c : s.substr(0, 40))
        {
            if (hex_digit.find(c) == std::string::npos) return false;
        }
        return true;
    }


    std::string genResumefileName(lt::info_hash_t const& ih)
    {
        lt::sha1_hash const& s1 = ih.get();
        std::stringstream ss;
        ss << s1;
        char const* path = tr_pathbuf{ gl_confdir, "/resume/"sv , ss.str() + ".resume"};
        return std::string(path);
    }


    auto makeResumeDir(std::string_view config_dir)
    {
        auto dir = fmt::format("{:s}/resume"sv, config_dir);
        tr_error tmp_err{};/*Temp*/
        tr_sys_dir_create(dir, TR_SYS_DIR_CREATE_PARENTS, 0777, &tmp_err);

        if (tmp_err && tmp_err.has_value())
        {
            tr_logAddError(fmt::format(
                _("Couldn't create directory '{path}': {error} ({error_code})"),
                fmt::arg("path", dir),
                fmt::arg("error", tmp_err.message()),
                fmt::arg("error_code", tmp_err.code())));

        }

        return dir;/*Dir Path*/
    }


    bool load_to_buff(std::string_view filename, std::vector<char>& buf)
    {
        tr_error tmp_err;/*Temp*/
        tr_file_read(filename, buf, &tmp_err);

        if (tmp_err)
        {
            tr_logAddError(fmt::format(
                _("Couldn't load '{path}': {error} ({error_code})"),
                fmt::arg("path", filename),
                fmt::arg("error", tmp_err.message()),
                fmt::arg("error_code", tmp_err.code())));

            return false;/*Failure*/
        }

        return true;/*Success*/
    }



    bool save_to_file(std::string_view filename, std::vector<char> const& v)
    {
        tr_error tmp_err;/*Temp*/
        tr_file_save(filename, v, &tmp_err);

        if (tmp_err)
        {
            tr_logAddError(fmt::format(
                _("Couldn't save '{path}': {error} ({error_code})"),
                fmt::arg("path", filename),
                fmt::arg("error", tmp_err.message()),
                fmt::arg("error_code", tmp_err.code())));

            return false;/*Failure*/
        }

        return true;/*Success*/
    }



} //anonymous namespace




/*load resume file of each torrents in session*/
std::vector<lt::add_torrent_params> tr_torrentLoadResume()
{
    std::vector<lt::add_torrent_params> atps;

    auto const resume_dir = tr_pathbuf{ std::string_view{gl_confdir}, "/resume"sv };

    bool exists = tr_sys_path_exists(resume_dir);

    //if resume dir not exist, create one
    //if resume dir altready exists, use it
    if(!exists)
    {
        makeResumeDir(gl_confdir);
    }

    //list all `.resume files` in dir
    for (std::string name : tr_sys_dir_get_files(resume_dir, [](std::string_view name) { return (bool)is_this_resume_file(std::string{name}); }))
    {
        auto resume_data = std::vector<char>{};
        auto const resume_file = tr_pathbuf{ resume_dir, '/', name };

        if (tr_file_read(resume_file, resume_data))
        {
            lt::error_code ec;
            add_torrent_params p = lt::read_resume_data(resume_data, ec);
            if (ec)
            {
                std::printf("  failed to parse resume data \"%s\": %s\n"
                    , resume_file.c_str(), ec.message().c_str());
                continue;
            }
            atps.push_back(std::move(p));
        }
    }

    return atps;

}




/*
get the specified-torrent's reusme file <full path>
*/
std::string FetchTorResumePath(lt::info_hash_t const& ih)
{
    auto resume_path = genResumefileName(ih);

    //if resume file not exists for some reason
    if(!tr_sys_path_exists(resume_path))
    {
        return "";
    }

    return resume_path;
}


/*save resume file of the one torrents in session
    call from pop_alerts handling `save_resume_data_alert` in Session.cc
*/

void tr_torrentSaveResume(lt::add_torrent_params& params)
{
    auto const resume_dir = tr_pathbuf{ gl_confdir, "/resume"sv };
    bool exists = tr_sys_path_exists(resume_dir);
    //if resume dir not exist, create one
    //if resume dir altready exists, use it
    if(!exists)
    {
        makeResumeDir(gl_confdir);
    }

    auto const buf = lt::write_resume_data_buf(params);
    auto name = genResumefileName(params.info_hashes);
    save_to_file(name, buf);
    
}






/*loading session params file, various settings_pack items gtk settings etc...*/
lt::session_params  tr_sessionLoadSettings()
{
    lt::session_params ret;/*RET*/
    
    auto default_sett = ctor_pack_settings();/*TEMP*/
	
    auto& param_sett_ref = ret.settings;
    
    param_sett_ref = default_sett;

    // if a settings file exists, use it to override the provided `default_sett`
    // if not exists like especially when first use, just use defaults settings
    if (auto const filename = tr_pathbuf{ gl_confdir, "/session.sett"sv };
        tr_sys_path_exists(filename))
    {
        std::vector<char> buff;
        if(load_to_buff(filename, buff))
        {
            ret = read_session_params(buff);

            lt::aux::session_settings ses_sett_tmp{ret.settings};

            ret.settings = non_default_settings(ses_sett_tmp);
        }

    }
  
    return ret;
}



/*

saving session params file, including libtorrent settings, gtk client setts
    
{
    'extensions': {},
    'ip_filter4': [ '00000000ffffffff00000000' ],
    'ip_filter6': [ '00000000000000000000000000000000ffffffffffffffffffffffffffffffff00000000' ],
    'settings': { 'alert_mask': 73, 'enable_natpmp': 0, 'enable_upnp': 0 },
    'gtk_client': { 'xx' : xx }
},

*/
//ONLY save non-default settings pack items To save storage
void tr_sessionSaveSettings(lt::session_params const& sp)
{
    
    auto const sessett_filename = tr_pathbuf{ gl_confdir, "/session.sett"sv };/*TEMP*/

    std::vector<char> out = write_session_params_buf(sp);/*TEMP*/

    save_to_file(sessett_filename, out);

}



/**
 * 
 * Recent download dir-1,2,3,4
 * Recent Reallocate dir-1,2,3,4
 */


size_t const max_recent_dirs = size_t{ 4 };

/*
    For FileChoolserDialog display rencent opened dir
    but every time you add a new torent, its save locatoin defaults to settings_pack::TR_KEY_download_dir
*/
std::list<std::string> gtr_get_recent_dirs(lt::settings_pack sett, char key)
{
    std::list<std::string> list;

    for (size_t i = 0; i < max_recent_dirs; ++i)
    {
        switch(key)
        {
            /*download*/
            case 'd':
                if (auto const val = sett.get_str(settings_pack::TR_KEY_recent_download_dir_1+i); !val.empty())
                {
                    list.push_back(val);
                }
                break;
            /*relocate*/
            case 'r':
                if (auto const val = sett.get_str(settings_pack::TR_KEY_recent_relocate_dir_1+i); !val.empty())
                {
                    list.push_back(val);
                }
                break;
            default:
                break;      
        }
    }

    return list;
}

void gtr_save_recent_dir(char key, Glib::RefPtr<Session> const& core, std::string const& dir)
{
    if (dir.empty())
    {
        return;
    }

    auto sett = core->get_sett();
    auto list = gtr_get_recent_dirs(sett, key);

    /* if it was already in the list, remove it */
    list.remove(dir);

    /* add it to the front of the list */
    list.push_front(dir);

    /* save the first max_recent_dirs directories */
    list.resize(max_recent_dirs);
    int i = 0;
    for (auto const& d : list)
    {
        switch(key)
        {
            /*download*/
            case 'd':
                core->set_pref(settings_pack::TR_KEY_recent_download_dir_1+i, d);
                break;
            /*relocate*/
            case 'r':
                core->set_pref(settings_pack::TR_KEY_recent_relocate_dir_1+i, d);
                break;
            default:
                break;
        }
        ++i;
    }    
}









