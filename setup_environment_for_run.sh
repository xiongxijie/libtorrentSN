#!/bin/bash




#using our own gsettings gschema
export GSETTINGS_SCHEMA_DIR=/media/pal/E/Downloads/libtorrent-SN/build/gtk/gschemas




#add path to search our gst-btdemux plugin for gstreamer
export GST_PLUGIN_PATH=/media/pal/E/Downloads/libtorrent-SN/build/gtk:$GST_PLUGIN_PATH




#use our own built gstreamer tailored for our btdemux
export LD_LIBRARY_PATH=/media/pal/E/FreeDsktop/gstreamer/install/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/media/pal/E/FreeDsktop/gstreamer/install/lib/x86_64-linux-gnu/gstreamer-1.0:$LD_LIBRARY_PATH