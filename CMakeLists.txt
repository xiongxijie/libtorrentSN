cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR) # Configurable policies: <= CMP0097

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0092 NEW)

list(APPEND CMAKE_MODULE_PATH 
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules"
		"${CMAKE_CURRENT_SOURCE_DIR}/third-party/rpavlik-cmake-modules"

)

set(CMAKE_MACOSX_RPATH ON)

# read_version("${CMAKE_CURRENT_SOURCE_DIR}/include/libtorrent/version.hpp" VER_MAJOR VER_MINOR VER_TINY)

project(libtorrent
	DESCRIPTION "Bittorrent library"
	VERSION 2.11.0
	LANGUAGES C CXX
)
set (SOVERSION "2.11")


include(LibtorrentMacros)
include(TrMacros)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(ExternalProject)
include(GNUInstallDirs)
include(GeneratePkgConfig)




set(TR_THIRD_PARTY_DIR_NAME third-party)
set(TR_THIRD_PARTY_SOURCE_DIR ${PROJECT_SOURCE_DIR}/${TR_THIRD_PARTY_DIR_NAME})
set(TR_THIRD_PARTY_BINARY_DIR ${PROJECT_BINARY_DIR}/${TR_THIRD_PARTY_DIR_NAME})





# message("PRoj source: ${PROJECT_SOURCE_DIR} \n")




set(libtorrent_include_files
	add_torrent_params.hpp
	address.hpp
	alert.hpp
	alert_types.hpp
	announce_entry.hpp
	assert.hpp
	bdecode.hpp
	bencode.hpp
	bitfield.hpp
	bloom_filter.hpp
	bt_peer_connection.hpp
	choker.hpp
	client_data.hpp
	close_reason.hpp
	config.hpp
	copy_ptr.hpp
	crc32c.hpp
	create_torrent.hpp
	deadline_timer.hpp
	debug.hpp
	disk_buffer_holder.hpp
	disk_interface.hpp
	disk_observer.hpp
	download_priority.hpp
	entry.hpp
	enum_net.hpp
	error.hpp
	error_code.hpp
	extensions.hpp
	file.hpp
	file_storage.hpp
	file_layout.hpp
	fingerprint.hpp
	flags.hpp
	fwd.hpp
	gzip.hpp
	# hash_picker.hpp
	hasher.hpp
	hex.hpp
	http_connection.hpp
	http_parser.hpp
	# http_seed_connection.hpp
	# http_stream.hpp
	http_tracker_connection.hpp
	# i2p_stream.hpp
	identify_client.hpp
	index_range.hpp
	io.hpp
	io_service.hpp
	ip_filter.hpp
	ip_voter.hpp
	libtorrent.hpp
	link.hpp
	lsd.hpp
	magnet_uri.hpp
	mmap_disk_io.hpp
	mmap_storage.hpp
	natpmp.hpp
	netlink.hpp
	operations.hpp
	optional.hpp
	parse_url.hpp
	part_file.hpp
	peer.hpp
	peer_class.hpp
	peer_class_set.hpp
	peer_class_type_filter.hpp
	peer_connection.hpp
	peer_connection_handle.hpp
	peer_connection_interface.hpp
	peer_id.hpp
	peer_info.hpp
	peer_list.hpp
	peer_request.hpp
	performance_counters.hpp
	pex_flags.hpp
	piece_block.hpp
	piece_block_progress.hpp
	piece_picker.hpp
	platform_util.hpp
	portmap.hpp
	# proxy_base.hpp
	puff.hpp
	random.hpp
	read_resume_data.hpp
	request_blocks.hpp
	# resolve_links.hpp
	session.hpp
	session_handle.hpp
	session_params.hpp
	# session_settings.hpp
	session_stats.hpp
	session_status.hpp
	session_types.hpp
	settings_pack.hpp
	sha1.hpp
	sha1_hash.hpp
	sha256.hpp
	sliding_average.hpp
	socket.hpp
	socket_io.hpp
	socket_type.hpp
	# socks5_stream.hpp
	span.hpp
	# ssl.hpp
	# ssl_stream.hpp
	stack_allocator.hpp
	stat.hpp
	stat_cache.hpp
	storage_defs.hpp
	string_util.hpp
	string_view.hpp
	tailqueue.hpp
	time.hpp
	torrent.hpp
	torrent_flags.hpp
	torrent_handle.hpp
	torrent_info.hpp
	torrent_peer.hpp
	torrent_peer_allocator.hpp
	torrent_status.hpp
	tracker_manager.hpp
	# truncate.hpp
	udp_socket.hpp
	udp_tracker_connection.hpp
	union_endpoint.hpp
	units.hpp
	upnp.hpp
	utf8.hpp
	vector_utils.hpp
	version.hpp
	# web_connection_base.hpp
	# web_peer_connection.hpp
	write_resume_data.hpp
	xml_parse.hpp
)

# set(libtorrent_kademlia_include_files
# 	announce_flags.hpp
# 	dht_observer.hpp
# 	dht_settings.hpp
# 	dht_state.hpp
# 	dht_storage.hpp
# 	dht_tracker.hpp
# 	direct_request.hpp
# 	dos_blocker.hpp
# 	ed25519.hpp
# 	find_data.hpp
# 	get_item.hpp
# 	get_peers.hpp
# 	io.hpp
# 	item.hpp
# 	msg.hpp
# 	node.hpp
# 	node_entry.hpp
# 	node_id.hpp
# 	observer.hpp
# 	put_data.hpp
# 	refresh.hpp
# 	routing_table.hpp
# 	rpc_manager.hpp
# 	sample_infohashes.hpp
# 	traversal_algorithm.hpp
# 	types.hpp
# )

set(libtorrent_extensions_include_files
	smart_ban.hpp
	ut_metadata.hpp
	ut_pex.hpp
)

