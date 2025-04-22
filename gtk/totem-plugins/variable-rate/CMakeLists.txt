
# we don't need any i18n merging and simply want to install the .desktop.in file directly to the install directory



# ${CMAKE_INSTALL_PREFIX} defaults to /usr/local/lib path, if we do not specify it

# install the .dekstop.in file to dir named as .plugin
set(plugin_name "variable-rate")
set(plugin_dir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/totem-plugins/${plugin_name}")
set(plugin_metadata_file "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_name}.plugin.desktop.in")
set(destination_metadata_file "${CMAKE_BINARY_DIR}/gtk/totem-plugins/${plugin_name}/${plugin_name}.plugin") 



# message(STATUS "In plugin-rotation dir plugin_metadata_file = ${plugin_metadata_file}")
# message(STATUS "In plugin-rotation dir destination_metadata_file = ${destination_metadata_file}")

# message(STATUS "In plugin-rotation dir plugin_dir = ${plugin_dir}")


# Add the shared library
add_library(${plugin_name} SHARED "totem-${plugin_name}-plugin.cc")


# Specify include directories, dependencies, etc.
target_include_directories(${plugin_name} PRIVATE ${plugins_incs})
target_link_libraries(${plugin_name} PRIVATE ${plugins_deps})


# Copy the .desktop.in file to the build directory after the build
add_custom_command(
    OUTPUT ${destination_metadata_file}
    COMMAND ${CMAKE_COMMAND} -E copy ${plugin_metadata_file} ${destination_metadata_file}
    DEPENDS ${plugin_metadata_file}
)


add_custom_target("${plugin_name}_copy_metadata" ALL DEPENDS ${destination_metadata_file})


# # Install the shared library
# install(TARGETS ${plugin_name}
#     LIBRARY DESTINATION ${plugin_dir}
# )


# # Install the .desktop file to the install directory
# install(FILES ${destination_metadata_file}
#     DESTINATION ${plugin_dir}
# )

