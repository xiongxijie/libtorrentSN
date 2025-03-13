/*

Copyright (c) 2007, Un Shyam
Copyright (c) 2006-2020, 2022, Arvid Norberg
Copyright (c) 2016-2017, Alden Torres
Copyright (c) 2016, Pavel Pimenov
Copyright (c) 2018, Greg Hazel
Copyright (c) 2018-2019, Steven Siloti
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

#ifndef TORRENT_BT_PEER_CONNECTION_HPP_INCLUDED
#define TORRENT_BT_PEER_CONNECTION_HPP_INCLUDED

#include <ctime>
#include <algorithm>
#include <vector>
#include <string>
#include <array>
#include <cstdint>

#include "libtorrent/debug.hpp"
#include "libtorrent/aux_/buffer.hpp"
#include "libtorrent/peer_connection.hpp"
#include "libtorrent/socket.hpp"
#include "libtorrent/peer_id.hpp"
#include "libtorrent/stat.hpp"
#include "libtorrent/alert.hpp"
#include "libtorrent/torrent.hpp"
#include "libtorrent/peer_request.hpp"
#include "libtorrent/piece_block_progress.hpp"
#include "libtorrent/config.hpp"
// #include "libtorrent/pe_crypto.hpp"
#include "libtorrent/io.hpp"
// #include "libtorrent/hash_picker.hpp"

namespace libtorrent {

#ifndef TORRENT_DISABLE_EXTENSIONS
	struct TORRENT_EXTRA_EXPORT ut_pex_peer_store
	{
		// stores all peers this peer is connected to. These lists
		// are updated with each pex message and are limited in size
		// to protect against malicious clients. These lists are also
		// used for looking up which peer a peer that supports holepunch
		// came from.
		// these are vectors to save memory and keep the items close
		// together for performance. Inserting and removing is relatively
		// cheap since the lists' size is limited
		using peers4_t = std::vector<std::pair<address_v4::bytes_type, std::uint16_t>>;
		peers4_t m_peers;
		using peers6_t = std::vector<std::pair<address_v6::bytes_type, std::uint16_t>>;
		peers6_t m_peers6;

		bool was_introduced_by(tcp::endpoint const& ep);

		virtual ~ut_pex_peer_store() {}
	};
#endif

	struct TORRENT_EXTRA_EXPORT bt_peer_connection
		: peer_connection
	{
	friend struct invariant_access;

		// this is the constructor where the we are the active part.
		// The peer_connection should handshake and verify that the
		// other end has the correct id
		explicit bt_peer_connection(peer_connection_args& pack);

		void start() override;

		enum
		{
			// pex_msg = 1,
			// metadata_msg = 2,
			upload_only_msg = 3,
			holepunch_msg = 4,
			// recommend_msg = 5,
			// comment_msg = 6,
			dont_have_msg = 7,
			share_mode_msg = 8
		};

		~bt_peer_connection() override;

		peer_id our_pid() const override { return m_our_peer_id; }



		connection_type type() const override
		{ return connection_type::bittorrent; }

		enum message_type
		{
			// standard messages
			msg_choke = 0,
			msg_unchoke,
			msg_interested,
			msg_not_interested,
			msg_have,
			msg_bitfield,
			msg_request,
			msg_piece,
			msg_cancel,
		
			// FAST extension
			msg_suggest_piece = 0xd,
			msg_have_all,
			msg_have_none,
			msg_reject_request,
			msg_allowed_fast,

			// extension protocol message
			msg_extended = 20,

			msg_hash_request = 21,
			msg_hashes,
			msg_hash_reject,

			num_supported_messages
		};

		enum class hp_message : std::uint8_t
		{
			// msg_types
			rendezvous = 0,
			connect = 1,
			failed = 2
		};

		enum class hp_error
		{
			// error codes
			no_error = 0,
			no_such_peer = 1,
			not_connected = 2,
			no_support = 3,
			no_self = 4
		};

		// called from the main loop when this connection has any
		// work to do.

		void on_sent(error_code const& error
			, std::size_t bytes_transferred) override;
		void on_receive(error_code const& error
			, std::size_t bytes_transferred) override;
		void on_receive_impl(std::size_t bytes_transferred);


		void get_specific_peer_info(peer_info& p) const override;
		bool in_handshake() const override;
		bool packet_finished() const { return m_recv_buffer.packet_finished(); }

		bool supports_holepunch() const { return m_holepunch_id != 0; }
#ifndef TORRENT_DISABLE_EXTENSIONS
		void set_ut_pex(std::weak_ptr<ut_pex_peer_store> ut_pex)
		{ m_ut_pex = std::move(ut_pex); }
		bool was_introduced_by(tcp::endpoint const& ep) const
		{ auto p = m_ut_pex.lock(); return p && p->was_introduced_by(ep); }
#endif

		bool support_extensions() const { return m_supports_extensions; }

		// the message handlers are called
		// each time a recv() returns some new
		// data, the last time it will be called
		// is when the entire packet has been
		// received, then it will no longer
		// be called. i.e. most handlers need
		// to check how much of the packet they
		// have received before any processing
		void on_choke(int received);
		void on_unchoke(int received);
		void on_interested(int received);
		void on_not_interested(int received);
		void on_have(int received);
		void on_bitfield(int received);
		void on_request(int received);
		void on_piece(int received);
		void on_cancel(int received);
		// void on_hash_request(int received);
		// void on_hashes(int received);
		// void on_hash_reject(int received);


		// FAST extension
		void on_suggest_piece(int received);
		void on_have_all(int received);
		void on_have_none(int received);
		void on_reject_request(int received);
		void on_allowed_fast(int received);
		void on_holepunch();

		void on_extended(int received);

		void on_extended_handshake();

		template<typename F, typename... Args>
		void extension_notify(F message, Args... args);

		// the following functions appends messages
		// to the send buffer
		void write_choke() override;
		void write_unchoke() override;
		void write_interested() override;
		void write_not_interested() override;
		void write_request(peer_request const& r) override;
		void write_cancel(peer_request const& r) override;
		void write_bitfield() override;
		void write_have(piece_index_t index) override;
		void write_dont_have(piece_index_t index) override;
		void write_piece(peer_request const& r, disk_buffer_holder buffer) override;
		void write_keepalive() override;
		void write_handshake();
		void write_upload_only(bool enabled) override;
		void write_extensions();
		void write_share_mode();
		void write_holepunch_msg(hp_message type, tcp::endpoint const& ep
			, hp_error error = hp_error::no_error);
		// void write_hash_request(hash_request const& req);
		// void write_hashes(hash_request const& req, span<sha256_hash> hashes);
		// void write_hash_reject(hash_request const& req, sha256_hash const& file_root);

		// void maybe_send_hash_request();


		// FAST extension
		void write_have_all();
		void write_have_none();
		void write_reject_request(peer_request const&) override;
		void write_allow_fast(piece_index_t piece) override;
		void write_suggest(piece_index_t piece) override;

		void on_connected() override;
		void on_metadata() override;

#if TORRENT_USE_INVARIANT_CHECKS
		void check_invariant() const;
		time_point m_last_choke;
#endif

	private:

		template <typename... Args>
		void send_message(message_type const type
			, counters::stats_counter_t const counter
			, Args... args)
		{
			TORRENT_ASSERT(m_sent_handshake);
			TORRENT_ASSERT(m_sent_bitfield);

			char msg[5 + sizeof...(Args) * 4]
				= { 0,0,0,1 + sizeof...(Args) * 4, static_cast<char>(type) };
			char* ptr = msg + 5;
			TORRENT_UNUSED(ptr);

			int tmp[] = {0, (aux::write_int32(args, ptr), 0)...};
			TORRENT_UNUSED(tmp);

			send_buffer(msg);

			stats_counters().inc_stats_counter(counter);
		}

		bool dispatch_message(int received);
		// returns the block currently being
		// downloaded. And the progress of that
		// block. If the peer isn't downloading
		// a piece for the moment, the boost::optional
		// will be invalid.
		piece_block_progress downloading_piece_progress() const override;


	public:

		// these functions encrypt the send buffer if m_rc4_encrypted
		// is true, otherwise it passes the call to the
		// peer_connection functions of the same names
		template <typename Holder>
		void append_const_send_buffer(Holder holder, int size)
		{
			append_send_buffer(std::move(holder), size);
		}

	private:


		enum class state_t : std::uint8_t
		{

			read_protocol_identifier,
			read_info_hash,
			read_peer_id,

			// handshake complete
			read_packet_size,
			read_packet
		};

		// state of on_receive. one of the enums in state_t
		state_t m_state = state_t::read_protocol_identifier;

		// this is set to true if the handshake from
		// the peer indicated that it supports the
		// extension protocol
		bool m_supports_extensions:1;

		bool m_supports_fast:1;

		// this is set to true when we send the bitfield message.
		// for magnet links we can't do that right away,
		// since we don't know how many pieces there are in
		// the torrent.
		bool m_sent_bitfield:1;

		// true if we're done sending the bittorrent handshake,
		// and can send bittorrent messages
		bool m_sent_handshake:1;

		// set to true once we send the allowed-fast messages. This is
		// only done once per connection
		bool m_sent_allowed_fast:1;


		std::string m_client_version;

		// the peer ID we advertise for ourself
		peer_id const m_our_peer_id;

		// this is a queue of ranges that describes
		// where in the send buffer actual payload
		// data is located. This is currently
		// only used to be able to gather statistics
		// separately on payload and protocol data.
		struct range
		{
			range(int s, int l)
				: start(s)
				, length(l)
			{
				TORRENT_ASSERT(s >= 0);
				TORRENT_ASSERT(l > 0);
			}
			int start;
			int length;
		};

		std::vector<range> m_payloads;

		// std::vector<hash_request> m_hash_requests;



		// the message ID for upload only message
		// 0 if not supported
		std::uint8_t m_upload_only_id = 0;

		// the message ID for holepunch messages
		std::uint8_t m_holepunch_id = 0;

		// the message ID for don't-have message
		std::uint8_t m_dont_have_id = 0;

		// the message ID for share mode message
		// 0 if not supported
		std::uint8_t m_share_mode_id = 0;

#ifndef TORRENT_DISABLE_EXTENSIONS
		std::weak_ptr<ut_pex_peer_store> m_ut_pex;
#endif

		std::array<char, 8> m_reserved_bits;
	};
}

#endif // TORRENT_BT_PEER_CONNECTION_HPP_INCLUDED