set(libtorrent_aux_include_files
	alert_manager.hpp
	aligned_storage.hpp
	aligned_union.hpp
	alloca.hpp
	allocating_handler.hpp
	# apply_pad_files.hpp
	array.hpp
	bandwidth_limit.hpp
	bandwidth_manager.hpp
	bandwidth_queue_entry.hpp
	bandwidth_socket.hpp
	bind_to_device.hpp
	buffer.hpp
	byteswap.hpp
	chained_buffer.hpp
	cpuid.hpp
	deferred_handler.hpp
	deprecated.hpp
	deque.hpp
	dev_random.hpp
	directory.hpp
	disable_warnings_pop.hpp
	disable_warnings_push.hpp
	disk_buffer_pool.hpp
	mmap_disk_job.hpp
	disk_io_thread_pool.hpp
	disk_job_fence.hpp
	disk_job_pool.hpp
	drive_info.hpp
	# ed25519.hpp
	escape_string.hpp
	export.hpp
	ffs.hpp
	file_descriptor.hpp
	file_progress.hpp
	file_view_pool.hpp
	has_block.hpp
	heterogeneous_queue.hpp
	instantiate_connection.hpp
	invariant_check.hpp
	io.hpp
	ip_helpers.hpp
	ip_notifier.hpp
	keepalive.hpp
	listen_socket_handle.hpp
	lsd.hpp
	# merkle.hpp
	# merkle_tree.hpp
	netlink_utils.hpp
	noexcept_movable.hpp
	numeric_cast.hpp
	packet_buffer.hpp
	packet_pool.hpp
	path.hpp
	polymorphic_socket.hpp
	pool.hpp
	portmap.hpp
	posix_part_file.hpp
	# proxy_settings.hpp
	range.hpp
	receive_buffer.hpp
	resolver.hpp
	resolver_interface.hpp
	scope_end.hpp
	session_call.hpp
	session_impl.hpp
	session_interface.hpp
	session_settings.hpp
	session_udp_sockets.hpp
	set_socket_buffer.hpp
	set_traffic_class.hpp
	set_traffic_class.hpp
	socket_type.hpp
	storage_free_list.hpp
	storage_utils.hpp
	string_ptr.hpp
	strview_less.hpp
	suggest_piece.hpp
	throw.hpp
	time.hpp
	timestamp_history.hpp
	torrent_impl.hpp
	torrent_list.hpp
	unique_ptr.hpp
	utp_socket_manager.hpp
	utp_stream.hpp
	vector.hpp
	win_crypto_provider.hpp
	win_file_handle.hpp
	win_util.hpp
)

set(try_signal_include_files
	try_signal
	signal_error_code
	try_signal_mingw
	try_signal_msvc
	try_signal_posix
)

set(sources
	add_torrent_params.cpp
	alert.cpp
	alert_manager.cpp
	announce_entry.cpp
	assert.cpp
	bandwidth_limit.cpp
	bandwidth_manager.cpp
	bandwidth_queue_entry.cpp
	bdecode.cpp
	bitfield.cpp
	bloom_filter.cpp
	bt_peer_connection.cpp
	chained_buffer.cpp
	choker.cpp
	close_reason.cpp
	copy_file.cpp
	cpuid.cpp
	crc32c.cpp
	create_torrent.cpp
	directory.cpp
	disabled_disk_io.cpp
	disk_buffer_holder.cpp
	disk_buffer_pool.cpp
	disk_interface.cpp
	disk_io_thread_pool.cpp
	disk_job_fence.cpp
	disk_job_pool.cpp
	drive_info.cpp
	entry.cpp
	enum_net.cpp
	error_code.cpp
	escape_string.cpp
	ffs.cpp
	file.cpp
	file_progress.cpp
	file_storage.cpp
	file_view_pool.cpp
	fingerprint.cpp
	generate_peer_id.cpp
	gzip.cpp
	# hash_picker.cpp
	hasher.cpp
	hex.cpp
	http_connection.cpp
	http_parser.cpp
	# http_seed_connection.cpp
	http_tracker_connection.cpp
	# i2p_stream.cpp
	identify_client.cpp
	instantiate_connection.cpp
	ip_filter.cpp
	ip_helpers.cpp
	ip_notifier.cpp
	ip_voter.cpp
	listen_socket_handle.cpp
	load_torrent.cpp
	lsd.cpp
	magnet_uri.cpp
	# merkle.cpp
	# merkle_tree.cpp
	mmap.cpp
	mmap_disk_io.cpp
	mmap_disk_job.cpp
	mmap_storage.cpp
	natpmp.cpp
	packet_buffer.cpp
	parse_url.cpp
	part_file.cpp
	path.cpp
	peer_class.cpp
	peer_class_set.cpp
	peer_connection.cpp
	peer_connection_handle.cpp
	peer_info.cpp
	peer_list.cpp
	performance_counters.cpp
	piece_picker.cpp
	platform_util.cpp
	posix_disk_io.cpp
	posix_part_file.cpp
	posix_storage.cpp
	# proxy_base.cpp
	# proxy_settings.cpp
	puff.cpp
	random.cpp
	read_resume_data.cpp
	receive_buffer.cpp
	request_blocks.cpp
	# resolve_links.cpp
	resolver.cpp
	session.cpp
	session_call.cpp
	session_handle.cpp
	session_impl.cpp
	session_params.cpp
	session_settings.cpp
	session_stats.cpp
	settings_pack.cpp
	sha1.cpp
	sha1_hash.cpp
	sha256.cpp
	socket_io.cpp
	socket_type.cpp
	# socks5_stream.cpp
	# ssl.cpp
	stack_allocator.cpp
	stat.cpp
	stat_cache.cpp
	storage_utils.cpp
	string_util.cpp
	time.cpp
	timestamp_history.cpp
	torrent.cpp
	torrent_handle.cpp
	torrent_info.cpp
	torrent_peer.cpp
	torrent_peer_allocator.cpp
	torrent_status.cpp
	tracker_manager.cpp
	# truncate.cpp
	udp_socket.cpp
	udp_tracker_connection.cpp
	upnp.cpp
	utf8.cpp
	utp_socket_manager.cpp
	utp_stream.cpp
	version.cpp
	# web_connection_base.cpp
	# web_peer_connection.cpp
	write_resume_data.cpp
	xml_parse.cpp

# -- extensions --
	smart_ban.cpp
	ut_pex.cpp
	ut_metadata.cpp
)

