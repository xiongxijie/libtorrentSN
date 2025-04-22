

set(TOTEM_LIB_SOURCES
# src/totem-player-toolbar.h
# src/totem-player-toolbar.c
    # src/backend/bacon-time-label.h,
    # src/backend/bacon-video-widget.h,
    # src/totem-interface.h,
    # src/totem-menu.h,
    # src/totem-playlist.h,
    # src/totem-preferences-dialog.h,
    # src/totem-profile.h,
    # src/plugins/totem-dirs.c,
    # src/plugins/totem-plugins-engine.c,
    # src/totem-menu.c,
    # src/totem-playlist.c,
    # src/totem-preferences-dialog.c,
    # src/totem-preferences-plugin-row.c,

)




# Specify the library target
add_library(TotemLib STATIC ${TOTEM_LIB_SOURCES})


# Specify the parent directory for the headers
target_include_directories(TotemLib PRIVATE ${CMAKE_SOURCE_DIR}/src
PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


target_link_libraries(TotemLib
PRIVATE
    gtk_impl

)

