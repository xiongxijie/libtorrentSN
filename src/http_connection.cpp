/*

Copyright (c) 2007-2022, Arvid Norberg
Copyright (c) 2015, Mikhail Titov
Copyright (c) 2016-2018, 2020, Alden Torres
Copyright (c) 2016, Andrei Kurushin
Copyright (c) 2017, Jan Berkel
Copyright (c) 2017, Steven Siloti
Copyright (c) 2019, patch3proxyheaders915360
Copyright (c) 2020, Paul-Louis Ageneau
Copyright (c) 2022, AlexeyKhrolenko
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

#include "libtorrent/http_connection.hpp"
#include "libtorrent/aux_/escape_string.hpp"
#include "libtorrent/aux_/instantiate_connection.hpp"
#include "libtorrent/gzip.hpp"
#include "libtorrent/parse_url.hpp"
#include "libtorrent/socket.hpp"
#include "libtorrent/aux_/socket_type.hpp" // for async_shutdown
#include "libtorrent/aux_/resolver_interface.hpp"
#include "libtorrent/aux_/bind_to_device.hpp"
#include "libtorrent/settings_pack.hpp"
#include "libtorrent/aux_/time.hpp"
#include "libtorrent/random.hpp"
#include "libtorrent/debug.hpp"
#include "libtorrent/time.hpp"
#include "libtorrent/io_context.hpp"
#include "libtorrent/aux_/ip_helpers.hpp"

#include <functional>
#include <string>
#include <algorithm>
#include <sstream>

using namespace std::placeholders;

namespace libtorrent {

http_connection::http_connection(io_context& ios
	, aux::resolver_interface& resolver
	, http_handler handler
	, bool bottled
	, int max_bottled_buffer_size
	, http_connect_handler ch
	, http_filter_handler fh
	, hostname_filter_handler hfh
	)
	: m_ios(ios)
	, m_next_ep(0)
	, m_resolver(resolver)
	, m_handler(std::move(handler))
	, m_connect_handler(std::move(ch))
	, m_filter_handler(std::move(fh))
	, m_hostname_filter_handler(std::move(hfh))
	, m_timer(ios)
	, m_completion_timeout(seconds(5))
	, m_limiter_timer(ios)
	, m_last_receive(aux::time_now())
	, m_start_time(aux::time_now())
	, m_read_pos(0)
	, m_redirects(5)
	, m_max_bottled_buffer_size(max_bottled_buffer_size)
	, m_rate_limit(0)
	, m_download_quota(0)
	, m_resolve_flags{}
	, m_port(0)
	, m_bottled(bottled)
{
	TORRENT_ASSERT(m_handler);
}

http_connection::~http_connection() = default;

void http_connection::get(std::string const& url, time_duration timeout
	, int handle_redirects, std::string const& user_agent
	, boost::optional<bind_info_t> const& bind_addr
	, aux::resolver_flags const resolve_flags
	)
{
	m_user_agent = user_agent;
	m_resolve_flags = resolve_flags;

	std::string protocol;
	std::string path;
	std::string hostname;
	std::string auth;
	error_code ec;

	int port;

	std::tie(protocol, auth, hostname, port, path)
		= parse_url_components(url, ec);

	TORRENT_UNUSED(auth);


	// m_auth = auth;

	int default_port = protocol == "https" ? 443 : 80;
	if (port == -1) port = default_port;

	// keep ourselves alive even if the callback function
	// deletes this object
	std::shared_ptr<http_connection> me(shared_from_this());

	if (ec)
	{
		post(m_ios, std::bind(&http_connection::callback
			, me, ec, span<char>{}));
		return;
	}

	if (m_hostname_filter_handler && !m_hostname_filter_handler(*this, hostname))
	{
		error_code err(errors::blocked_by_idna);
		post(m_ios, std::bind(&http_connection::callback
			, me, err, span<char>{}));
		return;
	}

	if (protocol != "http")
	{
		error_code err(errors::unsupported_url_protocol);
		post(m_ios, std::bind(&http_connection::callback
			, me, err, span<char>{}));
		return;
	}

	bool const ssl = (protocol == "https");

	std::stringstream request;

	
	request << "GET " << path << " HTTP/1.1\r\nHost: " << hostname;
	if (port != default_port) request << ":" << port << "\r\n";
	else request << "\r\n";
	

//	request << "Accept: */*\r\n";

	if (!m_user_agent.empty())
		request << "User-Agent: " << m_user_agent << "\r\n";

	if (m_bottled)
		request << "Accept-Encoding: gzip\r\n";



	request << "Connection: close\r\n\r\n";

	m_sendbuffer.assign(request.str());
	m_url = url;
	start(hostname, port, timeout, ssl, handle_redirects, bind_addr, m_resolve_flags);

}

