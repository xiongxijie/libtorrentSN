/*

Copyright (c) 2009, 2012-2015, 2017-2020, Arvid Norberg
Copyright (c) 2016, 2020, Alden Torres
Copyright (c) 2016, 2020, Steven Siloti
Copyright (c) 2020, Paul-Louis Ageneau
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
#include "libtorrent/aux_/socket_type.hpp"
#include "libtorrent/aux_/array.hpp"
#include "libtorrent/deadline_timer.hpp"
#include "libtorrent/debug.hpp"

namespace libtorrent {

	char const* socket_type_name(socket_type_t const s)
	{
		static aux::array<char const*, 2/*9*/, socket_type_t> const names{{{
			"TCP",
			// "Socks5",
			// "HTTP",
			"uTP" /*,*/
			// "",
			// "","","",""

		}}};
		return names[s];
	}

namespace aux {

	struct is_ssl_visitor {
		template <typename T>
		bool operator()(T const&) const { return false; }
	};

	bool is_ssl(socket_type const& s)
	{
		return boost::apply_visitor(is_ssl_visitor{}, s);
	}

	bool is_utp(socket_type const& s)
	{
		return boost::get<utp_stream>(&s);
	}



	struct idx_visitor {
		socket_type_t operator()(tcp::socket const&) const { return socket_type_t::tcp; }
		// socket_type_t operator()(socks5_stream const&) const { return socket_type_t::socks5; }
		// socket_type_t operator()(http_stream const&) const { return socket_type_t::http; }
		socket_type_t operator()(utp_stream const&) const { return socket_type_t::utp; }

// #if TORRENT_USE_SSL
// 		socket_type_t operator()(ssl_stream<tcp::socket> const&) const { return socket_type_t::tcp_ssl; }
// 		// socket_type_t operator()(ssl_stream<socks5_stream> const&) const { return socket_type_t::socks5_ssl; }
// 		socket_type_t operator()(ssl_stream<http_stream> const&) const { return socket_type_t::http_ssl; }
// 		socket_type_t operator()(ssl_stream<utp_stream> const&) const { return socket_type_t::utp_ssl; }
// #endif
	};

	socket_type_t socket_type_idx(socket_type const& s)
	{
		return boost::apply_visitor(idx_visitor{}, s);
	}

	char const* socket_type_name(socket_type const& s)
	{
		return socket_type_name(socket_type_idx(s));
	}

	struct set_close_reason_visitor {
		close_reason_t code_;
		void operator()(utp_stream& s) const
		{ s.set_close_reason(code_); }
		template <typename T>
		void operator()(T const&) const {}
	};

	void set_close_reason(socket_type& s, close_reason_t code)
	{
		boost::apply_visitor(set_close_reason_visitor{code}, s);
	}

	struct get_close_reason_visitor {

		close_reason_t operator()(utp_stream const& s) const
		{ return s.get_close_reason(); }
		template <typename T>
		close_reason_t operator()(T const&) const { return close_reason_t::none; }
	};

	close_reason_t get_close_reason(socket_type const& s)
	{
		return boost::apply_visitor(get_close_reason_visitor{}, s);
	}



	// void setup_ssl_hostname(socket_type& s, std::string const& hostname, error_code& ec)
	// {
	// 	TORRENT_UNUSED(ec);
	// 	TORRENT_UNUSED(hostname);
	// 	TORRENT_UNUSED(s);
	// }


	// the second argument is a shared pointer to an object that
	// will keep the socket (s) alive for the duration of the async operation
	void async_shutdown(socket_type& s, std::shared_ptr<void> holder)
	{
		TORRENT_UNUSED(holder);
		error_code e;
		s.close(e);
	}
}
}
