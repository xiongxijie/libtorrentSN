/*

Copyright (c) 2006-2022, Arvid Norberg
Copyright (c) 2007, Un Shyam
Copyright (c) 2015, Mikhail Titov
Copyright (c) 2016-2020, Alden Torres
Copyright (c) 2016-2017, Andrei Kurushin
Copyright (c) 2016-2018, Pavel Pimenov
Copyright (c) 2016-2020, Steven Siloti
Copyright (c) 2017, Antoine Dahan
Copyright (c) 2018, Greg Hazel
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

#include "libtorrent/config.hpp"

#include <memory> // unique_ptr
#include <vector>
#include <functional>

#ifndef TORRENT_DISABLE_LOGGING
#include "libtorrent/hex.hpp" // to_hex
#endif

#include "libtorrent/bt_peer_connection.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/identify_client.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/aux_/invariant_check.hpp"
#include "libtorrent/io.hpp"
#include "libtorrent/aux_/io.hpp"
#include "libtorrent/socket_io.hpp"
#include "libtorrent/extensions.hpp"
#include "libtorrent/aux_/session_interface.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/peer_info.hpp"
#include "libtorrent/random.hpp"
#include "libtorrent/aux_/alloca.hpp"
#include "libtorrent/aux_/socket_type.hpp"
// #include "libtorrent/aux_/merkle.hpp"
#include "libtorrent/performance_counters.hpp" // for counters
#include "libtorrent/aux_/alert_manager.hpp" // for alert_manager
#include "libtorrent/string_util.hpp" // for search
#include "libtorrent/aux_/generate_peer_id.hpp"
#include "libtorrent/aux_/ip_helpers.hpp"


namespace libtorrent {


#ifndef TORRENT_DISABLE_EXTENSIONS
	bool ut_pex_peer_store::was_introduced_by(tcp::endpoint const &ep)
	{
		if (aux::is_v4(ep))
		{
			peers4_t::value_type const v(ep.address().to_v4().to_bytes(), ep.port());
			auto const i = std::lower_bound(m_peers.begin(), m_peers.end(), v);
			return i != m_peers.end() && *i == v;
		}
		else
		{
			peers6_t::value_type const v(ep.address().to_v6().to_bytes(), ep.port());
			auto const i = std::lower_bound(m_peers6.begin(), m_peers6.end(), v);
			return i != m_peers6.end() && *i == v;
		}
	}
#endif // TORRENT_DISABLE_EXTENSIONS

	bt_peer_connection::bt_peer_connection(peer_connection_args& pack)
		: peer_connection(pack)
		, m_supports_extensions(false)
		, m_supports_fast(false)
		, m_sent_bitfield(false)
		, m_sent_handshake(false)
		, m_sent_allowed_fast(false)
		, m_our_peer_id(pack.our_peer_id)
	{
#ifndef TORRENT_DISABLE_LOGGING
		peer_log(peer_log_alert::info, "CONSTRUCT", "bt_peer_connection");
#endif

		m_reserved_bits.fill(0);
	}

	void bt_peer_connection::start()
	{
		peer_connection::start();

		// start in the state where we are trying to read the
		// handshake from the other side
		m_recv_buffer.reset(20);
		setup_receive();
	}

	bt_peer_connection::~bt_peer_connection() = default;


	void bt_peer_connection::on_connected()
	{
		if (is_disconnecting()) return;

		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);

		if (t->graceful_pause())
		{
#ifndef TORRENT_DISABLE_LOGGING
			peer_log(peer_log_alert::info, "ON_CONNECTED", "graceful-paused");
#endif
			disconnect(errors::torrent_paused, operation_t::bittorrent);
			return;
		}

		// make sure are much as possible of the response ends up in the same
		// packet, or at least back-to-back packets
		cork c_(*this);



		{
			write_handshake();

			TORRENT_ASSERT(m_sent_handshake);
			// start in the state where we are trying to read the
			// handshake from the other side
			m_recv_buffer.reset(20);
			setup_receive();
		}
	}

	void bt_peer_connection::on_metadata()
	{
#ifndef TORRENT_DISABLE_LOGGING
		peer_log(peer_log_alert::info, "ON_METADATA");
#endif

		disconnect_if_redundant();
		if (m_disconnecting) return;

		if (!m_sent_handshake) return;
		// we're still waiting to fully handshake with this peer. At the end of
		// the handshake we'll send the bitfield and dht port anyway. It's too
		// early to do now
		if (static_cast<int>(m_state)
			< static_cast<int>(state_t::read_packet_size))
		{
			return;
		}

		// connections that are still in the handshake
		// will send their bitfield when the handshake
		// is done
		std::shared_ptr<torrent> t = associated_torrent().lock();
// #ifndef TORRENT_DISABLE_SHARE_MODE
// 		if (!t->share_mode())
// #endif
		{
			bool const upload_only_enabled = t->is_upload_only()
// #ifndef TORRENT_DISABLE_SUPERSEEDING
// 				&& !t->super_seeding()
// #endif
				;
			send_upload_only(upload_only_enabled);
		}

		if (m_sent_bitfield) return;

		TORRENT_ASSERT(t);
		write_bitfield();
		TORRENT_ASSERT(m_sent_bitfield);
		// maybe_send_hash_request();
	}


	template<class F, typename... Args>
	void bt_peer_connection::extension_notify(F message, Args... args)
	{
#ifndef TORRENT_DISABLE_EXTENSIONS
		for (auto const& e : m_extensions)
		{
			(*e.*message)(args...);
		}
#endif
	}

	void bt_peer_connection::write_have_all()
	{
		INVARIANT_CHECK;

		m_sent_bitfield = true;
#ifndef TORRENT_DISABLE_LOGGING
		peer_log(peer_log_alert::outgoing_message, "HAVE_ALL");
#endif
		send_message(msg_have_all, counters::num_outgoing_have_all);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_have_all);
#endif
	}

	void bt_peer_connection::write_have_none()
	{
		INVARIANT_CHECK;
		m_sent_bitfield = true;
#ifndef TORRENT_DISABLE_LOGGING
		peer_log(peer_log_alert::outgoing_message, "HAVE_NONE");
#endif
		send_message(msg_have_none, counters::num_outgoing_have_none);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_have_none);
#endif
	}

	void bt_peer_connection::write_reject_request(peer_request const& r)
	{
		INVARIANT_CHECK;

		stats_counters().inc_stats_counter(counters::piece_rejects);

		if (!m_supports_fast) return;

#ifndef TORRENT_DISABLE_LOGGING
		peer_log(peer_log_alert::outgoing_message, "REJECT_PIECE"
			, "piece: %d | s: %d | l: %d", static_cast<int>(r.piece)
			, r.start, r.length);
#endif

		send_message(msg_reject_request, counters::num_outgoing_reject
			, static_cast<int>(r.piece), r.start, r.length);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_reject_request, r);
#endif
	}

	void bt_peer_connection::write_allow_fast(piece_index_t const piece)
	{
		INVARIANT_CHECK;

		if (!m_supports_fast) return;

#ifndef TORRENT_DISABLE_LOGGING
		peer_log(peer_log_alert::outgoing_message, "ALLOWED_FAST", "%d"
			, static_cast<int>(piece));
#endif

		TORRENT_ASSERT(associated_torrent().lock()->valid_metadata());

		send_message(msg_allowed_fast, counters::num_outgoing_allowed_fast
			, static_cast<int>(piece));

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_allow_fast, piece);
#endif
	}

	void bt_peer_connection::write_suggest(piece_index_t const piece)
	{
		INVARIANT_CHECK;

		if (!m_supports_fast) return;

#if TORRENT_USE_ASSERTS
		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);
		TORRENT_ASSERT(t->valid_metadata());
#endif

#ifndef TORRENT_DISABLE_LOGGING
		if (should_log(peer_log_alert::outgoing_message))
		{
#if !TORRENT_USE_ASSERTS
			std::shared_ptr<torrent> t = associated_torrent().lock();
#endif
			peer_log(peer_log_alert::outgoing_message, "SUGGEST"
				, "piece: %d num_peers: %d", static_cast<int>(piece)
				, t->has_picker() ? t->picker().get_availability(piece) : -1);
		}
#endif

		send_message(msg_suggest_piece, counters::num_outgoing_suggest
			, static_cast<int>(piece));

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_suggest, piece);
#endif
	}

	void bt_peer_connection::get_specific_peer_info(peer_info& p) const
	{
		TORRENT_ASSERT(!associated_torrent().expired());

		if (is_interesting()) p.flags |= peer_info::interesting;
		if (is_choked()) p.flags |= peer_info::choked;
		if (is_peer_interested()) p.flags |= peer_info::remote_interested;
		if (has_peer_choked()) p.flags |= peer_info::remote_choked;
		if (support_extensions()) p.flags |= peer_info::supports_extensions;
		if (is_outgoing()) p.flags |= peer_info::local_connection;
		if (is_utp(get_socket())) p.flags |= peer_info::utp_socket;
		// if (is_ssl(get_socket())) p.flags |= peer_info::ssl_socket;



		if (!is_connecting() && in_handshake())
			p.flags |= peer_info::handshake;
		if (is_connecting()) p.flags |= peer_info::connecting;

		p.client = m_client_version;
		p.connection_type = peer_info::standard_bittorrent;
	}

	bool bt_peer_connection::in_handshake() const
	{
		// this returns true until we have received a handshake
		// and until we have send our handshake
		return !m_sent_handshake || m_state < state_t::read_packet_size;
	}


	void bt_peer_connection::write_handshake()
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(!m_sent_handshake);
		m_sent_handshake = true;

		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);

		// add handshake to the send buffer
		static const char version_string[] = "BitTorrent protocol";
		const int string_len = sizeof(version_string) - 1;

		char handshake[1 + string_len + 8 + 20 + 20];
		char* ptr = handshake;
		// length of version string
		aux::write_uint8(string_len, ptr);
		// protocol identifier
		std::memcpy(ptr, version_string, string_len);
		ptr += string_len;
		// 8 zeroes
		std::memset(ptr, 0, 8);


		// we support extensions
		*(ptr + 5) |= 0x10;

		// we support FAST extension
		*(ptr + 7) |= 0x04;

		// this is a v1 peer in a hybrid torrent
		// indicate that we support upgrading to v2
		// if (!peer_info_struct()->protocol_v2 && t->info_hash().has_v2())
		// {
		// 	*(ptr + 7) |= 0x10;
		// }

#ifndef TORRENT_DISABLE_LOGGING
		if (should_log(peer_log_alert::outgoing_message))
		{
			std::string bitmask;
			for (int k = 0; k < 8; ++k)
			{
				for (int j = 0; j < 8; ++j)
				{
					if (ptr[k] & (0x80 >> j)) bitmask += '1';
					else bitmask += '0';
				}
			}
			peer_log(peer_log_alert::outgoing_message, "EXTENSIONS"
				, "%s", bitmask.c_str());
		}
#endif
		ptr += 8;

		// info hash
		sha1_hash const& ih = associated_info_hash();
		std::memcpy(ptr, ih.data(), ih.size());
		ptr += 20;

		std::memcpy(ptr, m_our_peer_id.data(), 20);

		TORRENT_ASSERT(!ih.is_all_zeros());

#ifndef TORRENT_DISABLE_LOGGING
		if (should_log(peer_log_alert::outgoing))
		{
			peer_log(peer_log_alert::outgoing, "HANDSHAKE"
				, "sent peer_id: %s client: %s"
				, aux::to_hex(m_our_peer_id).c_str(), identify_client(m_our_peer_id).c_str());
		}
		if (should_log(peer_log_alert::outgoing_message))
		{
			peer_log(peer_log_alert::outgoing_message, "HANDSHAKE"
				, "ih: %s", aux::to_hex(ih).c_str());
		}
#endif
		send_buffer(handshake);
	}

	piece_block_progress bt_peer_connection::downloading_piece_progress() const
	{
		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);

		span<char const> recv_buffer = m_recv_buffer.get();
		// are we currently receiving a 'piece' message?
		if (m_state != state_t::read_packet
			|| int(recv_buffer.size()) <= 9
			|| recv_buffer[0] != msg_piece)
			return {};

		const char* ptr = recv_buffer.data() + 1;
		peer_request r;
		r.piece = piece_index_t(aux::read_int32(ptr));
		r.start = aux::read_int32(ptr);
		r.length = m_recv_buffer.packet_size() - 9;

		// is any of the piece message header data invalid?
		if (!validate_piece_request(r)) return {};

		piece_block_progress p;

		p.piece_index = r.piece;
		p.block_index = r.start / t->block_size();
		p.bytes_downloaded = int(recv_buffer.size()) - 9;
		p.full_block_bytes = r.length;

		return p;
	}

	// message handlers

	// -----------------------------
	// ----------- CHOKE -----------
	// -----------------------------

	void bt_peer_connection::on_choke(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);
		received_bytes(0, received);
		if (m_recv_buffer.packet_size() != 1)
		{
			disconnect(errors::invalid_choke, operation_t::bittorrent, peer_error);
			return;
		}
		if (!m_recv_buffer.packet_finished()) return;

		incoming_choke();
		if (is_disconnecting()) return;
		if (!m_supports_fast)
		{
			// we just got choked, and the peer that choked use
			// doesn't support fast extensions, so we have to
			// assume that the choke message implies that all
			// of our requests are rejected. Go through them and
			// pretend that we received reject request messages
			std::shared_ptr<torrent> t = associated_torrent().lock();
			TORRENT_ASSERT(t);
			auto const dlq = download_queue();
			for (pending_block const& pb : dlq)
			{
				peer_request r;
				r.piece = pb.block.piece_index;
				r.start = pb.block.block_index * t->block_size();
				r.length = t->block_size();
				// if it's the last piece, make sure to
				// set the length of the request to not
				// exceed the end of the torrent. This is
				// necessary in order to maintain a correct
				// m_outstanding_bytes
				if (r.piece == t->torrent_file().last_piece())
				{
					r.length = std::min(t->torrent_file().piece_size(
						r.piece) - r.start, r.length);
				}
				incoming_reject_request(r);
			}
		}
	}

	// -----------------------------
	// ---------- UNCHOKE ----------
	// -----------------------------

	void bt_peer_connection::on_unchoke(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);
		received_bytes(0, received);
		if (m_recv_buffer.packet_size() != 1)
		{
			disconnect(errors::invalid_unchoke, operation_t::bittorrent, peer_error);
			return;
		}
		if (!m_recv_buffer.packet_finished()) return;

		incoming_unchoke();
	}

	// -----------------------------
	// -------- INTERESTED ---------
	// -----------------------------

	void bt_peer_connection::on_interested(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);
		received_bytes(0, received);
		if (m_recv_buffer.packet_size() != 1)
		{
			disconnect(errors::invalid_interested, operation_t::bittorrent, peer_error);
			return;
		}
		if (!m_recv_buffer.packet_finished()) return;

		// we defer sending the allowed set until the peer says it's interested in
		// us. This saves some bandwidth and allows us to omit messages for pieces
		// that the peer already has
		if (!m_sent_allowed_fast && m_supports_fast)
		{
			m_sent_allowed_fast = true;
			send_allowed_set();
		}

		incoming_interested();
	}

	// -----------------------------
	// ------ NOT INTERESTED -------
	// -----------------------------

	void bt_peer_connection::on_not_interested(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);
		received_bytes(0, received);
		if (m_recv_buffer.packet_size() != 1)
		{
			disconnect(errors::invalid_not_interested, operation_t::bittorrent, peer_error);
			return;
		}
		if (!m_recv_buffer.packet_finished()) return;

		incoming_not_interested();
	}

	// -----------------------------
	// ----------- HAVE ------------
	// -----------------------------

	void bt_peer_connection::on_have(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);
		received_bytes(0, received);
		if (m_recv_buffer.packet_size() != 5)
		{
			disconnect(errors::invalid_have, operation_t::bittorrent, peer_error);
			return;
		}
		if (!m_recv_buffer.packet_finished()) return;

		span<char const> recv_buffer = m_recv_buffer.get();

		const char* ptr = recv_buffer.data() + 1;
		piece_index_t const index(aux::read_int32(ptr));

		incoming_have(index);
		// maybe_send_hash_request();
	}

	// -----------------------------
	// --------- BITFIELD ----------
	// -----------------------------

	void bt_peer_connection::on_bitfield(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);

		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);

		received_bytes(0, received);
		// if we don't have the metadata, we cannot
		// verify the bitfield size
		if (t->valid_metadata()
			&& m_recv_buffer.packet_size() - 1 != (t->torrent_file().num_pieces() + CHAR_BIT - 1) / CHAR_BIT)
		{
			disconnect(errors::invalid_bitfield_size, operation_t::bittorrent, peer_error);
			return;
		}

		if (!m_recv_buffer.packet_finished()) return;

		span<char const> recv_buffer = m_recv_buffer.get();

		typed_bitfield<piece_index_t> bits;
		bits.assign(recv_buffer.data() + 1
			, t->valid_metadata()?get_bitfield().size():(m_recv_buffer.packet_size()-1)*CHAR_BIT);

		incoming_bitfield(bits);
	}

	// -----------------------------
	// ---------- REQUEST ----------
	// -----------------------------

	void bt_peer_connection::on_request(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);
		received_bytes(0, received);
		if (m_recv_buffer.packet_size() != 13)
		{
			disconnect(errors::invalid_request, operation_t::bittorrent, peer_error);
			return;
		}
		if (!m_recv_buffer.packet_finished()) return;

		span<char const> recv_buffer = m_recv_buffer.get();

		peer_request r;
		const char* ptr = recv_buffer.data() + 1;
		r.piece = piece_index_t(aux::read_int32(ptr));
		r.start = aux::read_int32(ptr);
		r.length = aux::read_int32(ptr);

		incoming_request(r);
	}

	// -----------------------------
	// ----------- PIECE -----------
	// -----------------------------

	void bt_peer_connection::on_piece(int const received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);

		span<char const> recv_buffer = m_recv_buffer.get();
		int const recv_pos = m_recv_buffer.pos();

		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);
		if (recv_pos == 1)
		{
			if (m_recv_buffer.packet_size() - 9 > t->block_size())
			{
				received_bytes(0, received);
				disconnect(errors::packet_too_large, operation_t::bittorrent, peer_error);
				return;
			}
		}
		// classify the received data as protocol chatter
		// or data payload for the statistics
		int piece_bytes = 0;

		int const header_size = 9;

		peer_request p;

		if (recv_pos >= header_size)
		{
			const char* ptr = recv_buffer.data() + 1;
			p.piece = piece_index_t(aux::read_int32(ptr));
			p.start = aux::read_int32(ptr);
			p.length = m_recv_buffer.packet_size() - header_size;
		}
		else
		{
			p.piece = piece_index_t(0);
			p.start = 0;
			p.length = 0;
		}

		if (recv_pos <= header_size)
		{
			// only received protocol data
			received_bytes(0, received);
		}
		else if (recv_pos - received >= header_size)
		{
			// only received payload data
			received_bytes(received, 0);
			piece_bytes = received;
		}
		else
		{
			// received a bit of both
			TORRENT_ASSERT(recv_pos - received < header_size);
			TORRENT_ASSERT(recv_pos > header_size);
			TORRENT_ASSERT(header_size - (recv_pos - received) <= header_size);
			received_bytes(
				recv_pos - header_size
				, header_size - (recv_pos - received));
			piece_bytes = recv_pos - header_size;
		}

		if (recv_pos < header_size) return;

#ifndef TORRENT_DISABLE_LOGGING
//			peer_log(peer_log_alert::incoming_message, "PIECE_FRAGMENT", "p: %d start: %d length: %d"
//				, p.piece, p.start, p.length);
#endif

		if (recv_pos - received < header_size)
		{
			// call this once, the first time the entire header
			// has been received
			start_receive_piece(p);
			if (is_disconnecting()) return;
		}

		incoming_piece_fragment(piece_bytes);
		if (!m_recv_buffer.packet_finished()) return;

		incoming_piece(p, recv_buffer.data() + header_size);
		// maybe_send_hash_request();
	}

	// -----------------------------
	// ---------- CANCEL -----------
	// -----------------------------

	void bt_peer_connection::on_cancel(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);
		received_bytes(0, received);
		if (m_recv_buffer.packet_size() != 13)
		{
			disconnect(errors::invalid_cancel, operation_t::bittorrent, peer_error);
			return;
		}
		if (!m_recv_buffer.packet_finished()) return;

		span<char const> recv_buffer = m_recv_buffer.get();

		peer_request r;
		const char* ptr = recv_buffer.data() + 1;
		r.piece = piece_index_t(aux::read_int32(ptr));
		r.start = aux::read_int32(ptr);
		r.length = aux::read_int32(ptr);

		incoming_cancel(r);
	}

// 	void bt_peer_connection::on_hash_request(int received)
// 	{
// 		INVARIANT_CHECK;

// 		TORRENT_ASSERT(received >= 0);
// 		received_bytes(0, received);

// 		if (!peer_info_struct()->protocol_v2)
// 		{
// 			disconnect(errors::invalid_message, operation_t::bittorrent, peer_error);
// 			return;
// 		}

// 		if (m_recv_buffer.packet_size() != 1 + 32 + 4 + 4 + 4 + 4)
// 		{
// 			disconnect(errors::invalid_hash_request, operation_t::bittorrent, peer_connection_interface::peer_error);
// 			return;
// 		}
// 		if (!m_recv_buffer.packet_finished()) return;

// 		std::shared_ptr<torrent> t = associated_torrent().lock();
// 		TORRENT_ASSERT(t);

// 		auto const& files = t->torrent_file().files();

// 		span<char const> recv_buffer = m_recv_buffer.get();
// 		const char* ptr = recv_buffer.begin() + 1;

// 		auto const file_root = sha256_hash(ptr);
// 		file_index_t const file_index = files.file_index_for_root(file_root);
// 		ptr += sha256_hash::size();
// 		int const base = aux::read_int32(ptr);
// 		int const index = aux::read_int32(ptr);
// 		int const count = aux::read_int32(ptr);
// 		int const proof_layers = aux::read_int32(ptr);
// 		hash_request hr(file_index, base, index, count, proof_layers);

// #ifndef TORRENT_DISABLE_LOGGING
// 		if (should_log(peer_log_alert::incoming_message))
// 		{
// 			peer_log(peer_log_alert::incoming_message, "HASH_REQUEST"
// 				, "file: %d base: %d idx: %d cnt: %d proofs: %d"
// 				, static_cast<int>(hr.file), hr.base, hr.index, hr.count, hr.proof_layers);
// 		}
// #endif

// 		if (!validate_hash_request(hr, files))
// 		{
// 			write_hash_reject(hr, file_root);
// 			return;
// 		}

// 		std::vector<sha256_hash> hashes = t->get_hashes(hr);

// 		if (hashes.empty())
// 		{
// 			write_hash_reject(hr, file_root);
// 			return;
// 		}

// 		write_hashes(hr, hashes);
// 	}

// 	void bt_peer_connection::on_hashes(int received)
// 	{
// 		INVARIANT_CHECK;

// 		TORRENT_ASSERT(received >= 0);
// 		received_bytes(0, received);

// 		if (!peer_info_struct()->protocol_v2)
// 		{
// 			disconnect(errors::invalid_message, operation_t::bittorrent, peer_error);
// 			return;
// 		}

// 		std::shared_ptr<torrent> t = associated_torrent().lock();
// 		TORRENT_ASSERT(t);

// 		auto const& files = t->torrent_file().files();

// 		span<char const> recv_buffer = m_recv_buffer.get();

// 		int const header_size = 1 + 32 + 4 + 4 + 4 + 4;

// 		if (recv_buffer.size() < header_size)
// 		{
// 			return;
// 		}

// 		const char* ptr = recv_buffer.begin() + 1;

// 		auto const file_root = sha256_hash(ptr);
// 		file_index_t file_index{ -1 };
// 		for (file_index_t i : files.file_range())
// 		{
// 			if (files.root(i) == file_root)
// 			{
// 				file_index = i;
// 				break;
// 			}
// 		}
// 		ptr += sha256_hash::size();
// 		int const base = aux::read_int32(ptr);
// 		int const index = aux::read_int32(ptr);
// 		int const count = aux::read_int32(ptr);
// 		int const proof_layers = aux::read_int32(ptr);

// 		hash_request const hr(file_index, base, index, count, proof_layers);

// 		if (!validate_hash_request(hr, t->torrent_file().files()))
// 		{
// 			disconnect(errors::invalid_hashes, operation_t::bittorrent, peer_connection_interface::peer_error);
// 			return;
// 		}

// 		// subtract one because the the base layer doesn't count
// 		int const proof_hashes = std::max(0
// 			, proof_layers - (merkle_num_layers(merkle_num_leafs(count)) - 1));

// 		if (m_recv_buffer.packet_size() != header_size
// 			+ (count + proof_hashes) * int(sha256_hash::size()))
// 		{
// 			disconnect(errors::invalid_hashes, operation_t::bittorrent, peer_connection_interface::peer_error);
// 			return;
// 		}

// 		if (!m_recv_buffer.packet_finished()) return;

// 		auto new_end = std::remove(m_hash_requests.begin(), m_hash_requests.end(), hr);
// 		m_hash_requests.erase(new_end, m_hash_requests.end());

// 		std::vector<sha256_hash> hashes;
// 		while (ptr != recv_buffer.end())
// 		{
// 			hashes.emplace_back(ptr);
// 			ptr += sha256_hash::size();
// 		}

// #ifndef TORRENT_DISABLE_LOGGING
// 		if (should_log(peer_log_alert::incoming_message))
// 		{
// 			peer_log(peer_log_alert::incoming_message, "HASHES"
// 				, "file: %d base: %d idx: %d cnt: %d proofs: %d"
// 				, static_cast<int>(hr.file), hr.base, hr.index, hr.count, hr.proof_layers);
// 		}
// #endif

// 		if (!t->add_hashes(hr, hashes))
// 		{
// 			disconnect(errors::invalid_hashes, operation_t::bittorrent, peer_connection_interface::peer_error);
// 			return;
// 		}

// 		maybe_send_hash_request();
// 	}

// 	void bt_peer_connection::on_hash_reject(int received)
// 	{
// 		INVARIANT_CHECK;

// 		TORRENT_ASSERT(received >= 0);
// 		received_bytes(0, received);

// 		if (!peer_info_struct()->protocol_v2)
// 		{
// 			disconnect(errors::invalid_message, operation_t::bittorrent, peer_error);
// 			return;
// 		}

// 		if (m_recv_buffer.packet_size() != 1 + 32 + 4 + 4 + 4 + 4)
// 		{
// 			disconnect(errors::invalid_hash_reject, operation_t::bittorrent, peer_connection_interface::peer_error);
// 			return;
// 		}
// 		if (!m_recv_buffer.packet_finished()) return;

// 		std::shared_ptr<torrent> t = associated_torrent().lock();
// 		TORRENT_ASSERT(t);

// 		span<char const> recv_buffer = m_recv_buffer.get();
// 		const char* ptr = recv_buffer.begin() + 1;

// 		auto const file_root = sha256_hash(ptr);
// 		file_index_t const file_index = t->torrent_file().files().file_index_for_root(file_root);
// 		ptr += sha256_hash::size();
// 		int const base = aux::read_int32(ptr);
// 		int const index = aux::read_int32(ptr);
// 		int const count = aux::read_int32(ptr);
// 		int const proof_layers = aux::read_int32(ptr);
// 		hash_request hr(file_index, base, index, count, proof_layers);

// #ifndef TORRENT_DISABLE_LOGGING
// 		if (should_log(peer_log_alert::incoming_message))
// 		{
// 			peer_log(peer_log_alert::incoming_message, "HASH_REJECT"
// 				, "file: %d base: %d idx: %d cnt: %d proofs: %d"
// 				, static_cast<int>(hr.file), hr.base, hr.index, hr.count, hr.proof_layers);
// 		}
// #endif

// 		auto new_end = std::remove(m_hash_requests.begin(), m_hash_requests.end(), hr);
// 		if (new_end == m_hash_requests.end()) return;
// 		m_hash_requests.erase(new_end, m_hash_requests.end());

// 		t->hashes_rejected(hr);

// 		maybe_send_hash_request();
// 	}


	void bt_peer_connection::on_suggest_piece(int received)
	{
		INVARIANT_CHECK;

		received_bytes(0, received);
		if (!m_supports_fast || m_recv_buffer.packet_size() != 5)
		{
			disconnect(errors::invalid_suggest, operation_t::bittorrent, peer_error);
			return;
		}

		if (!m_recv_buffer.packet_finished()) return;

		span<char const> recv_buffer = m_recv_buffer.get();

		const char* ptr = recv_buffer.data() + 1;
		piece_index_t const piece(aux::read_int32(ptr));
		incoming_suggest(piece);
	}

	void bt_peer_connection::on_have_all(int received)
	{
		INVARIANT_CHECK;

		received_bytes(0, received);
		if (!m_supports_fast || m_recv_buffer.packet_size() != 1)
		{
			disconnect(errors::invalid_have_all, operation_t::bittorrent, peer_error);
			return;
		}
		incoming_have_all();
		// maybe_send_hash_request();
	}

	void bt_peer_connection::on_have_none(int received)
	{
		INVARIANT_CHECK;

		received_bytes(0, received);
		if (!m_supports_fast || m_recv_buffer.packet_size() != 1)
		{
			disconnect(errors::invalid_have_none, operation_t::bittorrent, peer_error);
			return;
		}
		incoming_have_none();
	}

	void bt_peer_connection::on_reject_request(int received)
	{
		INVARIANT_CHECK;

		received_bytes(0, received);
		if (!m_supports_fast || m_recv_buffer.packet_size() != 13)
		{
			disconnect(errors::invalid_reject, operation_t::bittorrent, peer_error);
			return;
		}

		if (!m_recv_buffer.packet_finished()) return;

		span<char const> recv_buffer = m_recv_buffer.get();

		peer_request r;
		const char* ptr = recv_buffer.data() + 1;
		r.piece = piece_index_t(aux::read_int32(ptr));
		r.start = aux::read_int32(ptr);
		r.length = aux::read_int32(ptr);

		incoming_reject_request(r);
	}

	void bt_peer_connection::on_allowed_fast(int received)
	{
		INVARIANT_CHECK;

		received_bytes(0, received);
		if (!m_supports_fast || m_recv_buffer.packet_size() != 5)
		{
			disconnect(errors::invalid_allow_fast, operation_t::bittorrent, peer_error);
			return;
		}

		if (!m_recv_buffer.packet_finished()) return;
		span<char const> recv_buffer = m_recv_buffer.get();
		const char* ptr = recv_buffer.data() + 1;
		piece_index_t const index(aux::read_int32(ptr));

		incoming_allowed_fast(index);
	}

	// -----------------------------
	// -------- RENDEZVOUS ---------
	// -----------------------------

	void bt_peer_connection::on_holepunch()
	{
		INVARIANT_CHECK;

		if (!m_recv_buffer.packet_finished()) return;

		// we can't accept holepunch messages from peers
		// that don't support the holepunch extension
		// because we wouldn't be able to respond
		if (m_holepunch_id == 0) return;

		span<char const> recv_buffer = m_recv_buffer.get();
		TORRENT_ASSERT(recv_buffer.front() == msg_extended);
		recv_buffer = recv_buffer.subspan(1);
		TORRENT_ASSERT(recv_buffer.front() == holepunch_msg);
		recv_buffer = recv_buffer.subspan(1);

		char const* ptr = recv_buffer.data();
		char const* const end = recv_buffer.data() + recv_buffer.size();

		// ignore invalid messages
		if (int(recv_buffer.size()) < 2) return;

		auto const msg_type = static_cast<hp_message>(aux::read_uint8(ptr));
		int const addr_type = aux::read_uint8(ptr);

		tcp::endpoint ep;

		if (addr_type == 0)
		{
			if (int(recv_buffer.size()) < 2 + 4 + 2) return;
			// IPv4 address
			ep = aux::read_v4_endpoint<tcp::endpoint>(ptr);
		}
		else if (addr_type == 1)
		{
			// IPv6 address
			if (int(recv_buffer.size()) < 2 + 16 + 2) return;
			ep = aux::read_v6_endpoint<tcp::endpoint>(ptr);
		}
		else
		{
#ifndef TORRENT_DISABLE_LOGGING
			if (should_log(peer_log_alert::incoming_message))
			{
				static const char* hp_msg_name[] = {"rendezvous", "connect", "failed"};
				peer_log(peer_log_alert::incoming_message, "HOLEPUNCH"
					, "msg: %s from %s to: unknown address type"
					, (static_cast<int>(msg_type) < 3
						? hp_msg_name[static_cast<int>(msg_type)]
						: "unknown message type")
					, print_address(remote().address()).c_str());
			}
#endif

			return; // unknown address type
		}

#ifndef TORRENT_DISABLE_LOGGING
		if (msg_type > hp_message::failed)
		{
			if (should_log(peer_log_alert::incoming_message))
			{
				peer_log(peer_log_alert::incoming_message, "HOLEPUNCH"
					, "msg: unknown message type (%d) to: %s"
					, static_cast<int>(msg_type)
					, print_address(ep.address()).c_str());
			}
			return;
		}
#endif

		std::shared_ptr<torrent> t = associated_torrent().lock();
		if (!t) return;

		switch (msg_type)
		{
			case hp_message::rendezvous: // rendezvous
			{
#ifndef TORRENT_DISABLE_LOGGING
				if (should_log(peer_log_alert::incoming_message))
				{
					peer_log(peer_log_alert::incoming_message, "HOLEPUNCH"
						, "msg: rendezvous to: %s", print_address(ep.address()).c_str());
				}
#endif
				// this peer is asking us to introduce it to
				// the peer at 'ep'. We need to find which of
				// our connections points to that endpoint
				bt_peer_connection* p = t->find_peer(ep);
				if (p == nullptr)
				{
					// we're not connected to this peer
					write_holepunch_msg(hp_message::failed, ep, hp_error::not_connected);
					break;
				}
				if (!p->supports_holepunch())
				{
					write_holepunch_msg(hp_message::failed, ep, hp_error::no_support);
					break;
				}
				if (p == this)
				{
					write_holepunch_msg(hp_message::failed, ep, hp_error::no_self);
					break;
				}

				write_holepunch_msg(hp_message::connect, ep);
				p->write_holepunch_msg(hp_message::connect, remote());
			} break;
			case hp_message::connect:
			{
				// add or find the peer with this endpoint
				torrent_peer* p = t->add_peer(ep, peer_info::pex);
				if (p == nullptr || p->connection)
				{
#ifndef TORRENT_DISABLE_LOGGING
					if (should_log(peer_log_alert::incoming_message))
					{
						peer_log(peer_log_alert::incoming_message, "HOLEPUNCH"
							, "msg:connect to: %s ERROR: failed to add peer"
							, print_address(ep.address()).c_str());
					}
#endif
					// we either couldn't add this peer, or it's
					// already connected. Just ignore the connect message
					break;
				}
				if (p->banned)
				{
#ifndef TORRENT_DISABLE_LOGGING
					if (should_log(peer_log_alert::incoming_message))
					{
						peer_log(peer_log_alert::incoming_message, "HOLEPUNCH"
							, "msg:connect to: %s ERROR: peer banned", print_address(ep.address()).c_str());
					}
#endif
					// this peer is banned, don't connect to it
					break;
				}
				// to make sure we use the uTP protocol
				p->supports_utp = true;
				// #error make sure we make this a connection candidate
				// in case it has too many failures for instance
				t->connect_to_peer(p, true);
				// mark this connection to be in holepunch mode
				// so that it will retry faster and stick to uTP while it's
				// retrying
				t->update_want_peers();
				if (p->connection)
					p->connection->set_holepunch_mode();
#ifndef TORRENT_DISABLE_LOGGING
				if (should_log(peer_log_alert::incoming_message))
				{
					peer_log(peer_log_alert::incoming_message, "HOLEPUNCH"
						, "msg:connect to: %s"
						, print_address(ep.address()).c_str());
				}
#endif
			} break;
			case hp_message::failed:
			{
				if (end - ptr < 4) return;
				std::uint32_t const error = aux::read_uint32(ptr);
#ifndef TORRENT_DISABLE_LOGGING
				if (should_log(peer_log_alert::incoming_message))
				{
					static char const* err_msg[] = {"no such peer", "not connected", "no support", "no self"};
					peer_log(peer_log_alert::incoming_message, "HOLEPUNCH"
						, "msg:failed ERROR: %d msg: %s", error
						, ((error > 0 && error < 5)?err_msg[error-1]:"unknown message id"));
				}
#endif
				// #error deal with holepunch errors
				(void)error;
			} break;
		}
	}

	void bt_peer_connection::write_holepunch_msg(hp_message const type
		, tcp::endpoint const& ep, hp_error const error)
	{
		char buf[35];
		char* ptr = buf + 6;
		aux::write_uint8(type, ptr);
		if (aux::is_v4(ep)) aux::write_uint8(0, ptr);
		else aux::write_uint8(1, ptr);
		aux::write_endpoint(ep, ptr);

#ifndef TORRENT_DISABLE_LOGGING
		if (should_log(peer_log_alert::outgoing_message))
		{
			static const char* hp_msg_name[] = {"rendezvous", "connect", "failed"};
			static const char* hp_error_string[] = {"", "no such peer", "not connected", "no support", "no self"};
			peer_log(peer_log_alert::outgoing_message, "HOLEPUNCH"
				, "msg: %s to: %s ERROR: %s"
				, (static_cast<std::uint8_t>(type) < 3
					? hp_msg_name[static_cast<std::uint8_t>(type)]
					: "unknown message type")
				, print_address(ep.address()).c_str()
				, hp_error_string[static_cast<int>(error)]);
		}
#endif
		if (type == hp_message::failed)
		{
			aux::write_uint32(static_cast<int>(error), ptr);
		}

		// write the packet length and type
		char* hdr = buf;
		aux::write_uint32(ptr - buf - 4, hdr);
		aux::write_uint8(msg_extended, hdr);
		aux::write_uint8(m_holepunch_id, hdr);

		TORRENT_ASSERT(ptr <= buf + sizeof(buf));

		send_buffer({buf, ptr - buf});

		stats_counters().inc_stats_counter(counters::num_outgoing_extended);
	}



// 	void bt_peer_connection::write_hash_request(hash_request const& req)
// 	{
// 		INVARIANT_CHECK;

// 		char buf[5 + sha256_hash::size() + 4 * 4];
// 		char* ptr = buf;
// 		aux::write_uint32(int(sizeof(buf) - 4), ptr);
// 		aux::write_uint8(msg_hash_request, ptr);

// 		auto t = associated_torrent().lock();
// 		if (!t) return;
// 		auto const& ti = t->torrent_file();
// 		auto const& fs = ti.files();
// 		auto const root = fs.root(req.file);

// 		ptr = std::copy(root.begin(), root.end(), ptr);

// 		TORRENT_ASSERT(validate_hash_request(req, t->torrent_file().files()));

// 		aux::write_uint32(req.base, ptr);
// 		aux::write_uint32(req.index, ptr);
// 		aux::write_uint32(req.count, ptr);
// 		aux::write_uint32(req.proof_layers, ptr);

// 		stats_counters().inc_stats_counter(counters::num_outgoing_hash_request);

// 		m_hash_requests.push_back(req);

// #ifndef TORRENT_DISABLE_LOGGING
// 		if (should_log(peer_log_alert::outgoing_message))
// 		{
// 			peer_log(peer_log_alert::outgoing_message, "HASH_REQUEST"
// 				, "file: %d base: %d idx: %d cnt: %d proofs: %d"
// 				, int(req.file), req.base, req.index, req.count, req.proof_layers);
// 		}
// #endif

// 		send_buffer(buf);
// 	}



// 	void bt_peer_connection::write_hashes(hash_request const& req, span<sha256_hash> hashes)
// 	{
// 		INVARIANT_CHECK;

// 		int const packet_size = int(5 + sha256_hash::size()
// 			+ 4 * 4
// 			+ sha256_hash::size() * hashes.size());
// 		TORRENT_ALLOCA(buf, char, packet_size);
// 		char* ptr = buf.data();
// 		aux::write_uint32(packet_size - 4, ptr);
// 		aux::write_uint8(msg_hashes, ptr);

// 		auto t = associated_torrent().lock();
// 		if (!t) return;
// 		auto const& ti = t->torrent_file();
// 		auto const& fs = ti.files();
// 		auto root = fs.root(req.file);

// 		ptr = std::copy(root.begin(), root.end(), ptr);

// 		aux::write_uint32(req.base, ptr);
// 		aux::write_uint32(req.index, ptr);
// 		aux::write_uint32(req.count, ptr);
// 		aux::write_uint32(req.proof_layers, ptr);

// 		for (auto const& h : hashes)
// 			ptr = std::copy(h.begin(), h.end(), ptr);

// 		stats_counters().inc_stats_counter(counters::num_outgoing_hashes);

// #ifndef TORRENT_DISABLE_LOGGING
// 		if (should_log(peer_log_alert::outgoing_message))
// 		{
// 			peer_log(peer_log_alert::outgoing_message, "HASHES"
// 				, "file: %d base: %d idx: %d cnt: %d proofs: %d"
// 				, static_cast<int>(req.file), req.base, req.index, req.count, req.proof_layers);
// 		}
// #endif

// 		send_buffer(buf);
// 	}



// 	void bt_peer_connection::write_hash_reject(hash_request const& req, sha256_hash const& root)
// 	{
// 		INVARIANT_CHECK;

// 		char buf[5 + sha256_hash::size() + 4 * 4];
// 		char* ptr = buf;
// 		aux::write_uint32(int(sizeof(buf) - 4), ptr);
// 		aux::write_uint8(msg_hash_reject, ptr);

// 		auto t = associated_torrent().lock();
// 		if (!t) return;
// 		ptr = std::copy(root.begin(), root.end(), ptr);

// 		aux::write_uint32(req.base, ptr);
// 		aux::write_uint32(req.index, ptr);
// 		aux::write_uint32(req.count, ptr);
// 		aux::write_uint32(req.proof_layers, ptr);

// 		stats_counters().inc_stats_counter(counters::num_outgoing_hash_reject);

// #ifndef TORRENT_DISABLE_LOGGING
// 		if (should_log(peer_log_alert::outgoing_message))
// 		{
// 			peer_log(peer_log_alert::outgoing_message, "HASH_REJECT"
// 				, "base: %d idx: %d cnt: %d proofs: %d"
// 				, req.base, req.index, req.count, req.proof_layers);
// 		}
// #endif

// 		send_buffer(buf);
// 	}

	// void bt_peer_connection::maybe_send_hash_request()
	// {
	// 	if (is_disconnecting() || m_hash_requests.size() > 1) return;
	// 	if (!peer_info_struct()->protocol_v2) return;

	// 	std::shared_ptr<torrent> t = associated_torrent().lock();
	// 	TORRENT_ASSERT(t);

	// 	if (!t->valid_metadata()) return;

	// 	auto req = t->pick_hashes(this);
	// 	if (req.count > 0) write_hash_request(req);
	// }

	// -----------------------------
	// --------- EXTENDED ----------
	// -----------------------------

	void bt_peer_connection::on_extended(int received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);
		received_bytes(0, received);
		if (m_recv_buffer.packet_size() < 2)
		{
			disconnect(errors::invalid_extended, operation_t::bittorrent, peer_error);
			return;
		}

		if (associated_torrent().expired())
		{
			disconnect(errors::invalid_extended, operation_t::bittorrent, peer_error);
			return;
		}

		span<char const> recv_buffer = m_recv_buffer.get();
		if (int(recv_buffer.size()) < 2) return;

		TORRENT_ASSERT(recv_buffer.front() == msg_extended);
		recv_buffer = recv_buffer.subspan(1);

		int const extended_id = aux::read_uint8(recv_buffer);

		if (extended_id == 0)
		{
			on_extended_handshake();
			disconnect_if_redundant();
			return;
		}

		if (extended_id == upload_only_msg)
		{
			if (!m_recv_buffer.packet_finished()) return;
			if (m_recv_buffer.packet_size() != 3)
			{
#ifndef TORRENT_DISABLE_LOGGING
				peer_log(peer_log_alert::incoming_message, "UPLOAD_ONLY"
					, "ERROR: unexpected packet size: %d", m_recv_buffer.packet_size());
#endif
				return;
			}
			bool const ul = aux::read_uint8(recv_buffer) != 0;
#ifndef TORRENT_DISABLE_LOGGING
			peer_log(peer_log_alert::incoming_message, "UPLOAD_ONLY"
				, "%s", (ul?"true":"false"));
#endif
			set_upload_only(ul);
			return;
		}

// #ifndef TORRENT_DISABLE_SHARE_MODE
// 		if (extended_id == share_mode_msg)
// 		{
// 			if (!m_recv_buffer.packet_finished()) return;
// 			if (m_recv_buffer.packet_size() != 3)
// 			{
// #ifndef TORRENT_DISABLE_LOGGING
// 				peer_log(peer_log_alert::incoming_message, "SHARE_MODE"
// 					, "ERROR: unexpected packet size: %d", m_recv_buffer.packet_size());
// #endif
// 				return;
// 			}
// 			bool sm = aux::read_uint8(recv_buffer) != 0;
// #ifndef TORRENT_DISABLE_LOGGING
// 			peer_log(peer_log_alert::incoming_message, "SHARE_MODE"
// 				, "%s", (sm?"true":"false"));
// #endif
// 			set_share_mode(sm);
// 			return;
// 		}
// #endif // TORRENT_DISABLE_SHARE_MODE

		if (extended_id == holepunch_msg)
		{
			if (!m_recv_buffer.packet_finished()) return;
#ifndef TORRENT_DISABLE_LOGGING
			peer_log(peer_log_alert::incoming_message, "HOLEPUNCH");
#endif
			on_holepunch();
			return;
		}

		if (extended_id == dont_have_msg)
		{
			if (!m_recv_buffer.packet_finished()) return;
			if (m_recv_buffer.packet_size() != 6)
			{
#ifndef TORRENT_DISABLE_LOGGING
				peer_log(peer_log_alert::incoming_message, "DONT_HAVE"
					, "ERROR: unexpected packet size: %d", m_recv_buffer.packet_size());
#endif
				return;
			}
			piece_index_t const piece(aux::read_int32(recv_buffer));
			incoming_dont_have(piece);
			return;
		}

#ifndef TORRENT_DISABLE_LOGGING
		if (m_recv_buffer.packet_finished())
			peer_log(peer_log_alert::incoming_message, "EXTENSION_MESSAGE"
				, "msg: %d size: %d", extended_id, m_recv_buffer.packet_size());
#endif

#ifndef TORRENT_DISABLE_EXTENSIONS
		for (auto const& e : m_extensions)
		{
			if (e->on_extended(m_recv_buffer.packet_size() - 2, extended_id
				, recv_buffer))
				return;
		}
#endif

		disconnect(errors::invalid_message, operation_t::bittorrent, peer_error);
	}

	void bt_peer_connection::on_extended_handshake()
	{
		if (!m_recv_buffer.packet_finished()) return;

		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);

		span<char const> recv_buffer = m_recv_buffer.get();

		error_code ec;
		int pos;
		bdecode_node root = bdecode(recv_buffer.subspan(2), ec, &pos);
		if (ec || root.type() != bdecode_node::dict_t)
		{
#ifndef TORRENT_DISABLE_LOGGING
			if (should_log(peer_log_alert::info))
			{
				peer_log(peer_log_alert::info, "EXTENSION_MESSAGE"
					, "invalid extended handshake. pos: %d %s"
					, pos, print_error(ec).c_str());
			}
#endif
			return;
		}

#ifndef TORRENT_DISABLE_LOGGING
		if (should_log(peer_log_alert::incoming_message))
		{
			peer_log(peer_log_alert::incoming_message, "EXTENDED_HANDSHAKE"
				, "%s", print_entry(root, true).c_str());
		}
#endif

#ifndef TORRENT_DISABLE_EXTENSIONS
		for (auto i = m_extensions.begin();
			!m_extensions.empty() && i != m_extensions.end();)
		{
			// a false return value means that the extension
			// isn't supported by the other end. So, it is removed.
			if (!(*i)->on_extension_handshake(root))
				i = m_extensions.erase(i);
			else
				++i;
		}
		if (is_disconnecting()) return;
#endif

		// upload_only
		if (bdecode_node const m = root.dict_find_dict("m"))
		{
			m_upload_only_id = std::uint8_t(m.dict_find_int_value("upload_only", 0));
			m_holepunch_id = std::uint8_t(m.dict_find_int_value("ut_holepunch", 0));
			m_dont_have_id = std::uint8_t(m.dict_find_int_value("lt_donthave", 0));
		}

		// there is supposed to be a remote listen port
		int const listen_port = int(root.dict_find_int_value("p"));
		if (listen_port > 0 && peer_info_struct() != nullptr)
		{
			t->update_peer_port(listen_port, peer_info_struct(), peer_info::incoming);
			received_listen_port();
			if (is_disconnecting()) return;
		}

		// there should be a version too
		// but where do we put that info?

		int const last_seen_complete = int(root.dict_find_int_value("complete_ago", -1));
		if (last_seen_complete >= 0) set_last_seen_complete(last_seen_complete);

		auto const client_info = root.dict_find_string_value("v");
		if (!client_info.empty())
		{
			m_client_version = client_info.to_string();
			// the client name is supposed to be UTF-8
			aux::verify_encoding(m_client_version);
		}

		int const reqq = int(root.dict_find_int_value("reqq"));
		if (reqq > 0) max_out_request_queue(reqq);

		if (root.dict_find_int_value("upload_only", 0))
			set_upload_only(true);

// #ifndef TORRENT_DISABLE_SHARE_MODE
// 		if (m_settings.get_bool(settings_pack::support_share_mode)
// 			&& root.dict_find_int_value("share_mode", 0))
// 			set_share_mode(true);
// #endif

		auto const myip = root.dict_find_string_value("yourip");
		if (!myip.empty())
		{
			if (myip.size() == std::tuple_size<address_v4::bytes_type>::value)
			{
				address_v4::bytes_type bytes;
				std::copy(myip.begin(), myip.end(), bytes.begin());
				m_ses.set_external_address(local_endpoint()
					, address_v4(bytes)
					, aux::session_interface::source_peer, remote().address());
			}
			else if (myip.size() == std::tuple_size<address_v6::bytes_type>::value)
			{
				address_v6::bytes_type bytes;
				std::copy(myip.begin(), myip.end(), bytes.begin());
				address_v6 ipv6_address(bytes);
				if (ipv6_address.is_v4_mapped())
					m_ses.set_external_address(local_endpoint()
						, make_address_v4(v4_mapped, ipv6_address)
						, aux::session_interface::source_peer, remote().address());
				else
					m_ses.set_external_address(local_endpoint()
						, ipv6_address
						, aux::session_interface::source_peer, remote().address());
			}
		}

		// if we're finished and this peer is uploading only
		// disconnect it
		if (t->is_finished() && upload_only()
			&& m_settings.get_bool(settings_pack::close_redundant_connections)
// #ifndef TORRENT_DISABLE_SHARE_MODE
// 			&& !t->share_mode()
// #endif
			&& can_disconnect(errors::upload_upload_connection))
			disconnect(errors::upload_upload_connection, operation_t::bittorrent);

		stats_counters().inc_stats_counter(counters::num_incoming_ext_handshake);
	}

	bool bt_peer_connection::dispatch_message(int const received)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(received >= 0);

		// this means the connection has been closed already
		if (associated_torrent().expired())
		{
			received_bytes(0, received);
			return false;
		}

		span<char const> recv_buffer = m_recv_buffer.get();

		TORRENT_ASSERT(int(recv_buffer.size()) >= 1);
		int const packet_type = static_cast<std::uint8_t>(recv_buffer[0]);

#if TORRENT_USE_ASSERTS
		std::int64_t const cur_payload_dl = statistics().last_payload_downloaded();
		std::int64_t const cur_protocol_dl = statistics().last_protocol_downloaded();
#endif

		// call the handler for this packet type
		switch (packet_type)
		{
			// original BitTorrent message
			case msg_choke: on_choke(received); break;
			case msg_unchoke: on_unchoke(received); break;
			case msg_interested: on_interested(received); break;
			case msg_not_interested: on_not_interested(received); break;
			case msg_have: on_have(received); break;
			case msg_bitfield: on_bitfield(received); break;
			case msg_request: on_request(received); break;
			case msg_piece: on_piece(received); break;
			case msg_cancel: on_cancel(received); break;


			// FAST extension messages
			case msg_suggest_piece: on_suggest_piece(received); break;
			case msg_have_all: on_have_all(received); break;
			case msg_have_none: on_have_none(received); break;
			case msg_reject_request: on_reject_request(received); break;
			case msg_allowed_fast: on_allowed_fast(received); break;
			case msg_extended: on_extended(received); break;
			// case msg_hash_request: on_hash_request(received); break;
			// case msg_hashes: on_hashes(received); break;
			// case msg_hash_reject: on_hash_reject(received); break;
			default:
			{
#ifndef TORRENT_DISABLE_EXTENSIONS
				for (auto const& e : m_extensions)
				{
					if (e->on_unknown_message(m_recv_buffer.packet_size(), packet_type
						, recv_buffer.subspan(1)))
						return m_recv_buffer.packet_finished();
				}
#endif
				received_bytes(0, received);
				disconnect(errors::invalid_message, operation_t::bittorrent, peer_error);
				return m_recv_buffer.packet_finished();
			}
		}

#if TORRENT_USE_ASSERTS
		TORRENT_ASSERT(statistics().last_payload_downloaded() - cur_payload_dl >= 0);
		TORRENT_ASSERT(statistics().last_protocol_downloaded() - cur_protocol_dl >= 0);
		std::int64_t const stats_diff = statistics().last_payload_downloaded()
			- cur_payload_dl + statistics().last_protocol_downloaded()
			- cur_protocol_dl;
		TORRENT_ASSERT(stats_diff == received);
#endif

		bool const finished = m_recv_buffer.packet_finished();

		if (finished)
		{
			// count this packet in the session stats counters
			int const counter = (packet_type <= (msg_cancel+1))
				? counters::num_incoming_choke + packet_type
				: (packet_type <= msg_allowed_fast)
				? counters::num_incoming_suggest + packet_type
				: counters::num_incoming_extended;

			stats_counters().inc_stats_counter(counter);
		}

		return finished;
	}

	void bt_peer_connection::write_upload_only(bool const enabled)
	{
		INVARIANT_CHECK;

// #if TORRENT_USE_ASSERTS && !defined TORRENT_DISABLE_SHARE_MODE
// 		std::shared_ptr<torrent> t = associated_torrent().lock();
// 		TORRENT_ASSERT(!t->share_mode());
// #endif

		if (m_upload_only_id == 0) return;

		// if we send upload-only, the other end is very likely to disconnect
		// us, at least if it's a seed. If we don't want to close redundant
		// connections, don't sent upload-only
		if (!m_settings.get_bool(settings_pack::close_redundant_connections)) return;

		char msg[7] = {0, 0, 0, 3, msg_extended};
		char* ptr = msg + 5;
		aux::write_uint8(m_upload_only_id, ptr);
		aux::write_uint8(enabled, ptr);
		send_buffer(msg);

		stats_counters().inc_stats_counter(counters::num_outgoing_extended);
	}

// #ifndef TORRENT_DISABLE_SHARE_MODE
// 	void bt_peer_connection::write_share_mode()
// 	{
// 		INVARIANT_CHECK;

// 		std::shared_ptr<torrent> t = associated_torrent().lock();
// 		if (m_share_mode_id == 0) return;

// 		char msg[7] = {0, 0, 0, 3, msg_extended};
// 		char* ptr = msg + 5;
// 		aux::write_uint8(m_share_mode_id, ptr);
// 		aux::write_uint8(t->share_mode(), ptr);
// 		send_buffer(msg);

// 		stats_counters().inc_stats_counter(counters::num_outgoing_extended);
// 	}
// #endif

	void bt_peer_connection::write_keepalive()
	{
		INVARIANT_CHECK;

		// Don't require the bitfield to have been sent at this point
		// the case where m_sent_bitfield may not be true is if the
		// torrent doesn't have any metadata, and a peer is timing out.
		// then the keep-alive message will be sent before the bitfield
		// this is a violation to the original protocol, but necessary
		// for the metadata extension.
		TORRENT_ASSERT(m_sent_handshake);

		static const char msg[] = {0,0,0,0};
		send_buffer(msg);
	}

	void bt_peer_connection::write_cancel(peer_request const& r)
	{
		INVARIANT_CHECK;

		send_message(msg_cancel, counters::num_outgoing_cancel
			, static_cast<int>(r.piece), r.start, r.length);

		if (!m_supports_fast) incoming_reject_request(r);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_cancel, r);
#endif
	}

	void bt_peer_connection::write_request(peer_request const& r)
	{
		INVARIANT_CHECK;

		send_message(msg_request, counters::num_outgoing_request
			, static_cast<int>(r.piece), r.start, r.length);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_request, r);
#endif
	}

	void bt_peer_connection::write_bitfield()
	{
		INVARIANT_CHECK;

		// if we have not received the other peer's extension bits yet, how do we
		// know whether to send a have-all or have-none?
		TORRENT_ASSERT(m_state >= state_t::read_peer_id);

		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);
		TORRENT_ASSERT(m_sent_handshake);
		TORRENT_ASSERT(t->valid_metadata());

// #ifndef TORRENT_DISABLE_SUPERSEEDING
// 		if (t->super_seeding())
// 		{
// #ifndef TORRENT_DISABLE_LOGGING
// 			peer_log(peer_log_alert::info, "BITFIELD", "not sending bitfield, super seeding");
// #endif
// 			if (m_supports_fast) write_have_none();

// 			// if we are super seeding, pretend to not have any piece
// 			// and don't send a bitfield
// 			m_sent_bitfield = true;

// 			// bootstrap super-seeding by sending two have message
// 			piece_index_t piece = t->get_piece_to_super_seed(get_bitfield());
// 			if (piece >= piece_index_t(0)) superseed_piece(piece_index_t(-1), piece);
// 			piece = t->get_piece_to_super_seed(get_bitfield());
// 			if (piece >= piece_index_t(0)) superseed_piece(piece_index_t(-1), piece);
// 			return;
// 		}
// 		else
// #endif
			if (m_supports_fast && t->is_seed())
		{
			write_have_all();
			return;
		}
		else if (m_supports_fast && t->num_have() == 0)
		{
			write_have_none();
			return;
		}
		else if (t->num_have() == 0)
		{
			// don't send a bitfield if we don't have any pieces
#ifndef TORRENT_DISABLE_LOGGING
			peer_log(peer_log_alert::info, "BITFIELD", "not sending bitfield, have none");
#endif
			m_sent_bitfield = true;
			return;
		}

		const int num_pieces = t->torrent_file().num_pieces();
		TORRENT_ASSERT(num_pieces > 0);

		constexpr std::uint8_t char_bit_mask = CHAR_BIT - 1;
		constexpr std::uint8_t char_top_bit = 1 << (CHAR_BIT - 1);

		const int packet_size = (num_pieces + char_bit_mask) / CHAR_BIT + 5;

		TORRENT_ALLOCA(msg, char, packet_size);
		if (msg.data() == nullptr) return; // out of memory
		auto ptr = msg.begin();

		aux::write_int32(packet_size - 4, ptr);
		aux::write_uint8(msg_bitfield, ptr);

		if (t->is_seed())
		{
			std::fill_n(ptr, packet_size - 5, std::uint8_t{0xff});

			// Clear trailing bits
			msg.back() = static_cast<char>((0xff << ((CHAR_BIT - (num_pieces & char_bit_mask)) & char_bit_mask)) & 0xff);
		}
		else
		{
			std::memset(ptr, 0, aux::numeric_cast<std::size_t>(packet_size - 5));
			piece_picker const& p = t->picker();
			int mask = char_top_bit;
			for (piece_index_t i(0); i < piece_index_t(num_pieces); ++i)
			{
				if (p.have_piece(i)) *ptr |= mask;
				mask >>= 1;
				if (mask == 0)
				{
					mask = char_top_bit;
					++ptr;
				}
			}
		}

// #ifndef TORRENT_DISABLE_PREDICTIVE_PIECES
// 		// add predictive pieces to the bitfield as well, since we won't
// 		// announce them again
// 		for (piece_index_t const p : t->predictive_pieces())
// 			msg[5 + static_cast<int>(p) / CHAR_BIT] |= (char_top_bit >> (static_cast<int>(p) & char_bit_mask));
// #endif

#ifndef TORRENT_DISABLE_LOGGING
		if (should_log(peer_log_alert::outgoing_message))
		{
			std::string bitfield_string;
			auto const n_pieces = aux::numeric_cast<std::size_t>(num_pieces);
			bitfield_string.resize(n_pieces);
			for (std::size_t k = 0; k < n_pieces; ++k)
			{
				if (msg[5 + int(k) / CHAR_BIT] & (char_top_bit >> (k % CHAR_BIT))) bitfield_string[k] = '1';
				else bitfield_string[k] = '0';
			}
			peer_log(peer_log_alert::outgoing_message, "BITFIELD"
				, "%s", bitfield_string.c_str());
		}
#endif
		m_sent_bitfield = true;

		send_buffer(msg);

		stats_counters().inc_stats_counter(counters::num_outgoing_bitfield);
	}

	void bt_peer_connection::write_extensions()
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(m_supports_extensions);
		TORRENT_ASSERT(m_sent_handshake);

		entry handshake;
		entry::dictionary_type& m = handshake["m"].dict();

		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);

		// if we're using a proxy, our listen port won't be useful
		// anyway.
		if (is_outgoing())
		{
			auto const port = m_ses.listen_port(
				/*t->is_ssl_torrent() ? aux::transport::ssl : aux::transport::plaintext
				, */local_endpoint().address());
			if (port != 0) handshake["p"] = port;
		}

		// only send the port in case we bade the connection
		// on incoming connections the other end already knows
		// our listen port
		if (!m_settings.get_bool(settings_pack::anonymous_mode))
		{
			handshake["v"] = m_settings.get_str(settings_pack::handshake_client_version).empty()
				? m_settings.get_str(settings_pack::user_agent)
				: m_settings.get_str(settings_pack::handshake_client_version);
		}

		std::string remote_address;
		std::back_insert_iterator<std::string> out(remote_address);
		aux::write_address(remote().address(), out);

			handshake["yourip"] = remote_address;
		handshake["reqq"] = m_settings.get_int(settings_pack::max_allowed_in_request_queue);

		m["upload_only"] = upload_only_msg;
		m["ut_holepunch"] = holepunch_msg;
