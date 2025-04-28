#pragma once



#include <iostream>
#include <vector>
#include <cstring>

#include <glib.h>
#include <gio/gio.h>






typedef enum {
	TOTEM_PLAYLIST_DIRECTION_NEXT,
	TOTEM_PLAYLIST_DIRECTION_PREVIOUS
} TotemPlaylistDirection;



class PlaylistItem {
    public:
        int fileidx;
        char* filename;
        char* fullpath;
    
        PlaylistItem(int idx, const char* fname, const char* fpath)
            : fileidx(idx) {
            filename = strdup(fname);
            fullpath = strdup(fpath);
        }
    
        ~PlaylistItem() {
            free(filename);
            free(fullpath);
        }
};
    


class TotemPlaylist {
    private:
        std::vector<PlaylistItem*> items;
        int currentIndex = -1;
        GSettings* settings;
    
    public:
        TotemPlaylist() {
            std::cout << "create TotemPlayeList" << std::endl;
            settings = g_settings_new(TOTEM_GSETTINGS_SCHEMA);
        }
    
        ~TotemPlaylist() {
            std::cout << "~TotemPlaylist()" << std::endl;
            for (auto item : items) {
                delete item;
            }
            g_clear_object(&settings);
        }
    
        void add_one_row(int idx, const char* fname, const char* fpath) {
            items.push_back(new PlaylistItem(idx, fname, fpath));
            if (currentIndex == -1) currentIndex = 0;
        }
    
        void set_previous() {
            if (have_previous_item()) --currentIndex;
        }
    
        void set_next() {
            if (have_next_item()) ++currentIndex;
        }
    
        bool have_previous_item() const {
            return currentIndex > 0;
        }
    
        bool have_next_item() const {
            return currentIndex < (int)items.size() - 1;
        }
    
        bool get_repeat() const {
            return g_settings_get_boolean(settings, "repeat");
        }
    
        void set_repeat(bool value) {
            g_settings_set_boolean(settings, "repeat", value);
        }
    
        void set_at_start() {
            if (!items.empty()) currentIndex = 0;
        }
    
        void set_at_end() {
            if (!items.empty()) currentIndex = items.size() - 1;
        }
    
        int get_last() const {
            PlaylistItem* rear = items.empty() ? nullptr : items.back();
            return  rear->fileidx;
        }
    
        int get_current_fileidx() const {
            return (currentIndex >= 0 && currentIndex < (int)items.size()) ? items[currentIndex]->fileidx : -1;
        }
    
        const char* get_current_fullpath() const {
            return (currentIndex >= 0 && currentIndex < (int)items.size()) ? items[currentIndex]->fullpath : nullptr;
        }
    
        const char* get_current_filename() const {
            return (currentIndex >= 0 && currentIndex < (int)items.size()) ? items[currentIndex]->filename : nullptr;
        }

        void set_current(int idx) {
            for (size_t i = 0; i < items.size(); ++i) {
                if (items[i]->fileidx == idx) {
                    currentIndex = i;
                    return;
                }
            }
        }


        bool has_direction(TotemPlaylistDirection dir)
        {
            if(dir == TOTEM_PLAYLIST_DIRECTION_NEXT)
            {
                return have_next_item();
            }
            else if(dir == TOTEM_PLAYLIST_DIRECTION_PREVIOUS)
            {
                return have_previous_item();
            }
        }


        void set_direction (TotemPlaylistDirection dir)
        {
            if(dir == TOTEM_PLAYLIST_DIRECTION_NEXT)
            {
                set_next();
            }
            else if(dir == TOTEM_PLAYLIST_DIRECTION_PREVIOUS)
            {
                set_previous();
            }
        }
};