void http_connection::start(std::string const& hostname, int port
	, time_duration timeout, bool ssl
	, int handle_redirects
	, boost::optional<bind_info_t> const& bind_addr
	, aux::resolver_flags const resolve_flags

)
{
	m_redirects = handle_redirects;
	m_resolve_flags = resolve_flags;
	
	// keep ourselves alive even if the callback function
	// deletes this object
	std::shared_ptr<http_connection> me(shared_from_this());

	m_completion_timeout = timeout;
	m_timer.expires_after(m_completion_timeout);
	ADD_OUTSTANDING_ASYNC("http_connection::on_timeout");
	m_timer.async_wait(std::bind(&http_connection::on_timeout
		, std::weak_ptr<http_connection>(me), _1));
	m_called = false;
	m_parser.reset();
	m_recvbuffer.clear();
	m_read_pos = 0;

	if (m_sock && m_sock->is_open() && m_hostname == hostname && m_port == port
		&& m_ssl == ssl && m_bind_addr == bind_addr)
	{
		ADD_OUTSTANDING_ASYNC("http_connection::on_write");
		async_write(*m_sock, boost::asio::buffer(m_sendbuffer)
			, std::bind(&http_connection::on_write, me, _1));
	}
	else
	{
		m_ssl = ssl;
		m_bind_addr = bind_addr;
		error_code err;
		if (m_sock && m_sock->is_open()) m_sock->close(err);

		void* userdata = nullptr;

		// assume this is not a tracker connection. Tracker connections that
		// shouldn't be subject to the proxy should pass in nullptr as the proxy
		// pointer.
		m_sock.emplace(aux::instantiate_connection(m_ios, userdata, nullptr, false, false));

		if (m_bind_addr)
		{
			error_code ec;
			m_sock->open(m_bind_addr->ip.is_v4() ? tcp::v4() : tcp::v6(), ec);
#if TORRENT_HAS_BINDTODEVICE
			error_code ignore;
			bind_device(*m_sock, m_bind_addr->device.c_str(), ignore);
#endif
			m_sock->bind(tcp::endpoint(m_bind_addr->ip, 0), ec);
			if (ec)
			{
				post(m_ios, std::bind(&http_connection::callback
					, me, ec, span<char>{}));
				return;
			}
		}

		error_code ec;
		// setup_ssl_hostname(*m_sock, hostname, ec);
		if (ec)
		{
			post(m_ios, std::bind(&http_connection::callback
				, me, ec, span<char>{}));
			return;
		}

		m_endpoints.clear();
		m_next_ep = 0;



		{
			m_resolving_host = true;
			ADD_OUTSTANDING_ASYNC("http_connection::on_resolve");
			m_resolver.async_resolve(hostname, m_resolve_flags
				, std::bind(&http_connection::on_resolve
				, me, _1, _2));
		}
		m_hostname = hostname;
		m_port = std::uint16_t(port);
	}
}