// #ifndef TORRENT_DISABLE_SHARE_MODE
// 		if (m_settings.get_bool(settings_pack::support_share_mode))
// 			m["share_mode"] = share_mode_msg;
// #endif
		m["lt_donthave"] = dont_have_msg;

		int complete_ago = -1;
		if (t->last_seen_complete() > 0) complete_ago = t->time_since_complete();
		handshake["complete_ago"] = complete_ago;

		// if we're super seeding, don't say we're upload only, since it might
		// make peers disconnect. don't tell anyone we're upload only when in
		// share mode, we want to stay connected to seeds. if we're super seeding,
		// we don't want to make peers think that we only have a single piece and
		// is upload only, since they might disconnect immediately when they have
		// downloaded a single piece, although we'll make another piece available.
		// If we don't have metadata, we also need to suppress saying we're
		// upload-only. If we do, we may be disconnected before we receive the
		// metadata.
		if (t->is_upload_only()
// #ifndef TORRENT_DISABLE_SHARE_MODE
// 			&& !t->share_mode()
// #endif
			&& t->valid_metadata()
// #ifndef TORRENT_DISABLE_SUPERSEEDING
// 			&& !t->super_seeding()
// #endif
			)
		{
			handshake["upload_only"] = 1;
		}

// #ifndef TORRENT_DISABLE_SHARE_MODE
// 		if (m_settings.get_bool(settings_pack::support_share_mode)
// 			&& t->share_mode())
// 			handshake["share_mode"] = 1;
// #endif

