project(libtorrent-examples)

set(single_file_examples
bt-get
    magnet2torrent
    torrent2magnet
    simple_client
    # custom_storage
    stats_counters
    dump_torrent
    dump_bdecode
    make_torrent
    connection_tester
    upnp_test)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang)
	add_compile_options(-Wno-implicit-int-float-conversion)
endif()

foreach(example ${single_file_examples})
    add_executable(${example} "${example}.cpp")
    target_link_libraries(${example} PRIVATE torrent-rasterbar)
endforeach(example)

add_executable(client_test
    client_test.cpp
    print.cpp
    torrent_view.cpp
    session_view.cpp)
target_link_libraries(client_test PRIVATE torrent-rasterbar)