void http_connection::on_timeout(std::weak_ptr<http_connection> p
	, error_code const& e)
{
	COMPLETE_ASYNC("http_connection::on_timeout");
	std::shared_ptr<http_connection> c = p.lock();
	if (!c) return;

	if (e == boost::asio::error::operation_aborted) return;

	if (c->m_abort) return;

	time_point const now = clock_type::now();

	// be forgiving of timeout while we're still resolving the hostname
	// it may be delayed because we're queued up behind another slow lookup
	if (c->m_resolving_host
		&& (c->m_start_time + (c->m_completion_timeout * 2) > now))
	{
		ADD_OUTSTANDING_ASYNC("http_connection::on_timeout");
		c->m_timer.expires_at(c->m_start_time + c->m_completion_timeout * 2);
		c->m_timer.async_wait(std::bind(&http_connection::on_timeout, p, _1));
		return;
	}

	if (c->m_start_time + c->m_completion_timeout <= now)
	{
		// the connection timed out. If we have more endpoints to try, just
		// close this connection. The on_connect handler will try the next
		// endpoint in the list.
		if (c->m_next_ep < int(c->m_endpoints.size()))
		{
			error_code ec;
			c->m_sock->close(ec);
			if (!c->m_connecting) c->connect();
			c->m_last_receive = now;
			c->m_start_time = c->m_last_receive;
		}
		else
		{
			// the socket may have an outstanding operation, that keeps the
			// http_connection object alive. We want to cancel all that.
			error_code ec;
			c->m_sock->close(ec);
			c->callback(lt::errors::timed_out);
			return;
		}
	}

	ADD_OUTSTANDING_ASYNC("http_connection::on_timeout");
	c->m_timer.expires_at(c->m_start_time + c->m_completion_timeout);
	c->m_timer.async_wait(std::bind(&http_connection::on_timeout, p, _1));
}

void http_connection::close(bool force)
{
	if (m_abort) return;

	if (m_sock)
	{
		error_code ec;
		if (force)
		{
			m_sock->close(ec);
			m_timer.cancel();
		}
		else
		{
			async_shutdown(*m_sock, shared_from_this());
		}
	}
	else
		m_timer.cancel();

	m_limiter_timer.cancel();

	m_hostname.clear();
	m_port = 0;
	m_handler = nullptr;
	m_abort = true;
}



void http_connection::on_resolve(error_code const& e
	, std::vector<address> const& addresses)
{
	COMPLETE_ASYNC("http_connection::on_resolve");
	m_resolving_host = false;
	if (e)
	{
		callback(e);
		return;
	}
	
	if (m_abort) return;

	TORRENT_ASSERT(!addresses.empty());

	// reset timeout
	m_start_time = clock_type::now();

	for (auto const& addr : addresses)
		m_endpoints.emplace_back(addr, m_port);

	if (m_filter_handler) m_filter_handler(*this, m_endpoints);
	if (m_endpoints.empty())
	{
		close();
		return;
	}

	aux::random_shuffle(m_endpoints);

	// if we have been told to bind to a particular address
	// only connect to addresses of the same family
	if (m_bind_addr)
	{
		auto const new_end = std::remove_if(m_endpoints.begin(), m_endpoints.end()
			, [&](tcp::endpoint const& ep) { return aux::is_v4(ep) != m_bind_addr->ip.is_v4(); });

		m_endpoints.erase(new_end, m_endpoints.end());
		if (m_endpoints.empty())
		{
			callback(error_code(boost::system::errc::address_family_not_supported, generic_category()));
			close();
			return;
		}
	}

	connect();
}

void http_connection::connect()
{
	TORRENT_ASSERT(m_next_ep < int(m_endpoints.size()));

	std::shared_ptr<http_connection> me(shared_from_this());
	
	

	TORRENT_ASSERT(m_next_ep < int(m_endpoints.size()));
	if (m_next_ep >= int(m_endpoints.size())) return;

	tcp::endpoint target_address = m_endpoints[m_next_ep];
	++m_next_ep;

	ADD_OUTSTANDING_ASYNC("http_connection::on_connect");
	TORRENT_ASSERT(!m_connecting);
	m_connecting = true;
	m_sock->async_connect(target_address, std::bind(&http_connection::on_connect
		, me, _1));
}

void http_connection::on_connect(error_code const& e)
{
	COMPLETE_ASYNC("http_connection::on_connect");
	TORRENT_ASSERT(m_connecting);
	m_connecting = false;

	m_last_receive = clock_type::now();
	m_start_time = m_last_receive;
	if (!e)
	{
		if (m_connect_handler) m_connect_handler(*this);
		ADD_OUTSTANDING_ASYNC("http_connection::on_write");
		async_write(*m_sock, boost::asio::buffer(m_sendbuffer)
			, std::bind(&http_connection::on_write, shared_from_this(), _1));
	}
	else if (m_next_ep < int(m_endpoints.size()) && !m_abort)
	{
		// The connection failed. Try the next endpoint in the list.
		error_code ec;
		m_sock->close(ec);
		connect();
	}
	else
	{
		error_code ec;
		m_sock->close(ec);
		callback(e);
	}
}

