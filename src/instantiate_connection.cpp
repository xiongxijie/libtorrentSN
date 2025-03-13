/*

Copyright (c) 2007, 2010-2011, 2014-2020, Arvid Norberg
Copyright (c) 2018, Alden Torres
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

#include "libtorrent/socket.hpp"
#include "libtorrent/aux_/socket_type.hpp"
#include "libtorrent/aux_/utp_socket_manager.hpp"
#include "libtorrent/aux_/instantiate_connection.hpp"
#include "libtorrent/aux_/utp_stream.hpp"
// #include "libtorrent/ssl_stream.hpp"

namespace libtorrent { namespace aux {

	// TODO: 2 peer_connection and tracker_connection should probably be flags
	aux::socket_type instantiate_connection(io_context& ios
		, void* ssl_context
		, utp_socket_manager* sm
		, bool peer_connection
		, bool tracker_connection)
	{
		if (sm)
		{
				utp_stream s(ios);
				s.set_impl(sm->new_utp_socket(&s));
				return socket_type(std::move(s));			
		}
			
	
		return socket_type(tcp::socket(ios));
		
		
		TORRENT_ASSERT_FAIL();
		throw std::runtime_error("unknown socket type");
	}

}}