# -- kademlia --
# set(kademlia_sources
# 	dht_settings.cpp
# 	dht_state.cpp
# 	dht_storage.cpp
# 	dht_tracker.cpp
# 	dos_blocker.cpp
# 	ed25519.cpp
# 	find_data.cpp
# 	get_item.cpp
# 	get_peers.cpp
# 	item.cpp
# 	msg.cpp
# 	node.cpp
# 	node_entry.cpp
# 	node_id.cpp
# 	put_data.cpp
# 	refresh.cpp
# 	routing_table.cpp
# 	rpc_manager.cpp
# 	sample_infohashes.cpp
# 	traversal_algorithm.cpp
# )

# -- ed25519 --
# set(ed25519_sources
# 	add_scalar.cpp
# 	fe.cpp
# 	ge.cpp
# 	key_exchange.cpp
# 	keypair.cpp
# 	sc.cpp
# 	sign.cpp
# 	verify.cpp
# 	sha512.cpp
# 	hasher512.cpp
# )

set(try_signal_sources
	try_signal.cpp
	signal_error_code.cpp
)

list(TRANSFORM sources PREPEND "src/")
# list(TRANSFORM kademlia_sources PREPEND "src/kademlia/")
# list(TRANSFORM ed25519_sources PREPEND "src/ed25519/")
list(TRANSFORM libtorrent_include_files PREPEND "include/libtorrent/")
list(TRANSFORM libtorrent_extensions_include_files PREPEND "include/libtorrent/extensions/")
list(TRANSFORM libtorrent_aux_include_files PREPEND "include/libtorrent/aux_/")
list(TRANSFORM libtorrent_kademlia_include_files PREPEND "include/libtorrent/kademlia/")
list(TRANSFORM try_signal_sources PREPEND "deps/try_signal/")

# these options control target creation and thus have to be declared before the add_library() call
feature_option(BUILD_SHARED_LIBS "build libtorrent as a shared library" ON)
feature_option(static_runtime "build libtorrent with static runtime" OFF)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_public_dependency(Threads REQUIRED)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang)
	add_compile_options(
		-Weverything
		-Wno-c++98-compat-pedantic
		-Wno-c++11-compat-pedantic
		-Wno-padded
		-Wno-alloca
		-Wno-global-constructors
		-Wno-exit-time-destructors
		-Wno-weak-vtables
		-Wno-return-std-move-in-c++11
		-Wno-unsafe-buffer-usage
		-Wno-unknown-warning-option
	)
elseif(CMAKE_CXX_COMPILER_ID MATCHES GNU)
	add_compile_options(
		-Wall
		-Wextra
		-Wpedantic
		-Wvla
		-Wno-noexcept-type
		-Wno-format-zero-length
		-ftemplate-depth=512
	)
elseif(MSVC)
	add_compile_options(
		/W4
		# C4251: 'identifier' : class 'type' needs to have dll-interface to be
		#        used by clients of class 'type2'
		/wd4251
		# C4268: 'identifier' : 'const' static/global data initialized
		#        with compiler generated default constructor fills the object with zeros
		/wd4268
		# C4275: non DLL-interface classkey 'identifier' used as base for
		#        DLL-interface classkey 'identifier'
		/wd4275
		# C4373: virtual function overrides, previous versions of the compiler
		#        did not override when parameters only differed by const/volatile qualifiers
		/wd4373
		# C4503: 'identifier': decorated name length exceeded, name was truncated
		/wd4503
	)
endif()

if(static_runtime)
	if (MSVC)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	else()
		include(ucm_flags)
		ucm_set_runtime(STATIC)
	endif()
	set(Boost_USE_MULTITHREADED ON)
	set(Boost_USE_STATIC_RUNTIME ON)
	set(OPENSSL_MSVC_STATIC_RT ON)
endif()

add_library(torrent-rasterbar
	${sources}
	${try_signal_sources}
	${libtorrent_include_files}
	${libtorrent_extensions_include_files}
	${libtorrent_aux_include_files}
)

# C++ 14 support is required
target_compile_features(torrent-rasterbar
	PUBLIC
	 	##########
		cxx_std_17

		cxx_attribute_deprecated
		cxx_binary_literals
		cxx_contextual_conversions
		cxx_decltype_auto
		cxx_digit_separators
		cxx_generic_lambdas
		cxx_lambda_init_captures
		cxx_relaxed_constexpr
		cxx_variable_templates
)

if (BUILD_SHARED_LIBS)
	target_compile_definitions(torrent-rasterbar
		PRIVATE TORRENT_BUILDING_SHARED
		INTERFACE TORRENT_LINKING_SHARED
	)
endif()

set_target_properties(torrent-rasterbar
	PROPERTIES
		CXX_VISIBILITY_PRESET "hidden"
		VISIBILITY_INLINES_HIDDEN "true"
		VERSION ${PROJECT_VERSION}
		SOVERSION ${SOVERSION}
)

target_include_directories(torrent-rasterbar PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	PRIVATE deps/try_signal
)

target_compile_definitions(torrent-rasterbar
	PUBLIC
		$<$<CONFIG:Debug>:TORRENT_USE_ASSERTS>
		BOOST_ASIO_ENABLE_CANCELIO
		BOOST_ASIO_NO_DEPRECATED
	PRIVATE
		TORRENT_BUILDING_LIBRARY
		BOOST_EXCEPTION_DISABLE
		BOOST_ASIO_HAS_STD_CHRONO
)

if (NOT WIN32)
	target_compile_definitions(torrent-rasterbar
		PRIVATE
			_FILE_OFFSET_BITS=64
	)
endif()

target_link_libraries(torrent-rasterbar
	PUBLIC
		Threads::Threads
)

