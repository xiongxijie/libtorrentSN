

# Our tests contain printf formatting checks therefore we disable GCC format string errors:
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	check_cxx_compiler_flag("-Wno-error=format-truncation" _WNO_ERROR_FORMAT_TRUNCATION)
	if (_WNO_ERROR_FORMAT_TRUNCATION)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=format-truncation")
	endif()
endif()






add_library(test_common
	STATIC
	# broadcast_socket.cpp
	# dht_server.cpp
	# main.cpp
	make_torrent.cpp
	# peer_server.cpp
	settings.cpp
	setup_transfer.cpp
	# swarm_suite.cpp
	# test.cpp
	test_utils.cpp
	udp_tracker.cpp
	# web_seed_suite.cpp
)
target_link_libraries(test_common PUBLIC torrent-rasterbar)


if(CMAKE_CXX_COMPILER_ID MATCHES Clang)
	add_compile_options(-Wno-implicit-int-float-conversion )
	add_compile_options(-Wunused-parameter)

endif()




add_executable(Demo
test_anything.cpp
)
target_link_libraries(Demo PUBLIC test_common)


add_executable(Gemo
test_something.cpp
)
target_link_libraries(Gemo PUBLIC test_common)
 

# target_include_directories(Demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include) 