#ifndef TORRENT_DISABLE_EXTENSIONS
		// loop backwards, to make the first extension be the last
		// to fill in the handshake (i.e. give the first extensions priority)
		for (auto const& e : m_extensions)
		{
			e->add_handshake(handshake);
		}
#endif

#ifndef NDEBUG
		// make sure there are not conflicting extensions
		std::set<int> ext;
		for (entry::dictionary_type::const_iterator i = m.begin()
			, end(m.end()); i != end; ++i)
		{
			if (i->second.type() != entry::int_t) continue;
			int val = int(i->second.integer());
			TORRENT_ASSERT(ext.find(val) == ext.end());
			ext.insert(val);
		}
#endif

		std::vector<char> dict_msg;
		bencode(std::back_inserter(dict_msg), handshake);

		char msg[6];
		char* ptr = msg;

		// write the length of the message
		aux::write_int32(int(dict_msg.size()) + 2, ptr);
		aux::write_uint8(msg_extended, ptr);
		// signal handshake message
		aux::write_uint8(0, ptr);
		send_buffer(msg);
		send_buffer(dict_msg);

		stats_counters().inc_stats_counter(counters::num_outgoing_ext_handshake);

#ifndef TORRENT_DISABLE_LOGGING
		if (should_log(peer_log_alert::outgoing_message))
		{
			peer_log(peer_log_alert::outgoing_message, "EXTENDED_HANDSHAKE"
				, "%s", handshake.to_string(true).c_str());
		}