# Unconditional platform-specific settings
if (WIN32)
	target_link_libraries(torrent-rasterbar
		PUBLIC
			bcrypt mswsock ws2_32 iphlpapi
			debug dbghelp crypt32
	)

	add_definitions(-D_WIN32_WINNT=0x0A00) # target Windows 10 or later

	target_compile_definitions(torrent-rasterbar
		PUBLIC WIN32_LEAN_AND_MEAN # prevent winsock1 to be included
	)

	if (MSVC)
		target_compile_definitions(torrent-rasterbar
			PUBLIC
				BOOST_ALL_NO_LIB
				_SCL_SECURE_NO_DEPRECATE _CRT_SECURE_NO_DEPRECATE # disable bogus deprecation warnings on msvc8
		)
		target_compile_options(torrent-rasterbar
			PRIVATE
				# allow larger .obj files (with more sections)
				/bigobj
				# https://learn.microsoft.com/en-us/cpp/build/reference/permissive-standards-conformance?view=msvc-170
				/permissive-
				# https://docs.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8?view=msvc-170
				/utf-8
				# https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
				/Zc:__cplusplus
		)
		set_target_properties(torrent-rasterbar PROPERTIES LINK_FLAGS_RELEASE "/OPT:ICF=5 /OPT:REF")
	endif()
endif()

if (ANDROID)
	target_link_libraries(torrent-rasterbar PRIVATE ${CMAKE_DL_LIBS})
endif()

if (APPLE)
	# for ip_notifier
	target_link_libraries(torrent-rasterbar PRIVATE "-framework CoreFoundation" "-framework SystemConfiguration")
endif()

# check if we need to link with libatomic (not needed on MSVC)
if (NOT Windows)
	# TODO: migrate to CheckSourceCompiles in CMake >= 3.19
	include(CheckCXXSourceCompiles)

	set(ATOMICS_TEST_SOURCE [=[
		#include <atomic>
		#include <cstdint>
		std::atomic<int> x{0};
		int main() {
			x.fetch_add(1, std::memory_order_relaxed);
			return 0;
		}
	]=])
	string(REPLACE "std::atomic<int>" "std::atomic<std::int8_t>" ATOMICS8_TEST_SOURCE "${ATOMICS_TEST_SOURCE}")
	string(REPLACE "std::atomic<int>" "std::atomic<std::int64_t>" ATOMICS64_TEST_SOURCE "${ATOMICS_TEST_SOURCE}")

	if(APPLE)
		set(CMAKE_REQUIRED_FLAGS "-std=c++11")
	endif()
	check_cxx_source_compiles("${ATOMICS_TEST_SOURCE}" HAVE_CXX_ATOMICS_WITHOUT_LIB)
	check_cxx_source_compiles("${ATOMICS8_TEST_SOURCE}" HAVE_CXX_ATOMICS8_WITHOUT_LIB)
	check_cxx_source_compiles("${ATOMICS64_TEST_SOURCE}" HAVE_CXX_ATOMICS64_WITHOUT_LIB)
	if((NOT HAVE_CXX_ATOMICS_WITHOUT_LIB) OR (NOT HAVE_CXX_ATOMICS8_WITHOUT_LIB) OR (NOT HAVE_CXX_ATOMICS64_WITHOUT_LIB))
		set(CMAKE_REQUIRED_LIBRARIES "atomic")
		check_cxx_source_compiles("${ATOMICS_TEST_SOURCE}" HAVE_CXX_ATOMICS_WITH_LIB)
		check_cxx_source_compiles("${ATOMICS8_TEST_SOURCE}" HAVE_CXX_ATOMICS8_WITH_LIB)
		check_cxx_source_compiles("${ATOMICS64_TEST_SOURCE}" HAVE_CXX_ATOMICS64_WITH_LIB)
		if ((NOT HAVE_CXX_ATOMICS_WITH_LIB) OR (NOT HAVE_CXX_ATOMICS8_WITH_LIB) OR (NOT HAVE_CXX_ATOMICS64_WITH_LIB))
			message(FATAL_ERROR "No native support for std::atomic, or libatomic not found!")
		else()
			message(STATUS "Linking with libatomic for atomics support")
			unset(CMAKE_REQUIRED_LIBRARIES)
			target_link_libraries(torrent-rasterbar PUBLIC atomic)
		endif()
	endif()
	if(APPLE)
		unset(CMAKE_REQUIRED_FLAGS)
	endif()
endif()




feature_option(build_tests "build tests" OFF)
feature_option(build_examples "build examples" ON)
feature_option(build_tools "build tools" OFF)
feature_option(python-bindings "build python bindings" OFF)
feature_option(python-egg-info "generate python egg info" OFF)
feature_option(python-install-system-dir "Install python bindings to the system installation directory rather than the CMake installation prefix" OFF)

# these options require existing target
feature_option(dht "enable support for Mainline DHT" OFF)

target_optional_compile_definitions(torrent-rasterbar PUBLIC FEATURE NAME deprecated-functions DEFAULT ON
	DESCRIPTION "enable deprecated functions for backwards compatibility" DISABLED TORRENT_NO_DEPRECATE)
feature_option(encryption "Enables encryption in libtorrent" ON)
feature_option(exceptions "build with exception support" ON)
feature_option(gnutls "build using GnuTLS instead of OpenSSL" OFF)
target_optional_compile_definitions(torrent-rasterbar PUBLIC FEATURE NAME extensions DEFAULT ON
	DESCRIPTION "Enables protocol extensions" DISABLED TORRENT_DISABLE_EXTENSIONS)
target_optional_compile_definitions(torrent-rasterbar PUBLIC FEATURE NAME i2p DEFAULT OFF
	DESCRIPTION "build with I2P support" DISABLED TORRENT_USE_I2P=0)
target_optional_compile_definitions(torrent-rasterbar PUBLIC FEATURE NAME logging DEFAULT ON
	DESCRIPTION "build with logging" DISABLED TORRENT_DISABLE_LOGGING)
target_optional_compile_definitions(torrent-rasterbar PUBLIC FEATURE NAME mutable-torrents DEFAULT OFF
	DESCRIPTION "Enables mutable torrent support" DISABLED TORRENT_DISABLE_MUTABLE_TORRENTS)
target_optional_compile_definitions(torrent-rasterbar PUBLIC FEATURE NAME streaming DEFAULT OFF
	DESCRIPTION "Enables support for piece deadline" DISABLED TORRENT_DISABLE_STREAMING)