void http_connection::callback(error_code e, span<char> data)
{
	if (m_bottled && m_called) return;

	std::vector<char> buf;
	if (!data.empty() && m_bottled && m_parser.header_finished())
	{
		data = m_parser.collapse_chunk_headers(data);

		std::string const& encoding = m_parser.header("content-encoding");
		if (encoding == "gzip" || encoding == "x-gzip")
		{
			error_code ec;
			inflate_gzip(data, buf, m_max_bottled_buffer_size, ec);

			if (ec)
			{
				if (m_handler) m_handler(ec, m_parser, data, *this);
				return;
			}
			data = buf;
		}

		// if we completed the whole response, no need
		// to tell the user that the connection was closed by
		// the server or by us. Just clear any error
		if (m_parser.finished()) e.clear();
	}
	m_called = true;
	m_timer.cancel();
	if (m_handler) m_handler(e, m_parser, data, *this);
}

void http_connection::on_write(error_code const& e)
{
	COMPLETE_ASYNC("http_connection::on_write");

	if (e == boost::asio::error::operation_aborted) return;

	if (e)
	{
		callback(e);
		return;
	}

	if (m_abort) return;

	std::string().swap(m_sendbuffer);
	m_recvbuffer.resize(4096);

	int amount_to_read = int(m_recvbuffer.size()) - m_read_pos;
	if (m_rate_limit > 0 && amount_to_read > m_download_quota)
	{
		amount_to_read = m_download_quota;
		if (m_download_quota == 0)
		{
			if (!m_limiter_timer_active)
			{
				ADD_OUTSTANDING_ASYNC("http_connection::on_assign_bandwidth");
				on_assign_bandwidth(error_code());
			}
			return;
		}
	}
	ADD_OUTSTANDING_ASYNC("http_connection::on_read");
	m_sock->async_read_some(boost::asio::buffer(m_recvbuffer.data() + m_read_pos
		, std::size_t(amount_to_read))
		, std::bind(&http_connection::on_read
			, shared_from_this(), _1, _2));
}

