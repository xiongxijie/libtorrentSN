

#???
# plugin are each .so ,if they include header such as  totem and bacon-video-widget, they need to link again them
# but library  cannot directly link against an executable
#???


# libs that each plugin may need (such as glib, libpeas), Plugin also might need to link again such as bacon_video_widget.h and totem-object.h 
set(plugins_deps gtk_impl totemplugins)

set(plugins_incs 
    ${gtk_dir_source_dir} 
    ${gtk_dir_source_dir}/totem-plugins
) 
# set(plugins_cflags "-DTOTEM_PLUGIN_DIR=\"/media/pal/E/Downloads/libtorrent-SN/build/gtk/totem-plugins\"")
# target_compile_definitions(${plugin_name} PUBLIC ${plugins_cflags})


# Print the value of the variable
# message(STATUS "In totem-plugins dir plugins_incs = ${plugins_incs}")


#Add each plugin's dir entry
add_subdirectory(open-directory)
add_subdirectory(properties)
add_subdirectory(rotation)
add_subdirectory(variable-rate)
