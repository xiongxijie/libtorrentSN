/*

Copyright (c) 2007-2021, Arvid Norberg
Copyright (c) 2015, Thomas Yuan
Copyright (c) 2016-2018, 2020, Alden Torres
Copyright (c) 2016, Andrei Kurushin
Copyright (c) 2016, Steven Siloti
Copyright (c) 2022, Andrei Borzenkov
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
#include "libtorrent/udp_socket.hpp"
#include "libtorrent/socket_io.hpp"
#include "libtorrent/settings_pack.hpp"
#include "libtorrent/error.hpp"
#include "libtorrent/time.hpp"
#include "libtorrent/debug.hpp"
#include "libtorrent/deadline_timer.hpp"
#include "libtorrent/aux_/numeric_cast.hpp"
#include "libtorrent/aux_/ip_helpers.hpp" // for is_v4
#include "libtorrent/aux_/alert_manager.hpp"
// #include "libtorrent/socks5_stream.hpp" // for socks_error
#include "libtorrent/aux_/keepalive.hpp"
#include "libtorrent/aux_/resolver_interface.hpp"

#include <cstdlib>
#include <functional>

#include "libtorrent/aux_/disable_warnings_push.hpp"
#include <boost/asio/ip/v6_only.hpp>
#include "libtorrent/aux_/disable_warnings_pop.hpp"

#ifdef _WIN32
// for SIO_KEEPALIVE_VALS
#include <mstcpip.h>
#endif

namespace libtorrent {

using namespace std::placeholders;

// used to build SOCKS messages in
std::size_t const tmp_buffer_size = 270;

// used for SOCKS5 UDP wrapper header
std::size_t const max_header_size = 255;



#ifdef TORRENT_HAS_DONT_FRAGMENT
struct set_dont_frag
{
	set_dont_frag(udp::socket& sock, bool const df)
		: m_socket(sock)
		, m_df(df)
	{
		if (!m_df) return;
		error_code ignore_errors;
		m_socket.set_option(libtorrent::dont_fragment(true), ignore_errors);
		TORRENT_ASSERT_VAL(!ignore_errors, ignore_errors.message());
	}

	~set_dont_frag()
	{
		if (!m_df) return;
		error_code ignore_errors;
		m_socket.set_option(libtorrent::dont_fragment(false), ignore_errors);
		TORRENT_ASSERT_VAL(!ignore_errors, ignore_errors.message());
	}

private:
	udp::socket& m_socket;
	bool const m_df;
};
#else
struct set_dont_frag
{ set_dont_frag(udp::socket&, int) {} };
#endif

udp_socket::udp_socket(io_context& ios, aux::listen_socket_handle ls)
	: m_socket(ios)
	, m_ioc(ios)
	, m_buf(new receive_buffer())
	, m_listen_socket(std::move(ls))
	, m_bind_port(0)
	, m_abort(true)
{}

int udp_socket::read(span<packet> pkts, error_code& ec)
{
	auto const num = int(pkts.size());
	int ret = 0;
	packet p;

	while (ret < num)
	{
		int const len = int(m_socket.receive_from(boost::asio::buffer(*m_buf)
			, p.from, 0, ec));

		if (ec == error::would_block
			|| ec == error::try_again
			|| ec == error::operation_aborted
			|| ec == error::bad_descriptor)
		{
			return ret;
		}

		if (ec == error::interrupted)
		{
			continue;
		}

		if (ec)
		{
			// SOCKS5 cannot wrap ICMP errors. And even if it could, they certainly
			// would not arrive as unwrapped (regular) ICMP errors. If we're using
			// a proxy we must ignore these
		//	if (m_proxy_settings.type != settings_pack::none) continue;

			p.error = ec;
			p.data = span<char>();
		}
		else
		{
			p.data = {m_buf->data(), len};

			// support packets coming from the SOCKS5 proxy
			// if (active_socks5())
			// {
			// 	// if the source IP doesn't match the proxy's, ignore the packet
			// 	if (p.from != m_socks5_connection->target()) continue;
			// 	// if we failed to unwrap, silently ignore the packet
			// 	if (!unwrap(p)) continue;
			// }
			// else
			{
				// if we don't proxy trackers or peers, we may be receiving unwrapped
				// packets and we must let them through.
				// bool const proxy_only
				// 	= m_proxy_settings.proxy_peer_connections
				// 	&& m_proxy_settings.proxy_tracker_connections
					;

				// if we proxy everything, block all packets that aren't coming from
				// the proxy
				// if (m_proxy_settings.type != settings_pack::none && proxy_only) continue;
			}
		}

		pkts[ret] = p;
		++ret;

		// we only have a single buffer for now, so we can only return a
		// single packet. In the future though, we could attempt to drain
		// the socket here, or maybe even use recvmmsg()
		break;
	}

	return ret;
}

// bool udp_socket::active_socks5() const
// {
// 	return (m_socks5_connection && m_socks5_connection->active());
// }

// void udp_socket::send_hostname(char const* hostname, int const port
// 	, span<char const> p, error_code& ec, udp_send_flags_t const flags)
// {
// 	TORRENT_ASSERT(is_single_thread());

// 	// if the sockets are closed, the udp_socket is closing too
// 	if (!is_open())
// 	{
// 		ec = error_code(boost::system::errc::bad_file_descriptor, generic_category());
// 		return;
// 	}

// 	// bool const use_proxy
// 	// 	= ((flags & peer_connection) && m_proxy_settings.proxy_peer_connections)
// 	// 	|| ((flags & tracker_connection) && m_proxy_settings.proxy_tracker_connections)
// 	// 	|| !(flags & (tracker_connection | peer_connection))
// 	// 	;

// 	// if (use_proxy && m_proxy_settings.type != settings_pack::none)
// 	// {
// 	// 	if (active_socks5())
// 	// 	{
// 	// 		// send udp packets through SOCKS5 server
// 	// 		wrap(hostname, port, p, ec, flags);
// 	// 	}
// 	// 	else
// 	// 	{
// 	// 		ec = error_code(boost::system::errc::permission_denied, generic_category());
// 	// 	}
// 	// 	return;
// 	// }

// 	// the overload that takes a hostname is really only supported when we're
// 	// using a proxy
// 	address const target = make_address(hostname, ec);
// 	if (!ec) send(udp::endpoint(target, std::uint16_t(port)), p, ec, flags);
// }

void udp_socket::send(udp::endpoint const& ep, span<char const> p
	, error_code& ec, udp_send_flags_t const flags)
{
	TORRENT_ASSERT(is_single_thread());

	// if the sockets are closed, the udp_socket is closing too
	if (!is_open())
	{
		ec = error_code(boost::system::errc::bad_file_descriptor, generic_category());
		return;
	}

	// bool const use_proxy
	// 	= ((flags & peer_connection) && m_proxy_settings.proxy_peer_connections)
	// 	|| ((flags & tracker_connection) && m_proxy_settings.proxy_tracker_connections)
	// 	|| !(flags & (tracker_connection | peer_connection))
	// 	;

	// if (use_proxy && m_proxy_settings.type != settings_pack::none)
	// {
	// 	if (active_socks5())
	// 	{
	// 		// send udp packets through SOCKS5 server
	// 		wrap(ep, p, ec, flags);
	// 	}
	// 	else
	// 	{
	// 		ec = error_code(boost::system::errc::permission_denied, generic_category());
	// 	}
	// 	return;
	// }

	// set the DF flag for the socket and clear it again in the destructor
	set_dont_frag df(m_socket, (flags & dont_fragment)
		&& aux::is_v4(ep));

	m_socket.send_to(boost::asio::buffer(p.data(), static_cast<std::size_t>(p.size())), ep, 0, ec);
}

// void udp_socket::wrap(udp::endpoint const& ep, span<char const> p
// 	, error_code& ec, udp_send_flags_t const flags)
// {
// 	TORRENT_UNUSED(flags);
// 	using namespace libtorrent::aux;

// 	std::array<char, max_header_size> header;
// 	char* h = header.data();

// 	write_uint16(0, h); // reserved
// 	write_uint8(0, h); // fragment
// 	write_uint8(aux::is_v4(ep) ? 1 : 4, h); // atyp
// 	write_endpoint(ep, h);

// 	std::array<boost::asio::const_buffer, 2> iovec;
// 	iovec[0] = boost::asio::const_buffer(header.data(), aux::numeric_cast<std::size_t>(h - header.data()));
// 	iovec[1] = boost::asio::const_buffer(p.data(), static_cast<std::size_t>(p.size()));

// 	// set the DF flag for the socket and clear it again in the destructor
// 	set_dont_frag df(m_socket, (flags & dont_fragment) && aux::is_v4(ep));

// 	m_socket.send_to(iovec, m_socks5_connection->target(), 0, ec);
// }

// void udp_socket::wrap(char const* hostname, int const port, span<char const> p
// 	, error_code& ec, udp_send_flags_t const flags)
// {
// 	using namespace libtorrent::aux;

// 	std::array<char, max_header_size> header;
// 	char* h = header.data();

// 	write_uint16(0, h); // reserved
// 	write_uint8(0, h); // fragment
// 	write_uint8(3, h); // atyp
// 	std::size_t const hostlen = std::min(std::strlen(hostname), max_header_size - 7);
// 	write_uint8(hostlen, h); // hostname len
// 	std::memcpy(h, hostname, hostlen);
// 	h += hostlen;
// 	write_uint16(port, h);

// 	std::array<boost::asio::const_buffer, 2> iovec;
// 	iovec[0] = boost::asio::const_buffer(header.data(), aux::numeric_cast<std::size_t>(h - header.data()));
// 	iovec[1] = boost::asio::const_buffer(p.data(), static_cast<std::size_t>(p.size()));

// 	// set the DF flag for the socket and clear it again in the destructor
// 	set_dont_frag df(m_socket, (flags & dont_fragment)
// 		&& aux::is_v4(m_socket.local_endpoint(ec)));

// 	m_socket.send_to(iovec, m_socks5_connection->target(), 0, ec);
// }

// unwrap the UDP packet from the SOCKS5 header
// buf is an in-out parameter. It will be updated
// return false if the packet should be ignored. It's not a valid Socks5 UDP
// forwarded packet
bool udp_socket::unwrap(udp_socket::packet& pack)
{
	using namespace libtorrent::aux;

	// the minimum socks5 header size
	auto const size = aux::numeric_cast<int>(pack.data.size());
	if (size <= 10) return false;

	char* p = pack.data.data();
	p += 2; // reserved
	int const frag = read_uint8(p);
	// fragmentation is not supported
	if (frag != 0) return false;

	int const atyp = read_uint8(p);
	if (atyp == 1)
	{
		// IPv4
		pack.from = read_v4_endpoint<udp::endpoint>(p);
	}
	else if (atyp == 4)
	{
		// IPv6
		pack.from = read_v6_endpoint<udp::endpoint>(p);
	}
	else
	{
		std::uint8_t const len = read_uint8(p);
		if (len > pack.data.end() - p) return false;
		string_view hostname(p, len);
		p += len;

		error_code ec;
		address addr = make_address(std::string(hostname), ec);
		std::uint16_t const port = read_uint16(p);
		if (!ec)
			pack.from = udp::endpoint(addr, port);
		else
			pack.hostname = hostname;
	}

	pack.data = span<char>{p, size - (p - pack.data.data())};
	return true;
}

#if !defined BOOST_ASIO_ENABLE_CANCELIO && defined TORRENT_WINDOWS
#error BOOST_ASIO_ENABLE_CANCELIO needs to be defined when building libtorrent to enable cancel() in asio on windows
#endif

void udp_socket::close()
{
	TORRENT_ASSERT(is_single_thread());

	error_code ec;
	m_socket.close(ec);
	TORRENT_ASSERT_VAL(!ec || ec == error::bad_descriptor, ec);
	// if (m_socks5_connection)
	// {
	// 	m_socks5_connection->close();
	// 	m_socks5_connection.reset();
	// }
	m_abort = true;
}

void udp_socket::open(udp const& protocol, error_code& ec)
{
	TORRENT_ASSERT(is_single_thread());

	m_abort = false;

	if (m_socket.is_open()) m_socket.close(ec);
	ec.clear();

	m_socket.open(protocol, ec);
	if (ec) return;
	if (protocol == udp::v6())
	{
		error_code err;
		m_socket.set_option(boost::asio::ip::v6_only(true), err);

#ifdef TORRENT_WINDOWS
		// enable Teredo on windows
		m_socket.set_option(v6_protection_level(PROTECTION_LEVEL_UNRESTRICTED), err);
#endif // TORRENT_WINDOWS
	}

	// this is best-effort. ignore errors
#ifdef TORRENT_WINDOWS
	error_code err;
	m_socket.set_option(exclusive_address_use(true), err);
#endif
}

void udp_socket::bind(udp::endpoint const& ep, error_code& ec)
{
	if (!m_socket.is_open()) open(ep.protocol(), ec);
	if (ec) return;
	m_socket.bind(ep, ec);
	if (ec) return;
	m_socket.non_blocking(true, ec);
	if (ec) return;

	error_code err;
	m_bind_port = m_socket.local_endpoint(err).port();
	if (err) m_bind_port = ep.port();
}
/*
void udp_socket::set_proxy_settings(aux::proxy_settings const& ps
	, aux::alert_manager& alerts, aux::resolver_interface& resolver, bool const send_local_ep)
{
	TORRENT_ASSERT(is_single_thread());

	if (m_socks5_connection)
	{
		m_socks5_connection->close();
		m_socks5_connection.reset();
	}

	m_proxy_settings = ps;

	if (m_abort) return;

	if (ps.type == settings_pack::socks5
		|| ps.type == settings_pack::socks5_pw)
	{
		// connect to socks5 server and open up the UDP tunnel

		m_socks5_connection = std::make_shared<socks5>(m_ioc
			, m_listen_socket, alerts, resolver, send_local_ep);
		m_socks5_connection->start(ps);
	}
}
*/


constexpr udp_send_flags_t udp_socket::peer_connection;
constexpr udp_send_flags_t udp_socket::tracker_connection;
constexpr udp_send_flags_t udp_socket::dont_queue;
constexpr udp_send_flags_t udp_socket::dont_fragment;

}