void http_connection::on_read(error_code const& e
	, std::size_t bytes_transferred)
{
	COMPLETE_ASYNC("http_connection::on_read");

	if (m_rate_limit)
	{
		m_download_quota -= int(bytes_transferred);
		TORRENT_ASSERT(m_download_quota >= 0);
	}

	if (e == boost::asio::error::operation_aborted) return;

	if (m_abort) return;

	// keep ourselves alive even if the callback function
	// deletes this object
	std::shared_ptr<http_connection> me(shared_from_this());

	// when using the asio SSL wrapper, it seems like
	// we get the shut_down error instead of EOF
	if (e == boost::asio::error::eof || e == boost::asio::error::shut_down)
	{
		error_code ec = boost::asio::error::eof;
		TORRENT_ASSERT(bytes_transferred == 0);
		span<char> body;
		if (m_bottled && m_parser.header_finished())
		{
			body = span<char>(m_recvbuffer.data() + m_parser.body_start()
				, m_parser.get_body().size());
		}
		callback(ec, body);
		return;
	}

	if (e)
	{
		TORRENT_ASSERT(bytes_transferred == 0);
		callback(e);
		return;
	}

	m_read_pos += int(bytes_transferred);
	TORRENT_ASSERT(m_read_pos <= int(m_recvbuffer.size()));

	if (m_bottled || !m_parser.header_finished())
	{
		span<char const> rcv_buf(m_recvbuffer);
		bool error = false;
		m_parser.incoming(rcv_buf.first(m_read_pos), error);
		if (error)
		{
			// HTTP parse error
			error_code ec = errors::http_parse_error;
			callback(ec);
			return;
		}

		// having a nonempty path means we should handle redirects
		if (m_redirects && m_parser.header_finished())
		{
			int code = m_parser.status_code();

			if (is_redirect(code))
			{
				// attempt a redirect
				std::string const& location = m_parser.header("location");
				if (location.empty())
				{
					// missing location header
					callback(error_code(errors::http_missing_location));
					return;
				}

				error_code ec;
				// it would be nice to gracefully shut down SSL here
				// but then we'd have to do all the reconnect logic
				// in its handler. For now, just kill the connection.
//				async_shutdown(m_sock, me);
				m_sock->close(ec);

				std::string url = resolve_redirect_location(m_url, location);
				get(url, m_completion_timeout, m_redirects - 1
					, m_user_agent, m_bind_addr, m_resolve_flags
					);
				return;
			}

			m_redirects = 0;
		}

		if (!m_bottled && m_parser.header_finished())
		{
			if (m_read_pos > m_parser.body_start())
			{
				callback(e, span<char>(m_recvbuffer)
					.first(m_read_pos)
					.subspan(m_parser.body_start()));
			}
			m_read_pos = 0;
			m_last_receive = clock_type::now();
		}
		else if (m_bottled && m_parser.finished())
		{
			m_timer.cancel();
			callback(e, span<char>(m_recvbuffer)
				.first(m_read_pos)
				.subspan(m_parser.body_start()));
		}
	}
	else
	{
		TORRENT_ASSERT(!m_bottled);
		callback(e, span<char>(m_recvbuffer).first(m_read_pos));
		m_read_pos = 0;
		m_last_receive = clock_type::now();
	}

	// if we've hit the limit, double the buffer size
	if (int(m_recvbuffer.size()) == m_read_pos)
		m_recvbuffer.resize(std::min(m_read_pos * 2, m_max_bottled_buffer_size));

	if (m_read_pos == m_max_bottled_buffer_size)
	{
		// if we've reached the size limit, terminate the connection and
		// report the error
		callback(error_code(boost::system::errc::file_too_large, generic_category()));
		return;
	}
	int amount_to_read = int(m_recvbuffer.size()) - m_read_pos;
	if (m_rate_limit > 0 && amount_to_read > m_download_quota)
	{
		amount_to_read = m_download_quota;
		if (m_download_quota == 0)
		{
			if (!m_limiter_timer_active)
			{
				ADD_OUTSTANDING_ASYNC("http_connection::on_assign_bandwidth");
				on_assign_bandwidth(error_code());
			}
			return;
		}
	}
	ADD_OUTSTANDING_ASYNC("http_connection::on_read");
	m_sock->async_read_some(boost::asio::buffer(m_recvbuffer.data() + m_read_pos
		, std::size_t(amount_to_read))
		, std::bind(&http_connection::on_read
			, me, _1, _2));
}

void http_connection::on_assign_bandwidth(error_code const& e)
{
	COMPLETE_ASYNC("http_connection::on_assign_bandwidth");
	if ((e == boost::asio::error::operation_aborted
		&& m_limiter_timer_active)
		|| !m_sock->is_open())
	{
		callback(boost::asio::error::eof);
		return;
	}
	m_limiter_timer_active = false;
	if (e) return;

	if (m_abort) return;

	if (m_download_quota > 0) return;

	m_download_quota = m_rate_limit / 4;

	int amount_to_read = int(m_recvbuffer.size()) - m_read_pos;
	if (amount_to_read > m_download_quota)
		amount_to_read = m_download_quota;

	if (!m_sock->is_open()) return;

	ADD_OUTSTANDING_ASYNC("http_connection::on_read");
	m_sock->async_read_some(boost::asio::buffer(m_recvbuffer.data() + m_read_pos
		, std::size_t(amount_to_read))
		, std::bind(&http_connection::on_read
			, shared_from_this(), _1, _2));

	m_limiter_timer_active = true;
	m_limiter_timer.expires_after(milliseconds(250));
	ADD_OUTSTANDING_ASYNC("http_connection::on_assign_bandwidth");
	m_limiter_timer.async_wait(std::bind(&http_connection::on_assign_bandwidth
		, shared_from_this(), _1));
}

void http_connection::rate_limit(int limit)
{
	if (!m_sock->is_open()) return;

	if (!m_limiter_timer_active)
	{
		m_limiter_timer_active = true;
		m_limiter_timer.expires_after(milliseconds(250));
		ADD_OUTSTANDING_ASYNC("http_connection::on_assign_bandwidth");
		m_limiter_timer.async_wait(std::bind(&http_connection::on_assign_bandwidth
			, shared_from_this(), _1));
	}
	m_rate_limit = limit;
}

}