#################################################
# if(NOT gnutls)
# 	find_public_dependency(OpenSSL)
# 	set_package_properties(OpenSSL
# 		PROPERTIES
# 			URL "https://www.openssl.org/"
# 			DESCRIPTION "Full-strength general purpose cryptography library"
# 			TYPE RECOMMENDED
# 			PURPOSE "Provides HTTPS support to libtorrent"
# 	)

# 	if(TARGET OpenSSL::SSL)
# 		# TODO: needed until https://gitlab.kitware.com/cmake/cmake/issues/19263 is fixed
# 		if(WIN32 AND OPENSSL_USE_STATIC_LIBS)
# 			target_link_libraries(torrent-rasterbar PRIVATE crypt32)
# 		endif()
# 		target_link_libraries(torrent-rasterbar PUBLIC OpenSSL::SSL)
# 		target_compile_definitions(torrent-rasterbar
# 			PUBLIC
# --------		TORRENT_USE_OPENSSL
# --------		TORRENT_USE_LIBCRYPTO
# --------		TORRENT_SSL_PEERS
# --------		OPENSSL_NO_SSL2)
# 	endif()
# endif()

# if(gnutls OR NOT TARGET OpenSSL::SSL)
# 	find_public_dependency(GnuTLS)
# 	set_package_properties(GnuTLS
# 		PROPERTIES
# 			URL "https://www.gnutls.org/"
# 			DESCRIPTION "GnuTLS is a free software implementation of the TLS and DTLS protocols"
# 			TYPE RECOMMENDED
# 			PURPOSE "Provides HTTPS support to libtorrent"
# 	)
# 	if(GNUTLS_FOUND)
# 		target_link_libraries(torrent-rasterbar PUBLIC GnuTLS::GnuTLS)
# 		target_compile_definitions(torrent-rasterbar
# 			PUBLIC
# 				TORRENT_USE_GNUTLS
# 				TORRENT_SSL_PEERS)
# 		target_include_directories(torrent-rasterbar PUBLIC
# 			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/deps/asio-gnutls/include>
#             $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# 		install(DIRECTORY deps/asio-gnutls/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# 	elseif(gnutls)
# 		message(FATAL_ERROR "GnuTLS library not found")
# 	endif()
# endif()

# if (NOT GNUTLS_FOUND AND NOT TARGET OpenSSL::SSL)
# 	if(TARGET OpenSSL::Crypto)
# 		target_link_libraries(torrent-rasterbar PUBLIC OpenSSL::Crypto)
# 		target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_USE_LIBCRYPTO)
# 	else()
# 		find_public_dependency(LibGcrypt)
# 		set_package_properties(LibGcrypt
# 			PROPERTIES
# 				URL "https://www.gnupg.org/software/libgcrypt/index.html"
# 				DESCRIPTION "A general purpose cryptographic library"
# 				TYPE RECOMMENDED
# 				PURPOSE "Use GCrypt instead of the built-in functions for RC4 and SHA1"
# 		)
# 		if (LibGcrypt_FOUND)
# 			target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_USE_LIBGCRYPT)
# 			target_link_libraries(torrent-rasterbar PRIVATE LibGcrypt::LibGcrypt)
# 		endif()
# 	endif()
# endif()
#################################################




# if (encryption)
	# target_sources(torrent-rasterbar PRIVATE include/libtorrent/pe_crypto.hpp src/pe_crypto.cpp)
# else()
	target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_DISABLE_ENCRYPTION)
# endif()

# if (dht)
# 	target_sources(torrent-rasterbar PRIVATE
# 		${libtorrent_kademlia_include_files}
# 		include/libtorrent/aux_/hasher512.hpp
# 		${kademlia_sources}
# 		${ed25519_sources}
# 	)
# else()
	target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_DISABLE_DHT)
# endif()

# Boost
find_public_dependency(Boost REQUIRED)
target_link_libraries(torrent-rasterbar PUBLIC Boost::headers)
if (Boost_MAJOR_VERSION LESS_EQUAL 1 AND Boost_MINOR_VERSION LESS 69)
	find_public_dependency(Boost REQUIRED COMPONENTS system)
	target_link_libraries(torrent-rasterbar PUBLIC Boost::system)
endif()

if (exceptions)
	if (MSVC)
		target_compile_options(torrent-rasterbar PUBLIC /EHsc)
	else (MSVC)
		target_compile_options(torrent-rasterbar PUBLIC -fexceptions)
	endif (MSVC)
else()
	if (MSVC)
		target_compile_definitions(torrent-rasterbar PUBLIC _HAS_EXCEPTIONS=0)
	else (MSVC)
		target_compile_options(torrent-rasterbar PUBLIC -fno-exceptions)
	endif (MSVC)
endif()

# developer options


target_optional_compile_definitions(torrent-rasterbar PUBLIC NAME utp-log DEFAULT ON
ENABLED TORRENT_UTP_LOG_ENABLE)
target_optional_compile_definitions(torrent-rasterbar PUBLIC NAME asio-debugging DEFAULT ON
ENABLED TORRENT_ASIO_DEBUGGING)
target_optional_compile_definitions(torrent-rasterbar PUBLIC NAME profile-calls DEFAULT ON
ENABLED TORRENT_PROFILE_CALLS=1)
option(developer-options "Activates options useful for a developer")
if(developer-options)
	set(asserts "auto" CACHE STRING "use assertions")
	set_property(CACHE asserts PROPERTY STRINGS auto on off production system)
	if ("${asserts}" MATCHES "on|production|system")
		target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_USE_ASSERTS=1)
	endif()
	if ("${asserts}" STREQUAL "production")
		target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_PRODUCTION_ASSERTS=1)
	elseif("${asserts}" STREQUAL "system")
		target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_USE_SYSTEM_ASSERTS=1)
	endif()

	target_optional_compile_definitions(torrent-rasterbar PUBLIC NAME asio-debugging DEFAULT OFF
		ENABLED TORRENT_ASIO_DEBUGGING)
	target_optional_compile_definitions(torrent-rasterbar PUBLIC NAME picker-debugging DEFAULT OFF
		ENABLED TORRENT_DEBUG_REFCOUNTS)
	set(invariant-checks "off" CACHE STRING "")
	set_property(CACHE invariant-checks PROPERTY STRINGS off on full)
	if (invariant-checks MATCHES "on|full")
		target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_USE_INVARIANT_CHECKS=1)
	endif()
	if (invariant-checks STREQUAL "full")
		target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_EXPENSIVE_INVARIANT_CHECKS)
	endif()

	target_optional_compile_definitions(torrent-rasterbar PUBLIC NAME utp-log DEFAULT OFF
		ENABLED TORRENT_UTP_LOG_ENABLE)
	target_optional_compile_definitions(torrent-rasterbar PUBLIC NAME simulate-slow-read DEFAULT OFF
		ENABLED TORRENT_SIMULATE_SLOW_READ)
	option(debug-iterators "" OFF)
	if (debug-iterators)
		if (MSVC)
			target_compile_definitions(torrent-rasterbar PUBLIC _ITERATOR_DEBUG_LEVEL=2)
		endif()
		if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
			target_compile_definitions(torrent-rasterbar PUBLIC _GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC)
		endif()
	endif()
	target_optional_compile_definitions(torrent-rasterbar PUBLIC NAME profile-calls DEFAULT OFF
		ENABLED TORRENT_PROFILE_CALLS=1)
