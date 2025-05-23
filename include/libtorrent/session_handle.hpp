/*

Copyright (c) 2014-2018, Steven Siloti
Copyright (c) 2015-2018, Alden Torres
Copyright (c) 2015-2022, Arvid Norberg
Copyright (c) 2020, AllSeeingEyeTolledEweSew
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_SESSION_HANDLE_HPP_INCLUDED
#define TORRENT_SESSION_HANDLE_HPP_INCLUDED

#include <memory> // for shared_ptr

#include "libtorrent/config.hpp"
#include "libtorrent/fwd.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/alert.hpp" // alert_category::error
#include "libtorrent/peer_class.hpp"
#include "libtorrent/peer_class_type_filter.hpp"
#include "libtorrent/peer_id.hpp"
#include "libtorrent/io_context.hpp"
#include "libtorrent/session_types.hpp"
#include "libtorrent/portmap.hpp" // for portmap_protocol


// #include "libtorrent/kademlia/announce_flags.hpp"

#if TORRENT_ABI_VERSION == 1
// #include "libtorrent/session_settings.hpp"
#include <deque>
#endif

#include "libtorrent/extensions.hpp"
#include "libtorrent/session_types.hpp" // for session_flags_t

namespace libtorrent {

	struct torrent;

#if TORRENT_ABI_VERSION == 1
	struct session_status;
	using user_load_function_t = std::function<void(sha1_hash const&
		, std::vector<char>&, error_code&)>;
#endif

	// this class provides a non-owning handle to a session and a subset of the
	// interface of the session class. If the underlying session is destructed
	// any handle to it will no longer be valid. is_valid() will return false and
	// any operation on it will throw a system_error exception, with error code
	// invalid_session_handle.
	struct TORRENT_EXPORT session_handle
	{
		friend struct session;
		friend struct aux::session_impl;

		// hidden
		session_handle() = default;
		session_handle(session_handle const& t) = default;
		session_handle(session_handle&& t) noexcept = default;
		session_handle& operator=(session_handle const&) & = default;
		session_handle& operator=(session_handle&&) & noexcept = default;

#if TORRENT_ABI_VERSION == 1
		using save_state_flags_t = libtorrent::save_state_flags_t;
		using session_flags_t = libtorrent::session_flags_t;
#endif

		// returns true if this handle refers to a valid session object. If the
		// session has been destroyed, all session_handle objects will expire and
		// not be valid.
		bool is_valid() const { return !m_impl.expired(); }

		// saves settings (i.e. the settings_pack)
		static constexpr save_state_flags_t save_settings = 0_bit;


#if TORRENT_ABI_VERSION == 1
		// save pe_settings
		// TORRENT_DEPRECATED static constexpr save_state_flags_t save_encryption_settings = 3_bit;
		TORRENT_DEPRECATED static constexpr save_state_flags_t save_as_map = 4_bit;
		// TORRENT_DEPRECATED static constexpr save_state_flags_t save_proxy = 5_bit;

		// TORRENT_DEPRECATED static constexpr save_state_flags_t save_peer_proxy = 8_bit;
		// TORRENT_DEPRECATED static constexpr save_state_flags_t save_web_proxy = 9_bit;
		// TORRENT_DEPRECATED static constexpr save_state_flags_t save_tracker_proxy = 10_bit;
#endif

		// load or save state from plugins
		static constexpr save_state_flags_t save_extension_state = 11_bit;

		// load or save the IP filter set on the session
		static constexpr save_state_flags_t save_ip_filter = 12_bit;

#if TORRENT_ABI_VERSION <= 2
		// deprecated in 2.0
		// instead of these functions, use session_state() below, and restore
		// state using the session_params on session construction.

		// loads and saves all session settings, including dht_settings,
		// encryption settings and proxy settings. ``save_state`` writes all keys
		// to the ``entry`` that's passed in, which needs to either not be
		// initialized, or initialized as a dictionary.
		//
		// ``load_state`` expects a bdecode_node which can be built from a bencoded
		// buffer with bdecode().
		//
		// The ``flags`` argument is used to filter which parts of the session
		// state to save or load. By default, all state is saved/restored (except
		// for the individual torrents).
		//
		// When saving settings, there are two fields that are *not* loaded.
		// ``peer_fingerprint`` and ``user_agent``. Those are left as configured
		// by the ``session_settings`` passed to the session constructor or
		// subsequently set via apply_settings().
		TORRENT_DEPRECATED
		void save_state(entry& e, save_state_flags_t flags = save_state_flags_t::all()) const;
		TORRENT_DEPRECATED
		void load_state(bdecode_node const& e, save_state_flags_t flags = save_state_flags_t::all());
#endif

		// returns the current session state. This can be passed to
		// write_session_params() to save the state to disk and restored using
		// read_session_params() when constructing a new session. The kind of
		// state that's included is all settings, the DHT routing table, possibly
		// plugin-specific state.
		// the flags parameter can be used to only save certain parts of the
		// session state
		// only save non_default_settings
		session_params session_state(save_state_flags_t flags = save_state_flags_t::all()) const;

		// .. note::
		// 	these calls are potentially expensive and won't scale well with
		// 	lots of torrents. If you're concerned about performance, consider
		// 	using ``post_torrent_updates()`` instead.
		//
		// ``get_torrent_status`` returns a vector of the torrent_status for
		// every torrent which satisfies ``pred``, which is a predicate function
		// which determines if a torrent should be included in the returned set
		// or not. Returning true means it should be included and false means
		// excluded. The ``flags`` argument is the same as to
		// torrent_handle::status(). Since ``pred`` is guaranteed to be
		// called for every torrent, it may be used to count the number of
		// torrents of different categories as well.
		//
		// ``refresh_torrent_status`` takes a vector of torrent_status structs
		// (for instance the same vector that was returned by
		// get_torrent_status() ) and refreshes the status based on the
		// ``handle`` member. It is possible to use this function by first
		// setting up a vector of default constructed ``torrent_status`` objects,
		// only initializing the ``handle`` member, in order to request the
		// torrent status for multiple torrents in a single call. This can save a
		// significant amount of time if you have a lot of torrents.
		//
		// Any torrent_status object whose ``handle`` member is not referring to
		// a valid torrent are ignored.
		//
		// The intended use of these functions is to start off by calling
		// ``get_torrent_status()`` to get a list of all torrents that match your
		// criteria. Then call ``refresh_torrent_status()`` on that list. This
		// will only refresh the status for the torrents in your list, and thus
		// ignore all other torrents you might be running. This may save a
		// significant amount of time, especially if the number of torrents you're
		// interested in is small. In order to keep your list of interested
		// torrents up to date, you can either call ``get_torrent_status()`` from
		// time to time, to include torrents you might have become interested in
		// since the last time. In order to stop refreshing a certain torrent,
		// simply remove it from the list.
		std::vector<torrent_status> get_torrent_status(
			std::function<bool(torrent_status const&)> const& pred
			, status_flags_t flags = {}) const;
		void refresh_torrent_status(std::vector<torrent_status>* ret
			, status_flags_t flags = {}) const;

		// This functions instructs the session to post the state_update_alert,
		// containing the status of all torrents whose state changed since the
		// last time this function was called.
		//
		// Only torrents who has the state subscription flag set will be
		// included. This flag is on by default. See add_torrent_params.
		// the ``flags`` argument is the same as for torrent_handle::status().
		// see status_flags_t in torrent_handle.
		void post_torrent_updates(status_flags_t flags = status_flags_t::all());

		// This function will post a session_stats_alert object, containing a
		// snapshot of the performance counters from the internals of libtorrent.
		// To interpret these counters, query the session via
		// session_stats_metrics().
		//
		// For more information, see the session-statistics_ section.
		void post_session_stats();

		// internal
		io_context& get_context();


		// ``find_torrent()`` looks for a torrent with the given info-hash. In
		// case there is such a torrent in the session, a torrent_handle to that
		// torrent is returned. In case the torrent cannot be found, an invalid
		// torrent_handle is returned.
		//
		// See ``torrent_handle::is_valid()`` to know if the torrent was found or
		// not.
		//
		// ``get_torrents()`` returns a vector of torrent_handles to all the
		// torrents currently in the session.
		torrent_handle find_torrent(sha1_hash const& info_hash) const;
		std::vector<torrent_handle> get_torrents() const;

		// You add torrents through the add_torrent() function where you give an
		// object with all the parameters. The add_torrent() overloads will block
		// until the torrent has been added (or failed to be added) and returns
		// an error code and a torrent_handle. In order to add torrents more
		// efficiently, consider using async_add_torrent() which returns
		// immediately, without waiting for the torrent to add. Notification of
		// the torrent being added is sent as add_torrent_alert.
		//
		// The overload that does not take an error_code throws an exception on
		// error and is not available when building without exception support.
		// The torrent_handle returned by add_torrent() can be used to retrieve
		// information about the torrent's progress, its peers etc. It is also
		// used to abort a torrent.
		//
		// If the torrent you are trying to add already exists in the session (is
		// either queued for checking, being checked or downloading)
		// ``add_torrent()`` will throw system_error which derives from
		// ``std::exception`` unless duplicate_is_error is set to false. In that
		// case, add_torrent() will return the handle to the existing torrent.
		//
		// The add_torrent_params class has a flags field. It can be used to
		// control what state the new torrent will be added in. Common flags to
		// want to control are torrent_flags::paused and
		// torrent_flags::auto_managed. In order to add a magnet link that will
		// just download the metadata, but no payload, set the
		// torrent_flags::upload_mode flag.
		//
		// Special consideration has to be taken when adding hybrid torrents
		// (i.e. torrents that are BitTorrent v2 torrents that are backwards
		// compatible with v1). For more details, see BitTorrent-v2-torrents_.
#ifndef BOOST_NO_EXCEPTIONS
		torrent_handle add_torrent(add_torrent_params&& params);
		torrent_handle add_torrent(add_torrent_params const& params);
#endif
		torrent_handle add_torrent(add_torrent_params&& params, error_code& ec);
		torrent_handle add_torrent(add_torrent_params const& params, error_code& ec);
		void async_add_torrent(add_torrent_params&& params);
		void async_add_torrent(add_torrent_params const& params);

#ifndef BOOST_NO_EXCEPTIONS
#if TORRENT_ABI_VERSION == 1
		// deprecated in 0.14
		TORRENT_DEPRECATED
		torrent_handle add_torrent(
			torrent_info const& ti
			, std::string const& save_path
			, entry const& resume_data = entry()
			, storage_mode_t storage_mode = storage_mode_sparse
			, bool paused = false);

		// deprecated in 0.14
		TORRENT_DEPRECATED
		torrent_handle add_torrent(
			char const* tracker_url
			, sha1_hash const& info_hash
			, char const* name
			, std::string const& save_path
			, entry const& resume_data = entry()
			, storage_mode_t storage_mode = storage_mode_sparse
			, bool paused = false
			, client_data_t userdata = {});
#endif // TORRENT_ABI_VERSION
#endif

		// Pausing the session has the same effect as pausing every torrent in
		// it, except that torrents will not be resumed by the auto-manage
		// mechanism. Resuming will restore the torrents to their previous paused
		// state. i.e. the session pause state is separate from the torrent pause
		// state. A torrent is inactive if it is paused or if the session is
		// paused.
		void pause();
		void resume();
		bool is_paused() const;

#if TORRENT_ABI_VERSION == 1
		// *the feature of dynamically loading/unloading torrents is deprecated
		// and discouraged*
		//
		// This function enables dynamic-loading-of-torrent-files_. When a
		// torrent is unloaded but needs to be available in memory, this function
		// is called **from within the libtorrent network thread**. From within
		// this thread, you can **not** use any of the public APIs of libtorrent
		// itself. The info-hash of the torrent is passed in to the function
		// and it is expected to fill in the passed in ``vector<char>`` with the
		// .torrent file corresponding to it.
		//
		// If there is an error loading the torrent file, the ``error_code``
		// (``ec``) should be set to reflect the error. In such case, the torrent
		// itself is stopped and set to an error state with the corresponding
		// error code.
		//
		// Given that the function is called from the internal network thread of
		// libtorrent, it's important to not stall. libtorrent will not be able
		// to send nor receive any data until the function call returns.
		//
		// The signature of the function to pass in is::
		//
		// 	void fun(sha1_hash const& info_hash, std::vector<char>& buf, error_code& ec);
		TORRENT_DEPRECATED
		void set_load_function(user_load_function_t fun);

#include "libtorrent/aux_/disable_warnings_push.hpp"

		// deprecated in libtorrent 1.1, use performance_counters instead
		// returns session wide-statistics and status. For more information, see
		// the ``session_status`` struct.
		TORRENT_DEPRECATED
		session_status status() const;

#include "libtorrent/aux_/disable_warnings_pop.hpp"

		// deprecated in 1.2
		TORRENT_DEPRECATED
		void get_torrent_status(std::vector<torrent_status>* ret
			, std::function<bool(torrent_status const&)> const& pred
			, status_flags_t flags = {}) const;


#endif



		// This function adds an extension to this session. The argument is a
		// function object that is called with a ``torrent_handle`` and which should
		// return a ``std::shared_ptr<torrent_plugin>``. To write custom
		// plugins, see `libtorrent plugins`_. For the typical bittorrent client
		// all of these extensions should be added. The main plugins implemented
		// in libtorrent are:
		//
		// uTorrent metadata
		// 	Allows peers to download the metadata (.torrent files) from the swarm
		// 	directly. Makes it possible to join a swarm with just a tracker and
		// 	info-hash.
		//
		// .. code:: c++
		//
		// 	#include <libtorrent/extensions/ut_metadata.hpp>
		// 	ses.add_extension(&lt::create_ut_metadata_plugin);
		//
		// uTorrent peer exchange
		// 	Exchanges peers between clients.
		//
		// .. code:: c++
		//
		// 	#include <libtorrent/extensions/ut_pex.hpp>
		// 	ses.add_extension(&lt::create_ut_pex_plugin);
		//
		// smart ban plugin
		// 	A plugin that, with a small overhead, can ban peers
		// 	that sends bad data with very high accuracy. Should
		// 	eliminate most problems on poisoned torrents.
		//
		// .. code:: c++
		//
		// 	#include <libtorrent/extensions/smart_ban.hpp>
		// 	ses.add_extension(&lt::create_smart_ban_plugin);
		//
		//
		// .. _`libtorrent plugins`: reference-Plugins.html
		void add_extension(std::function<std::shared_ptr<torrent_plugin>(
			torrent_handle const&, client_data_t)> ext);
		void add_extension(std::shared_ptr<plugin> ext);

#if TORRENT_ABI_VERSION == 1
		// deprecated in 0.15
		// use load_state and save_state instead
		TORRENT_DEPRECATED
		void load_state(entry const& ses_state
			, save_state_flags_t flags = save_state_flags_t::all());
		TORRENT_DEPRECATED
		entry state() const;
#endif // TORRENT_ABI_VERSION

		// Sets a filter that will be used to reject and accept incoming as well
		// as outgoing connections based on their originating ip address. The
		// default filter will allow connections to any ip address. To build a
		// set of rules for which addresses are accepted and not, see ip_filter.
		//
		// Each time a peer is blocked because of the IP filter, a
		// peer_blocked_alert is generated. ``get_ip_filter()`` Returns the
		// ip_filter currently in the session. See ip_filter.
		void set_ip_filter(ip_filter f);
		ip_filter get_ip_filter() const;

		// apply port_filter ``f`` to incoming and outgoing peers. a port filter
		// will reject making outgoing peer connections to certain remote ports.
		// The main intention is to be able to avoid triggering certain
		// anti-virus software by connecting to SMTP, FTP ports.
		void set_port_filter(port_filter const& f);

#if TORRENT_ABI_VERSION == 1
		// deprecated in 1.1, use settings_pack::peer_fingerprint instead
		TORRENT_DEPRECATED
		void set_peer_id(peer_id const& pid);

		// deprecated in 1.1.7. read settings_pack::peer_fingerprint instead
		TORRENT_DEPRECATED
		peer_id id() const;
#endif

#if TORRENT_ABI_VERSION == 1
		// deprecated in 1.2
		// sets the key sent to trackers. If it's not set, it is initialized
		// by libtorrent. The key may be used by the tracker to identify the
		// peer potentially across you changing your IP.
		void set_key(std::uint32_t key);
#endif

		// built-in peer classes
		static constexpr peer_class_t global_peer_class_id{0};
		static constexpr peer_class_t tcp_peer_class_id{1};
		static constexpr peer_class_t local_peer_class_id{2};

		// ``is_listening()`` will tell you whether or not the session has
		// successfully opened a listening port. If it hasn't, this function will
		// return false, and then you can set a new
		// settings_pack::listen_interfaces to try another interface and port to
		// bind to.
		//
		// ``listen_port()`` returns the port we ended up listening on.
		unsigned short listen_port() const;
		// unsigned short ssl_listen_port() const;
		bool is_listening() const;

		// Sets the peer class filter for this session. All new peer connections
		// will take this into account and be added to the peer classes specified
		// by this filter, based on the peer's IP address.
		//
		// The ip-filter essentially maps an IP -> uint32. Each bit in that 32
		// bit integer represents a peer class. The least significant bit
		// represents class 0, the next bit class 1 and so on.
		//
		// For more info, see ip_filter.
		//
		// For example, to make all peers in the range 200.1.1.0 - 200.1.255.255
		// belong to their own peer class, apply the following filter:
		//
		// .. code:: c++
		//
		// 	ip_filter f = ses.get_peer_class_filter();
		// 	peer_class_t my_class = ses.create_peer_class("200.1.x.x IP range");
		// 	f.add_rule(make_address("200.1.1.0"), make_address("200.1.255.255")
		// 		, 1 << static_cast<std::uint32_t>(my_class));
		// 	ses.set_peer_class_filter(f);
		//
		// This setting only applies to new connections, it won't affect existing
		// peer connections.
		//
		// This function is limited to only peer class 0-31, since there are only
		// 32 bits in the IP range mapping. Only the set bits matter; no peer
		// class will be removed from a peer as a result of this call, peer
		// classes are only added.
		//
		// The ``peer_class`` argument cannot be greater than 31. The bitmasks
		// representing peer classes in the ``peer_class_filter`` are 32 bits.
		//
		// The ``get_peer_class_filter()`` function returns the current filter.
		//
		// For more information, see peer-classes_.
		void set_peer_class_filter(ip_filter const& f);
		ip_filter get_peer_class_filter() const;

		// Sets and gets the *peer class type filter*. This is controls automatic
		// peer class assignments to peers based on what kind of socket it is.
		//
		// It does not only support assigning peer classes, it also supports
		// removing peer classes based on socket type.
		//
		// The order of these rules being applied are:
		//
		// 1. peer-class IP filter
		// 2. peer-class type filter, removing classes
		// 3. peer-class type filter, adding classes
		//
		// For more information, see peer-classes_.
		void set_peer_class_type_filter(peer_class_type_filter const& f);
		peer_class_type_filter get_peer_class_type_filter() const;

		// Creates a new peer class (see peer-classes_) with the given name. The
		// returned integer is the new peer class identifier. Peer classes may
		// have the same name, so each invocation of this function creates a new
		// class and returns a unique identifier.
		//
		// Identifiers are assigned from low numbers to higher. So if you plan on
		// using certain peer classes in a call to set_peer_class_filter(),
		// make sure to create those early on, to get low identifiers.
		//
		// For more information on peer classes, see peer-classes_.
		peer_class_t create_peer_class(char const* name);

		// This call dereferences the reference count of the specified peer
		// class. When creating a peer class it's automatically referenced by 1.
		// If you want to recycle a peer class, you may call this function. You
		// may only call this function **once** per peer class you create.
		// Calling it more than once for the same class will lead to memory
		// corruption.
		//
		// Since peer classes are reference counted, this function will not
		// remove the peer class if it's still assigned to torrents or peers. It
		// will however remove it once the last peer and torrent drops their
		// references to it.
		//
		// There is no need to call this function for custom peer classes. All
		// peer classes will be properly destructed when the session object
		// destructs.
		//
		// For more information on peer classes, see peer-classes_.
		void delete_peer_class(peer_class_t cid);

		// These functions queries information from a peer class and updates the
		// configuration of a peer class, respectively.
		//
		// ``cid`` must refer to an existing peer class. If it does not, the
		// return value of ``get_peer_class()`` is undefined.
		//
		// ``set_peer_class()`` sets all the information in the
		// peer_class_info object in the specified peer class. There is no
		// option to only update a single property.
		//
		// A peer or torrent belonging to more than one class, the highest
		// priority among any of its classes is the one that is taken into
		// account.
		//
		// For more information, see peer-classes_.
		peer_class_info get_peer_class(peer_class_t cid) const;
		void set_peer_class(peer_class_t cid, peer_class_info const& pci);

#if TORRENT_ABI_VERSION == 1
		// if the listen port failed in some way you can retry to listen on
		// another port- range with this function. If the listener succeeded and
		// is currently listening, a call to this function will shut down the
		// listen port and reopen it using these new properties (the given
		// interface and port range). As usual, if the interface is left as 0
		// this function will return false on failure. If it fails, it will also
		// generate alerts describing the error. It will return true on success.
		enum listen_on_flags_t
		{
			// this is always on starting with 0.16.2
			listen_reuse_address TORRENT_DEPRECATED_ENUM = 0x01,
			listen_no_system_port TORRENT_DEPRECATED_ENUM = 0x02
		};

		// deprecated in 0.16

		// specify which interfaces to bind outgoing connections to
		// This has been moved to a session setting
		TORRENT_DEPRECATED
		void use_interfaces(char const* interfaces);

		// instead of using this, specify listen interface and port in
		// the settings_pack::listen_interfaces setting
		TORRENT_DEPRECATED
		void listen_on(
			std::pair<int, int> const& port_range
			, error_code& ec
			, const char* net_interface = nullptr
			, int flags = 0);
#endif

		// delete the files belonging to the torrent from disk.
		// including the part-file, if there is one
		static constexpr remove_flags_t delete_files = 0_bit;

		// delete just the part-file associated with this torrent
		static constexpr remove_flags_t delete_partfile = 1_bit;

#if TORRENT_ABI_VERSION <= 2
		// this will add common extensions like ut_pex, ut_metadata, lt_tex
		// smart_ban and possibly others.
		TORRENT_DEPRECATED static constexpr session_flags_t add_default_plugins = 0_bit;
#endif

#if TORRENT_ABI_VERSION == 1
		// this will start features like DHT, local service discovery, UPnP
		// and NAT-PMP.
		TORRENT_DEPRECATED static constexpr session_flags_t start_default_features = 1_bit;
#endif

		// when set, the session will start paused. Call
		// session_handle::resume() to start
		static constexpr session_flags_t paused = 2_bit;

		// ``remove_torrent()`` will close all peer connections associated with
		// the torrent and tell the tracker that we've stopped participating in
		// the swarm. This operation cannot fail. When it completes, you will
		// receive a torrent_removed_alert.
		//
		// remove_torrent() is non-blocking, but will remove the torrent from the
		// session synchronously. Calling session_handle::add_torrent() immediately
		// afterward with the same torrent will succeed. Note that this creates a
		// new handle which is not equal to the removed one.
		//
		// The optional second argument ``options`` can be used to delete all the
		// files downloaded by this torrent. To do so, pass in the value
		// ``session_handle::delete_files``. Once the torrent is deleted, a
		// torrent_deleted_alert is posted.
		//
		// The torrent_handle remains valid for some time after remove_torrent() is
		// called. It will become invalid only after all libtorrent tasks (such as
		// I/O tasks) release their references to the torrent. Until this happens,
		// torrent_handle::is_valid() will return true, and other calls such
		// as torrent_handle::status() will succeed. Because of this, and because
		// remove_torrent() is non-blocking, the following sequence usually
		// succeeds (does not throw system_error):
		// .. code:: c++
		//
		//	session.remove_handle(handle);
		//	handle.save_resume_data();
		//
		// Note that when a queued or downloading torrent is removed, its position
		// in the download queue is vacated and every subsequent torrent in the
		// queue has their queue positions updated. This can potentially cause a
		// large state_update to be posted. When removing all torrents, it is
		// advised to remove them from the back of the queue, to minimize the
		// shifting.
		void remove_torrent(const torrent_handle&, remove_flags_t = {});

		// Applies the settings specified by the settings_pack ``s``. This is an
		// asynchronous operation that will return immediately and actually apply
		// the settings to the main thread of libtorrent some time later.
		void apply_settings(settings_pack const&);
		void apply_settings(settings_pack&&);
		settings_pack get_settings() const;

#if TORRENT_ABI_VERSION == 1



		// These functions sets and queries the proxy settings to be used for the
		// session.
		//
		// For more information on what settings are available for proxies, see
		// proxy_settings. If the session is not in anonymous mode, proxies that
		// aren't working or fail, will automatically be disabled and packets
		// will flow without using any proxy. If you want to enforce using a
		// proxy, even when the proxy doesn't work, enable anonymous_mode in
		// settings_pack.
		// TORRENT_DEPRECATED
		// void set_proxy(proxy_settings const& s);
		// TORRENT_DEPRECATED
		// proxy_settings proxy() const;

		// deprecated in 0.16
		// Get the number of uploads.
		TORRENT_DEPRECATED
		int num_uploads() const;

		// Get the number of connections. This number also contains the
		// number of half open connections.
		TORRENT_DEPRECATED
		int num_connections() const;

		// deprecated in 0.15.
		// TORRENT_DEPRECATED
		// void set_peer_proxy(proxy_settings const& s);
		// TORRENT_DEPRECATED
		// void set_web_seed_proxy(proxy_settings const& s);
		// TORRENT_DEPRECATED
		// void set_tracker_proxy(proxy_settings const& s);

		// TORRENT_DEPRECATED
		// proxy_settings peer_proxy() const;
		// TORRENT_DEPRECATED
		// proxy_settings web_seed_proxy() const;
		// TORRENT_DEPRECATED
		// proxy_settings tracker_proxy() const;



		// deprecated in 0.16
		TORRENT_DEPRECATED
		int upload_rate_limit() const;
		TORRENT_DEPRECATED
		int download_rate_limit() const;
		TORRENT_DEPRECATED
		int local_upload_rate_limit() const;
		TORRENT_DEPRECATED
		int local_download_rate_limit() const;
		TORRENT_DEPRECATED
		int max_half_open_connections() const;

		TORRENT_DEPRECATED
		void set_local_upload_rate_limit(int bytes_per_second);
		TORRENT_DEPRECATED
		void set_local_download_rate_limit(int bytes_per_second);
		TORRENT_DEPRECATED
		void set_upload_rate_limit(int bytes_per_second);
		TORRENT_DEPRECATED
		void set_download_rate_limit(int bytes_per_second);
		TORRENT_DEPRECATED
		void set_max_uploads(int limit);
		TORRENT_DEPRECATED
		void set_max_connections(int limit);
		TORRENT_DEPRECATED
		void set_max_half_open_connections(int limit);

		TORRENT_DEPRECATED
		int max_connections() const;
		TORRENT_DEPRECATED
		int max_uploads() const;

#endif

		// Alerts is the main mechanism for libtorrent to report errors and
		// events. ``pop_alerts`` fills in the vector passed to it with pointers
		// to new alerts. The session still owns these alerts and they will stay
		// valid until the next time ``pop_alerts`` is called. You may not delete
		// the alert objects.
		//
		// It is safe to call ``pop_alerts`` from multiple different threads, as
		// long as the alerts themselves are not accessed once another thread
		// calls ``pop_alerts``. Doing this requires manual synchronization
		// between the popping threads.
		//
		// ``wait_for_alert`` will block the current thread for ``max_wait`` time
		// duration, or until another alert is posted. If an alert is available
		// at the time of the call, it returns immediately. The returned alert
		// pointer is the head of the alert queue. ``wait_for_alert`` does not
		// pop alerts from the queue, it merely peeks at it. The returned alert
		// will stay valid until ``pop_alerts`` is called twice. The first time
		// will pop it and the second will free it.
		//
		// If there is no alert in the queue and no alert arrives within the
		// specified timeout, ``wait_for_alert`` returns nullptr.
		//
		// In the python binding, ``wait_for_alert`` takes the number of
		// milliseconds to wait as an integer.
		//
		// The alert queue in the session will not grow indefinitely. Make sure
		// to pop periodically to not miss notifications. To control the max
		// number of alerts that's queued by the session, see
		// ``settings_pack::alert_queue_size``.
		//
		// Some alerts are considered so important that they are posted even when
		// the alert queue is full. Some alerts are considered mandatory and cannot
		// be disabled by the ``alert_mask``. For instance,
		// save_resume_data_alert and save_resume_data_failed_alert are always
		// posted, regardless of the alert mask.
		//
		// To control which alerts are posted, set the alert_mask
		// (settings_pack::alert_mask).
		//
		// If the alert queue fills up to the point where alerts are dropped, this
		// will be indicated by a alerts_dropped_alert, which contains a bitmask
		// of which types of alerts were dropped. Generally it is a good idea to
		// make sure the alert queue is large enough, the alert_mask doesn't have
		// unnecessary categories enabled and to call pop_alert() frequently, to
		// avoid alerts being dropped.
		//
		// the ``set_alert_notify`` function lets the client set a function object
		// to be invoked every time the alert queue goes from having 0 alerts to
		// 1 alert. This function is called from within libtorrent, it may be the
		// main thread, or it may be from within a user call. The intention of
		// of the function is that the client wakes up its main thread, to poll
		// for more alerts using ``pop_alerts()``. If the notify function fails
		// to do so, it won't be called again, until ``pop_alerts`` is called for
		// some other reason. For instance, it could signal an eventfd, post a
		// message to an HWND or some other main message pump. The actual
		// retrieval of alerts should not be done in the callback. In fact, the
		// callback should not block. It should not perform any expensive work.
		// It really should just notify the main application thread.
		//
		// The type of an alert is returned by the polymorphic function
		// ``alert::type()`` but can also be queries from a concrete type via
		// ``T::alert_type``, as a static constant.
		void pop_alerts(std::vector<alert*>* alerts);
		alert* wait_for_alert(time_duration max_wait);
		void set_alert_notify(std::function<void()> const& fun);

#if TORRENT_ABI_VERSION == 1
		// use the setting instead
		TORRENT_DEPRECATED
		size_t set_alert_queue_size_limit(size_t queue_size_limit_);

		// Changes the mask of which alerts to receive. By default only errors
		// are reported. ``m`` is a bitmask where each bit represents a category
		// of alerts.
		//
		// ``get_alert_mask()`` returns the current mask;
		//
		// See category_t enum for options.
		TORRENT_DEPRECATED
		void set_alert_mask(std::uint32_t m);
		TORRENT_DEPRECATED
		std::uint32_t get_alert_mask() const;

		// Starts and stops Local Service Discovery. This service will broadcast
		// the info-hashes of all the non-private torrents on the local network to
		// look for peers on the same swarm within multicast reach.
		//
		// deprecated. use settings_pack::enable_lsd instead
		TORRENT_DEPRECATED
		void start_lsd();
		TORRENT_DEPRECATED
		void stop_lsd();

		// Starts and stops the UPnP service. When started, the listen port and
		// the DHT port are attempted to be forwarded on local UPnP router
		// devices.
		//
		// The upnp object returned by ``start_upnp()`` can be used to add and
		// remove arbitrary port mappings. Mapping status is returned through the
		// portmap_alert and the portmap_error_alert. The object will be valid
		// until ``stop_upnp()`` is called. See upnp-and-nat-pmp_.
		//
		// deprecated. use settings_pack::enable_upnp instead
		TORRENT_DEPRECATED
		void start_upnp();
		TORRENT_DEPRECATED
		void stop_upnp();

		// Starts and stops the NAT-PMP service. When started, the listen port
		// and the DHT port are attempted to be forwarded on the router through
		// NAT-PMP.
		//
		// The natpmp object returned by ``start_natpmp()`` can be used to add
		// and remove arbitrary port mappings. Mapping status is returned through
		// the portmap_alert and the portmap_error_alert. The object will be
		// valid until ``stop_natpmp()`` is called. See upnp-and-nat-pmp_.
		//
		// deprecated. use settings_pack::enable_natpmp instead
		TORRENT_DEPRECATED
		void start_natpmp();
		TORRENT_DEPRECATED
		void stop_natpmp();
#endif

		// protocols used by add_port_mapping()
		static constexpr portmap_protocol udp = portmap_protocol::udp;
		static constexpr portmap_protocol tcp = portmap_protocol::tcp;

		// add_port_mapping adds one or more port forwards on UPnP and/or NAT-PMP,
		// whichever is enabled. A mapping is created for each listen socket
		// in the session. The return values are all handles referring to the
		// port mappings that were just created. Pass them to delete_port_mapping()
		// to remove them.
		std::vector<port_mapping_t> add_port_mapping(portmap_protocol t, int external_port, int local_port);
		void delete_port_mapping(port_mapping_t handle);

		// This option indicates if the ports are mapped using natpmp
		// and upnp. If mapping was already made, they are deleted and added
		// again. This only works if natpmp and/or upnp are configured to be
		// enable.
		static constexpr reopen_network_flags_t reopen_map_ports = 0_bit;

		// Instructs the session to reopen all listen and outgoing sockets.
		//
		// It's useful in the case your platform doesn't support the built in
		// IP notifier mechanism, or if you have a better more reliable way to
		// detect changes in the IP routing table.
		void reopen_network_sockets(reopen_network_flags_t options = reopen_map_ports);

		// This function is intended only for use by plugins. This type does
		// not have a stable API and should be relied on as little as possible.
		std::shared_ptr<aux::session_impl> native_handle() const
		{ return m_impl.lock(); }

	private:

		template <typename Fun, typename... Args>
		void async_call(Fun f, Args&&... a) const;

		template <typename Fun, typename... Args>
		void sync_call(Fun f, Args&&... a) const;

		template <typename Ret, typename Fun, typename... Args>
		Ret sync_call_ret(Fun f, Args&&... a) const;

		explicit session_handle(std::weak_ptr<aux::session_impl> impl)
			: m_impl(std::move(impl))
		{}

		std::weak_ptr<aux::session_impl> m_impl;
	};

} // namespace libtorrent

#endif // TORRENT_SESSION_HANDLE_HPP_INCLUDED