#endif
	}

	void bt_peer_connection::write_choke()
	{
		INVARIANT_CHECK;

		if (is_choked()) return;
		send_message(msg_choke, counters::num_outgoing_choke);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_choke);
#endif
	}

	void bt_peer_connection::write_unchoke()
	{
		INVARIANT_CHECK;

		send_message(msg_unchoke, counters::num_outgoing_unchoke);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_unchoke);
#endif
	}

	void bt_peer_connection::write_interested()
	{
		INVARIANT_CHECK;

		send_message(msg_interested, counters::num_outgoing_interested);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_interested);
#endif
	}

	void bt_peer_connection::write_not_interested()
	{
		INVARIANT_CHECK;

		send_message(msg_not_interested, counters::num_outgoing_not_interested);

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_not_interested);
#endif
	}

	void bt_peer_connection::write_have(piece_index_t const index)
	{
		INVARIANT_CHECK;
		TORRENT_ASSERT(associated_torrent().lock()->valid_metadata());
		TORRENT_ASSERT(index >= piece_index_t(0));
		TORRENT_ASSERT(index < associated_torrent().lock()->torrent_file().end_piece());

		// if we haven't sent the bitfield yet, this piece should be included in
		// there instead
		if (!m_sent_bitfield) return;

		send_message(msg_have, counters::num_outgoing_have
			, static_cast<int>(index));

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_have, index);
#endif
	}

	void bt_peer_connection::write_dont_have(piece_index_t const index)
	{
		INVARIANT_CHECK;
		TORRENT_ASSERT(associated_torrent().lock()->valid_metadata());
		TORRENT_ASSERT(index >= piece_index_t(0));
		TORRENT_ASSERT(index < associated_torrent().lock()->torrent_file().end_piece());

		if (in_handshake()) return;

		TORRENT_ASSERT(m_sent_handshake);
		TORRENT_ASSERT(m_sent_bitfield);

		if (!m_supports_extensions || m_dont_have_id == 0) return;

		char msg[] = {0,0,0,6,msg_extended,char(m_dont_have_id),0,0,0,0};
		char* ptr = msg + 6;
		aux::write_int32(static_cast<int>(index), ptr);
		send_buffer(msg);

		stats_counters().inc_stats_counter(counters::num_outgoing_extended);
	}

	void bt_peer_connection::write_piece(peer_request const& r, disk_buffer_holder buffer)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(m_sent_handshake);
		TORRENT_ASSERT(m_sent_bitfield);
		TORRENT_ASSERT(r.length >= 0);

		std::shared_ptr<torrent> t = associated_torrent().lock();
		TORRENT_ASSERT(t);

	// the hash piece looks like this:
	// uint8_t  msg
	// uint32_t piece index
	// uint32_t start
	// uint32_t list len
	// var      bencoded list
	// var      piece data
		char msg[4 + 1 + 4 + 4 + 4];
		char* ptr = msg;
		TORRENT_ASSERT(r.length <= 16 * 1024);
		aux::write_int32(r.length + 1 + 4 + 4, ptr);
		aux::write_uint8(msg_piece, ptr);
		aux::write_int32(static_cast<int>(r.piece), ptr);
		aux::write_int32(r.start, ptr);

		send_buffer({msg, 13});

		if (buffer.is_mutable())
		{
			append_send_buffer(std::move(buffer), r.length);
		}
		else
		{
			append_const_send_buffer(std::move(buffer), r.length);
		}

		m_payloads.emplace_back(send_buffer_size() - r.length, r.length);
		setup_send();

		stats_counters().inc_stats_counter(counters::num_outgoing_piece);

		if (t->alerts().should_post<block_uploaded_alert>())
		{
			t->alerts().emplace_alert<block_uploaded_alert>(t->get_handle(),
				remote(), pid(), r.start / t->block_size() , r.piece);
		}