endif()

# This is best effort attempt to propagate whether the library was built with
# C++11 or not. It affects the ABI of entry. A client building with C++14 and
# linking against a libtorrent binary built with C++11 can still define
# TORRENT_CXX11_ABI
if ("${CMAKE_CXX_STANDARD}" STREQUAL "11")
	target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_CXX11_ABI)
endif()

# There is little to none support for using pkg-config with MSVC and most users won't bother with it.
# However, msys is a linux-like platform on Windows that do support/prefer using pkg-config.
if (NOT MSVC)
	generate_and_install_pkg_config_file(torrent-rasterbar libtorrent-rasterbar)
endif()

include(CheckCXXCompilerFlag)

# add_subdirectory(bindings)

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
	file(RELATIVE_PATH CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_LIBDIR}")
endif()

install(TARGETS torrent-rasterbar EXPORT LibtorrentRasterbarTargets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/libtorrent DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h*")

# === generate a CMake Config File ===
include(CMakePackageConfigHelpers)
set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/LibtorrentRasterbar)
string(REGEX REPLACE "([^;]+)" "find_dependency(\\1)" _find_dependency_calls "${_package_dependencies}")
string(REPLACE ";" "\n" _find_dependency_calls "${_find_dependency_calls}")

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/LibtorrentRasterbar/LibtorrentRasterbarConfigVersion.cmake"
	VERSION ${libtorrent_VERSION}
	COMPATIBILITY AnyNewerVersion
)

export(EXPORT LibtorrentRasterbarTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/LibtorrentRasterbar/LibtorrentRasterbarTargets.cmake"
	NAMESPACE LibtorrentRasterbar::
)

configure_package_config_file(LibtorrentRasterbarConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/LibtorrentRasterbar/LibtorrentRasterbarConfig.cmake"
	INSTALL_DESTINATION "${ConfigPackageLocation}"
	NO_SET_AND_CHECK_MACRO
	NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(EXPORT LibtorrentRasterbarTargets
	NAMESPACE
		LibtorrentRasterbar::
	DESTINATION
		${ConfigPackageLocation}
)
install(
	FILES
		"${CMAKE_CURRENT_BINARY_DIR}/LibtorrentRasterbar/LibtorrentRasterbarConfig.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/LibtorrentRasterbar/LibtorrentRasterbarConfigVersion.cmake"
	DESTINATION
		${ConfigPackageLocation}
)

install(
	FILES
		${CMAKE_CURRENT_SOURCE_DIR}/examples/cmake/FindLibtorrentRasterbar.cmake
	DESTINATION
		${CMAKE_INSTALL_DATADIR}/cmake/Modules
)

if (MSVC)
	set_target_properties(torrent-rasterbar
	PROPERTIES
		PDB_NAME torrent-rasterbar
		PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
		COMPILE_PDB_NAME torrent-rasterbar
		COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
	)

	if (static_runtime)
		set(PDB_INSTALL_DIR lib)
	else()
		set(PDB_INSTALL_DIR bin)
	endif()

	install(
		FILES
			${CMAKE_BINARY_DIR}/torrent-rasterbar.pdb
		DESTINATION
			${PDB_INSTALL_DIR}
		CONFIGURATIONS
			Debug RelWithDebInfo
		OPTIONAL
	)
endif()

# === build tools ===
if (build_tools)
	add_subdirectory(tools)
endif()

# === build examples ===
if (build_examples)
	add_subdirectory(examples)
endif()

# === build tests ===
if(build_tests)
	enable_testing()
	# this will make some internal functions available in the DLL interface
	target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_EXPORT_EXTRA)
	add_subdirectory(test)
endif()


# enable TORRENT_EXPORT_EXTRA or some functions cannot be linked
add_subdirectory(Play)
target_compile_definitions(torrent-rasterbar PUBLIC TORRENT_EXPORT_EXTRA)







#=============================== build MY OWN client "copied" from Transmission gtk (GUI client) ============================
set(GIOMM_MINIMUM 2.26.0)
set(GLIBMM_MINIMUM 2.60.0)
set(GTKMM3_MINIMUM 3.24.0)
set(PSL_MINIMUM 0.21.1)
set(CURL_MINIMUM 7.28.0)

# Native Language Support
# find_package(Gettext 0.19.7 REQUIRED)
# add_subdirectory(po)

#========================================
set(CMAKE_FOLDER "${TR_THIRD_PARTY_DIR_NAME}")
set(SOURCE_ICONS_DIR "${PROJECT_SOURCE_DIR}/icons")

find_package(Fmt)
find_package(Small)
find_package(FastFloat)
find_package(PkgConfig QUIET)
find_package(CURL ${CURL_MINIMUM} REQUIRED)
find_package(UtfCpp)


unset(CMAKE_FOLDER)
#========================================
add_subdirectory(${TR_THIRD_PARTY_SOURCE_DIR}/wildmat)


