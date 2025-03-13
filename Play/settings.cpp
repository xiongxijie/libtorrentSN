#include "libtorrent/settings_pack.hpp"
#include "libtorrent/alert.hpp"
#include "settings.hpp"

using namespace lt;

lt::settings_pack settings()
{
	alert_category_t const mask =
		alert_category::error
		| alert_category::peer
		| alert_category::port_mapping
		| alert_category::storage
		| alert_category::tracker
		| alert_category::connect
		| alert_category::status
		//$| alert_category::ip_block
		| alert_category::session_log
		| alert_category::torrent_log
		| alert_category::peer_log
		| alert_category::incoming_request
		| alert_category::port_mapping_log
		| alert_category::file_progress
		| alert_category::piece_progress;

	settings_pack pack;
	pack.set_bool(settings_pack::enable_lsd, true);
	pack.set_bool(settings_pack::enable_natpmp, true);
	pack.set_bool(settings_pack::enable_upnp, true);
	// pack.set_bool(settings_pack::enable_dht, false);
	// pack.set_str(settings_pack::dht_bootstrap_nodes, "");

	// pack.set_bool(settings_pack::prefer_rc4, false);
	// pack.set_int(settings_pack::in_enc_policy, settings_pack::pe_forced);
	// pack.set_int(settings_pack::out_enc_policy, settings_pack::pe_forced);
	// pack.set_int(settings_pack::allowed_enc_level, settings_pack::pe_both);
#if TORRENT_ABI_VERSION == 1
	pack.set_bool(settings_pack::rate_limit_utp, true);
#endif

	pack.set_int(settings_pack::alert_mask, mask);

// #ifndef TORRENT_BUILD_SIMULATOR
// 	pack.set_bool(settings_pack::allow_multiple_connections_per_ip, true);
// #else
	// we use 0 threads (disk I/O operations will be performed in the network
	// thread) to be simulator friendly.
	pack.set_int(settings_pack::aio_threads, 0);
	pack.set_int(settings_pack::hashing_threads, 0);
// #endif

#if TORRENT_ABI_VERSION == 1
	pack.set_int(settings_pack::half_open_limit, 1);
#endif

	return pack;
}