#ifndef TORRENT_DISABLE_EXTENSIONS
		extension_notify(&peer_plugin::sent_piece, r);
#endif
	}

	// --------------------------
	// RECEIVE DATA
	// --------------------------

	void bt_peer_connection::on_receive(error_code const& error
		, std::size_t bytes_transferred)
	{
		INVARIANT_CHECK;

		if (error)
		{
			received_bytes(0, int(bytes_transferred));
			return;
		}

		// make sure are much as possible of the response ends up in the same
		// packet, or at least back-to-back packets
		cork c_(*this);

			on_receive_impl(bytes_transferred);
	}


	void bt_peer_connection::on_receive_impl(std::size_t bytes_transferred)
	{
		std::shared_ptr<torrent> t = associated_torrent().lock();

		span<char const> recv_buffer = m_recv_buffer.get();

		if (m_state == state_t::read_protocol_identifier)
		{
			TORRENT_ASSERT(!m_outgoing || m_sent_handshake);

			received_bytes(0, int(bytes_transferred));
			bytes_transferred = 0;
			TORRENT_ASSERT(m_recv_buffer.packet_size() == 20);

			if (!m_recv_buffer.packet_finished()) return;
			recv_buffer = m_recv_buffer.get();

			int const packet_size = recv_buffer[0];
			static const char protocol_string[] = "\x13" "BitTorrent protocol";

			if (packet_size != 19 ||
				recv_buffer.first(20) != span<char const>{protocol_string, 20})
			{
				disconnect(errors::invalid_info_hash, operation_t::bittorrent, failure);
				return;
			}
			else
			{

#ifndef TORRENT_DISABLE_LOGGING
				peer_log(peer_log_alert::incoming_message, "HANDSHAKE", "BitTorrent protocol");
#endif
			}

			TORRENT_ASSERT(!m_outgoing || m_sent_handshake);
			m_state = state_t::read_info_hash;
			m_recv_buffer.reset(28);
		}

		// fall through
		if (m_state == state_t::read_info_hash)
		{
			received_bytes(0, int(bytes_transferred));
			bytes_transferred = 0;
			TORRENT_ASSERT(m_recv_buffer.packet_size() == 28);

			if (!m_recv_buffer.packet_finished()) return;
			recv_buffer = m_recv_buffer.get();

#ifndef TORRENT_DISABLE_LOGGING
			std::string extensions;
			extensions.reserve(8 * 8);
			for (int i = 0; i < 8; ++i)
				for (int j = 0; j < 8; ++j)
					extensions += (recv_buffer[i] & (0x80 >> j)) ? '1' : '0';

			if (should_log(peer_log_alert::incoming_message))
			{
				peer_log(peer_log_alert::incoming_message, "EXTENSIONS", "%s ext: %s%s%s"
					, extensions.c_str()
				
					, (recv_buffer[7] & 0x04) ? "FAST " : ""
					, (recv_buffer[7] & 0x10) ? "v2 " : ""
					, (recv_buffer[5] & 0x10) ? "extension " : "");
			}
#endif

			std::memcpy(m_reserved_bits.data(), recv_buffer.data(), 8);
			if (recv_buffer[5] & 0x10)
				m_supports_extensions = true;

	

			if (recv_buffer[7] & 0x04)
				m_supports_fast = true;

			t = associated_torrent().lock();

			// ok, now we have got enough of the handshake. Is this connection
			// attached to a torrent?
			if (!t)
			{
				// now, we have to see if there's a torrent with the
				// info_hash we got from the peer
				sha1_hash info_hash;
				std::copy(recv_buffer.begin() + 8, recv_buffer.begin() + 28
					, info_hash.data());

				attach_to_torrent(info_hash_t(info_hash));
				if (is_disconnecting()) return;

				t = associated_torrent().lock();
				TORRENT_ASSERT(t);

				// this must go after the connection is attached to a torrent because that is what
				// adds the peer info for incoming connections
				// if (recv_buffer[7] & 0x10)
				// {
				// 	if (t->valid_metadata() && !t->info_hash().has_v2())
				// 	{
				// 		// the peer claims to support the v2 protocol with a non-v2 torrent
				// 		disconnect(errors::invalid_info_hash, operation_t::bittorrent);
				// 		return;
				// 	}
				// 	peer_info_struct()->protocol_v2 = true;
				// }
			}
			else
			{
				// verify info hash
				// also check for all zero info hash in the torrent to make sure
				// the client isn't attempting to use a protocol version the torrent
				// doesn't support
				// if (std::equal(recv_buffer.begin() + 8, recv_buffer.begin() + 28
				// 	, t->info_hash().get(protocol_version::V2).data())
				// 	&& t->info_hash().has_v2())
				// {
				// 	peer_info_struct()->protocol_v2 = true;
				// }
				// else
				 if
				(!std::equal(recv_buffer.begin() + 8, recv_buffer.begin() + 28
						, associated_info_hash().data())
					|| associated_info_hash().is_all_zeros())
				{
#ifndef TORRENT_DISABLE_LOGGING
					peer_log(peer_log_alert::info, "ERROR", "received invalid info_hash");
#endif
					disconnect(errors::invalid_info_hash, operation_t::bittorrent, failure);
					return;
				}

#ifndef TORRENT_DISABLE_LOGGING
				peer_log(peer_log_alert::incoming, "HANDSHAKE", "info_hash received");
#endif
			}

			// if this is a local connection, we have already
			// sent the handshake
			if (!is_outgoing()) write_handshake();
			TORRENT_ASSERT(m_sent_handshake);

			if (is_disconnecting()) return;

			m_state = state_t::read_peer_id;
			m_recv_buffer.reset(20);
		}

		// fall through
		if (m_state == state_t::read_peer_id)
		{
			TORRENT_ASSERT(m_sent_handshake);
			received_bytes(0, int(bytes_transferred));

			t = associated_torrent().lock();
			if (!t)
			{
				TORRENT_ASSERT(!m_recv_buffer.packet_finished()); // TODO
				return;
			}
			TORRENT_ASSERT(m_recv_buffer.packet_size() == 20);

			if (!m_recv_buffer.packet_finished()) return;
			recv_buffer = m_recv_buffer.get();

#ifndef TORRENT_DISABLE_LOGGING
			if (should_log(peer_log_alert::incoming))
			{
				char hex_pid[41];
				aux::to_hex({recv_buffer.data(), 20}, hex_pid);
				hex_pid[40] = 0;
				char ascii_pid[21];
				ascii_pid[20] = 0;
				for (int i = 0; i != 20; ++i)
					ascii_pid[i] = (is_print(recv_buffer[i])) ? recv_buffer[i] : '.';

				peer_log(peer_log_alert::incoming, "HANDSHAKE", "received peer_id: %s client: %s ascii: \"%s\""
					, hex_pid, identify_client(peer_id(recv_buffer.data())).c_str(), ascii_pid);
			}
#endif
			peer_id pid;
			std::copy(recv_buffer.begin(), recv_buffer.begin() + 20, pid.data());

			// now, let's see if this connection should be closed
			peer_connection* p = t->find_peer(pid);
			if (p)
			{
				TORRENT_ASSERT(p->pid() == pid);
				// we found another connection with the same peer-id
				// which connection should be closed in order to be
				// sure that the other end closes the same connection?
				// the peer with greatest peer-id is the one allowed to
				// initiate connections. So, if our peer-id is greater than
				// the others, we should close the incoming connection,
				// if not, we should close the outgoing one.
				if ((pid < m_our_peer_id) == is_outgoing())
				{
					p->disconnect(errors::duplicate_peer_id, operation_t::bittorrent);
				}
				else
				{
					disconnect(errors::duplicate_peer_id, operation_t::bittorrent);
					return;
				}
			}

			set_pid(pid);
			m_client_version = identify_client(pid);
			if (pid[0] == '-' && pid[1] == 'B' && pid[2] == 'C' && pid[7] == '-')
			{
				// if this is a bitcomet client, lower the request queue size limit
				if (max_out_request_queue() > 50) max_out_request_queue(50);
			}

			if (t->is_self_connection(pid))
			{
				disconnect(errors::self_connection, operation_t::bittorrent);
				return;
			}

#ifndef TORRENT_DISABLE_EXTENSIONS
			for (auto i = m_extensions.begin()
				, end(m_extensions.end()); i != end;)
			{
				if (!(*i)->on_handshake(m_reserved_bits))
				{
					i = m_extensions.erase(i);
				}
				else
				{
					++i;
				}
			}
			if (is_disconnecting()) return;
#endif

			if (m_supports_extensions) write_extensions();

#ifndef TORRENT_DISABLE_LOGGING
			peer_log(peer_log_alert::incoming_message, "HANDSHAKE", "connection ready");
#endif
			// consider this a successful connection, reset the failcount
			if (peer_info_struct())
				t->clear_failcount(peer_info_struct());



			// complete the handshake
			// we don't know how many pieces there are until we
			// have the metadata
			if (t->ready_for_connections())
			{
				write_bitfield();
			
				// maybe_send_hash_request();

				// if we don't have any pieces, don't do any preemptive
				// unchoking at all.
				if (t->num_have() > 0)
				{
					// if the peer is ignoring unchoke slots, or if we have enough
					// unused slots, unchoke this peer right away, to save a round-trip
					// in case it's interested.
					maybe_unchoke_this_peer();
				}
			}

			m_state = state_t::read_packet_size;
			m_recv_buffer.reset(5);

			TORRENT_ASSERT(!m_recv_buffer.packet_finished());
			return;
		}

		// cannot fall through into
		if (m_state == state_t::read_packet_size)
		{
			// Make sure this is not fallen though into
			TORRENT_ASSERT(recv_buffer.data() == m_recv_buffer.get().data());
			TORRENT_ASSERT(recv_buffer.size() == m_recv_buffer.get().size());
			TORRENT_ASSERT(m_recv_buffer.packet_size() == 5);

			if (!t) return;

			// the 5th byte (if one) should not count as protocol
			// byte here, instead it's counted in the message
			// handler itself, for the specific message
			TORRENT_ASSERT(bytes_transferred <= 5);
			int used_bytes = int(recv_buffer.size()) > 4 ? int(bytes_transferred) - 1: int(bytes_transferred);
			received_bytes(0, used_bytes);
			bytes_transferred -= aux::numeric_cast<std::size_t>(used_bytes);
			if (int(recv_buffer.size()) < 4) return;

			TORRENT_ASSERT(bytes_transferred <= 1);

			const char* ptr = recv_buffer.data();
			int const packet_size = aux::read_int32(ptr);

			// don't accept packets larger than 1 MB
			if (packet_size > 1024 * 1024 || packet_size < 0)
			{
				// packet too large
				received_bytes(0, int(bytes_transferred));
				disconnect(errors::packet_too_large, operation_t::bittorrent, peer_error);
				return;
			}

			if (packet_size == 0)
			{
				TORRENT_ASSERT(bytes_transferred <= 1);
				received_bytes(0, int(bytes_transferred));
				incoming_keepalive();
				if (is_disconnecting()) return;
				// keepalive message
				m_state = state_t::read_packet_size;
				m_recv_buffer.cut(4, 5);
				return;
			}
			if (int(recv_buffer.size()) < 5) return;

			m_state = state_t::read_packet;
			m_recv_buffer.cut(4, packet_size);
			recv_buffer = m_recv_buffer.get();
			TORRENT_ASSERT(int(recv_buffer.size()) == 1);
			TORRENT_ASSERT(bytes_transferred == 1);
		}

		if (m_state == state_t::read_packet)
		{
			TORRENT_ASSERT(recv_buffer.data() == m_recv_buffer.get().data());
			TORRENT_ASSERT(recv_buffer.size() == m_recv_buffer.get().size());
			if (!t)
			{
				received_bytes(0, int(bytes_transferred));
				disconnect(errors::torrent_removed, operation_t::bittorrent, failure);
				return;
			}
#if TORRENT_USE_ASSERTS
			std::int64_t const cur_payload_dl = statistics().last_payload_downloaded();
			std::int64_t const cur_protocol_dl = statistics().last_protocol_downloaded();
#endif
			if (dispatch_message(int(bytes_transferred)))
			{
				m_state = state_t::read_packet_size;
				m_recv_buffer.reset(5);
			}

#if TORRENT_USE_ASSERTS
			TORRENT_ASSERT(statistics().last_payload_downloaded() - cur_payload_dl >= 0);
			TORRENT_ASSERT(statistics().last_protocol_downloaded() - cur_protocol_dl >= 0);
			std::int64_t const stats_diff = statistics().last_payload_downloaded() - cur_payload_dl +
				statistics().last_protocol_downloaded() - cur_protocol_dl;
			TORRENT_ASSERT(stats_diff == std::int64_t(bytes_transferred));
			TORRENT_ASSERT(!m_recv_buffer.packet_finished());
#endif
			return;
		}

		TORRENT_ASSERT(!m_recv_buffer.packet_finished());
	}


	// --------------------------
	// SEND DATA
	// --------------------------

	void bt_peer_connection::on_sent(error_code const& error
		, std::size_t const bytes_transferred)
	{
		INVARIANT_CHECK;

		if (error)
		{
			sent_bytes(0, int(bytes_transferred));
			return;
		}

		// manage the payload markers
		int amount_payload = 0;
		if (!m_payloads.empty())
		{
			// this points to the first entry to not erase. i.e.
			// [begin, first_to_keep) will be erased because
			// the payload ranges they represent have been sent
			auto first_to_keep = m_payloads.begin();

			for (auto i = m_payloads.begin(); i != m_payloads.end(); ++i)
			{
				i->start -= int(bytes_transferred);
				if (i->start < 0)
				{
					if (i->start + i->length <= 0)
					{
						amount_payload += i->length;
						TORRENT_ASSERT(first_to_keep == i);
						++first_to_keep;
					}
					else
					{
						amount_payload += -i->start;
						i->length -= -i->start;
						i->start = 0;
					}
				}
			}

			// remove all payload ranges that have been sent
			m_payloads.erase(m_payloads.begin(), first_to_keep);
		}

		TORRENT_ASSERT(amount_payload <= int(bytes_transferred));
		sent_bytes(amount_payload, int(bytes_transferred) - amount_payload);

		if (amount_payload > 0)
		{
			std::shared_ptr<torrent> t = associated_torrent().lock();
			TORRENT_ASSERT(t);
			if (t) t->update_last_upload();
		}
	}

#if TORRENT_USE_INVARIANT_CHECKS
	void bt_peer_connection::check_invariant() const
	{
		std::shared_ptr<torrent> t = associated_torrent().lock();

		if (!in_handshake())
		{
			TORRENT_ASSERT(m_sent_handshake);
		}

		if (!m_payloads.empty())
		{
			for (std::vector<range>::const_iterator i = m_payloads.begin();
				i != m_payloads.end() - 1; ++i)
			{
				TORRENT_ASSERT(i->start + i->length <= (i+1)->start);
			}
		}
	}
#endif

}