tr_auto_option(USE_SYSTEM_EVENT2 "Use system event2 library" AUTO)
tr_add_external_auto_library(EVENT2 libevent event
TARGET libevent::event
CMAKE_ARGS
	-DEVENT__DISABLE_OPENSSL:BOOL=ON
	-DEVENT__DISABLE_BENCHMARK:BOOL=ON
	-DEVENT__DISABLE_TESTS:BOOL=ON
	-DEVENT__DISABLE_REGRESS:BOOL=ON
	-DEVENT__DISABLE_SAMPLES:BOOL=ON
	-DEVENT__LIBRARY_TYPE:STRING=STATIC)



tr_auto_option(USE_SYSTEM_PSL "Use system psl library" AUTO)
tr_add_external_auto_library(PSL libpsl psl
    TARGET psl::psl)


set(TR_NETWORK_LIBRARIES)
tr_select_library("c;socket;net" socket "" LIB)
if(NOT LIB MATCHES "^(|c)$")
	list(APPEND TR_NETWORK_LIBRARIES ${LIB})
endif()

tr_select_library("c;nsl;bind" gethostbyname "" LIB)
if(NOT LIB MATCHES "^(|c)$")
	list(APPEND TR_NETWORK_LIBRARIES ${LIB})
endif()





if(PKG_CONFIG_FOUND)
	message("---- PKG_CONFIG FOUND !!! \n")
endif()

# GTK3 is the prefix for the variables that pkg_check_modules generates, not the gtk3 lib
# -----${GTK3_FOUND} → Indicates whether the required packages were found (TRUE or FALSE).
# -----{GTK3_LIBRARIES} → List of libraries to link against.
# -----${GTK3_CFLAGS} → Compiler flags needed for these packages.
# -----${GTK3_INCLUDE_DIRS} → List of include directories for the found packages.
pkg_check_modules(GTK3
	gtkmm-3.0>=${GTKMM3_MINIMUM}
	glibmm-2.4>=${GLIBMM_MINIMUM}
	giomm-2.4>=${GIOMM_MINIMUM}
	glib-2.0
	libhandy-1 #used by totem ui
	libpeas-1.0 #used by totem plugins system 
	gobject-introspection-1.0>=0.6.7 #used by totem plugin engine
	libportal-gtk3 #used by totem's open-directory plugin
	gstreamer-1.0>=1.21.1
	gstreamer-tag-1.0>=1.21.1 # maybe redundant, gstreamer-1.0 already cover these modules for us
	gstreamer-video-1.0>=1.21.1
	gstreamer-pbutils-1.0>=1.21.1
	gstreamer-base-1.0>=1.21.1
	gstreamer-plugins-base-1.0>=1.21.1
	gstreamer-plugins-good-1.0>=1.21.1
	# pangomm-1.4 # gtkmm-3.0 already use pangomm,since we dont explicitly include pangomm headers, so no need to declare it as a explicit dependecy
	)
set(GTK_VERSION 3)
set(GTK_FOUND ${GTK3_FOUND})

message("---- dependencies listed in GTK3 found: ${GTK3_FOUND}!!!!\n")

if(NOT GTK_FOUND)
	message(FATAL_ERROR "dependencies listed in GTK3 no all found")
endif()
















#************* Check the target version Gstreamer(system one or custom one) have plugins we needed for BaconVideoWidget is available
# If you export PKG_CONFIG_PATH to your custom GStreamer installation before running pkg_check_modules, it will prioritize that version

# If you want to hardcode the custom GStreamer path directly in the CMake file
# set(MY_GST_PATH "/custom/path/to/my gstreamer")
# set(ENV{PKG_CONFIG_PATH} "${MY_GST_PATH}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
# pkg_check_modules(GST REQUIRED gstreamer-1.0)

#  pkg-config will prioritize the GStreamer version found in PKG_CONFIG_PATH path when resolving gstreamer-1.0
# so we can retrieve the tools directory from pkg-config for our targeted version gstreamer (whether system install one or our custom path one)
execute_process(
    COMMAND pkg-config --variable=toolsdir gstreamer-1.0
    OUTPUT_VARIABLE OUR_GST_TOOLSDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# NO_DEFAULT_PATH flag in the find_program command ensures that only the paths you provide will be searched for the program gst-inspect-1.0. 
# Without this flag, CMake would also check the system's default locations (e.g., system-wide directories like /usr/bin, etc.) for the program.
find_program(GST_INSPECT gst-inspect-1.0 PATHS ${OUR_GST_TOOLSDIR} NO_DEFAULT_PATH)
if(NOT GST_INSPECT)
    message(FATAL_ERROR "Cannot find required GStreamer-1.0 tool 'gst-inspect-1.0'. It should be part of gstreamer-1_0-utils. Please install it.")
endif()

#we can use gst-inspect-1.0 [plugin_name] | echo $? manually in terminal to check whether that plugin exists in your target gstreamer(system one or custom one)
set(GST_BASE_PLUGINS_NEEDED filesrc videoscale decodebin)
set(GST_GOOD_PLUGINS_NEEDED autoaudiosink autovideosink scaletempo gtkglsink glsinkbin)
# Flag to check if all gsteamer plugins are found
set(ALL_GST_PLUGINS_FOUND TRUE)
foreach(PLUGIN IN LISTS GST_BASE_PLUGINS_NEEDED)
    execute_process(
        COMMAND ${GST_INSPECT} ${PLUGIN}
        RESULT_VARIABLE RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(NOT RESULT EQUAL 0)
        message(FATAL_ERROR "Cannot find required GStreamer-1.0 plugin '${PLUGIN}'. It should be part of gst-plugins-base. Please install it.")
		set(ALL_GST_PLUGINS_FOUND FALSE)
    endif()
endforeach()
foreach(PLUGIN IN LISTS GST_GOOD_PLUGINS_NEEDED)
    execute_process(
        COMMAND ${GST_INSPECT} ${PLUGIN}
        RESULT_VARIABLE RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(NOT RESULT EQUAL 0)
        message(FATAL_ERROR "Cannot find required GStreamer-1.0 plugin '${PLUGIN}'. It should be part of gst-plugins-good. Please install it.")
		set(ALL_GST_PLUGINS_FOUND FALSE)
    endif()
endforeach()
# Print success message if all plugins are found
if(ALL_GST_PLUGINS_FOUND)
    message(STATUS "All required GStreamer plugins are installed and found.\n\n\n")
endif()



















### Compiler Warnings
set(C_WARNING_FLAGS)
set(CXX_WARNING_FLAGS)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

if(MSVC)
    set(WARNING_CANDIDATES /W4)

    foreach(FLAG ${WARNING_CANDIDATES})
        list(APPEND C_WARNING_FLAGS ${FLAG})
        list(APPEND CXX_WARNING_FLAGS ${FLAG})
    endforeach()
else()
    set(WARNING_CANDIDATES
        -W
        -Wall
        -Wextra
        -Wcast-align
        -Wduplicated-cond
        -Wextra-semi
        -Wextra-semi-stmt
        -Wextra-tokens
        -Wfloat-equal
        -Wgnu
        -Winit-self
        -Wint-in-bool-context
        -Wlogical-op
        -Wmissing-format-attribute
        -Wnull-dereference
        -Wpointer-arith
        -Wredundant-decls
        -Wredundant-move
        -Wreorder-ctor
        -Wrestrict
        -Wreturn-std-move
        -Wself-assign
        -Wself-move
        -Wsemicolon-before-method-body
        -Wsentinel
        -Wshadow
        -Wsign-compare
        -Wsometimes-uninitialized
        -Wstring-conversion
        -Wsuggest-destructor-override
        -Wsuggest-override
        -Wuninitialized
        -Wunreachable-code
        -Wunused
        -Wunused-const-variable
        -Wunused-parameter
        -Wunused-result
        -Wwrite-strings)

    if(MINGW)
        # Disable excessive warnings since we're using __USE_MINGW_ANSI_STDIO
        # Hopefully, any potential issues will be spotted on other platforms
        list(APPEND WARNING_CANDIDATES -Wno-format)
    else()
        list(APPEND WARNING_CANDIDATES -Wformat-security)
    endif()

    set(CMAKE_REQUIRED_FLAGS)

    foreach(FLAG -Werror /WX)
        tr_make_id("${FLAG}" FLAG_ID)
        set(CACHE_ID "${CMAKE_C_COMPILER_ID}_C_HAS${FLAG_ID}")
        string(TOLOWER "${CACHE_ID}" CACHE_ID)
        check_c_compiler_flag(${FLAG} ${CACHE_ID})
        if(${CACHE_ID})
            # Make sure the next loop only adds flags that are relevant for a particular language
            set(CMAKE_REQUIRED_FLAGS ${FLAG})
            break()
        endif()
    endforeach()

    foreach(FLAG ${WARNING_CANDIDATES})
        tr_make_id("${FLAG}" FLAG_ID)

        # if available, add to C warnings
        set(CACHE_ID "${CMAKE_C_COMPILER_ID}_C_HAS${FLAG_ID}")
        string(TOLOWER "${CACHE_ID}" CACHE_ID)
        check_c_compiler_flag(${FLAG} ${CACHE_ID})
        if(${CACHE_ID})
            list(APPEND C_WARNING_FLAGS ${FLAG})
        endif()

        # if available, add to CXX warnings
        set(CACHE_ID "${CMAKE_CXX_COMPILER_ID}_CXX_HAS${FLAG_ID}")
        string(TOLOWER "${CACHE_ID}" CACHE_ID)
        check_cxx_compiler_flag(${FLAG} ${CACHE_ID})
        if(${CACHE_ID})
            list(APPEND CXX_WARNING_FLAGS ${FLAG})
        endif()

        unset(CACHE_ID)
        unset(FLAG_ID)
    endforeach()

    unset(CMAKE_REQUIRED_FLAGS)
endif()

string(REPLACE ";" "$<SEMICOLON>" C_WARNING_FLAGS_GENEX "${C_WARNING_FLAGS}")
string(REPLACE ";" "$<SEMICOLON>" CXX_WARNING_FLAGS_GENEX "${CXX_WARNING_FLAGS}")
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:${C_WARNING_FLAGS_GENEX}>
    $<$<COMPILE_LANGUAGE:CXX>:${CXX_WARNING_FLAGS_GENEX}>)

###











if(GTK_FOUND)
		# INTERFACE means does not compile or link any object files itself, but instead, it propagates its properties 
		# (like compiler options, include directories, and link libraries) to other targets that link to it.
		
		# IMPORTED means exists externally libs, CMake does not compile or link anything for an IMPORTED target
	add_library(gtk_impl INTERFACE IMPORTED) # creates a virtual target named gtk_impl that is not compiled, 
	# Any target that links to gtk_impl will automatically get all GTK3-related settings

	# This applies compiler flags (like -DGTK_ENABLE_DEBUG or -fPIC) needed for `GTK3`(a pkg_check_modules preifx) to all targets who try link to gtk_impl.
    target_compile_options(gtk_impl
        INTERFACE
            ${GTK${GTK_VERSION}_CFLAGS_OTHER})

	#  add GTK3(a pkg_check_modules prefix) include directories to any target who link against gtk_impl. 
    target_include_directories(gtk_impl
        INTERFACE
            ${GTK${GTK_VERSION}_INCLUDE_DIRS})

	# sets the GTK3(a pkg_check_modules prefix) library search paths, ensuring that the linker can find libraries in GTK3  
    target_link_directories(gtk_impl
        INTERFACE
            ${GTK${GTK_VERSION}_LIBRARY_DIRS})
	# This makes gtk_impl propagate the GTK3(a pkg_check_modules prefix) library dependencies (e.g., libgtk-3.so, libglib-2.0.so) to any target who will links with gtk_impl
    target_link_libraries(gtk_impl
        INTERFACE
            ${GTK${GTK_VERSION}_LIBRARIES})
endif()


add_subdirectory(gtk)


feature_summary(DEFAULT_DESCRIPTION WHAT ALL)
