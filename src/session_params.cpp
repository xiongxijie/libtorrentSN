/*

Copyright (c) 2016, Alden Torres
Copyright (c) 2017, Steven Siloti
Copyright (c) 2019-2020, Arvid Norberg
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

#include "libtorrent/session_params.hpp"
#include "libtorrent/aux_/session_impl.hpp"
#include "libtorrent/extensions/ut_pex.hpp"
#include "libtorrent/extensions/ut_metadata.hpp"
#include "libtorrent/extensions/smart_ban.hpp"

namespace libtorrent {

namespace {

std::vector<std::shared_ptr<plugin>> default_plugins(
	bool empty = false)
{
#ifndef TORRENT_DISABLE_EXTENSIONS
	if (empty) return {};
	using wrapper = aux::session_impl::session_plugin_wrapper;
	return {
		std::make_shared<wrapper>(create_ut_pex_plugin),
		std::make_shared<wrapper>(create_ut_metadata_plugin),
		std::make_shared<wrapper>(create_smart_ban_plugin)
	};
#else
	TORRENT_UNUSED(empty);
	return {};
#endif
}

} // anonymous namespace

TORRENT_VERSION_NAMESPACE_3

session_params::session_params(settings_pack&& sp)
	: session_params(std::move(sp), default_plugins())
{}

session_params::session_params(settings_pack const& sp)
	: session_params(sp, default_plugins())
{}

session_params::session_params()
	: extensions(default_plugins())

{}

session_params::session_params(settings_pack&& sp
	, std::vector<std::shared_ptr<plugin>> exts)
	: settings(std::move(sp))
	, extensions(std::move(exts))

{}

session_params::session_params(settings_pack const& sp // NOLINT
	, std::vector<std::shared_ptr<plugin>> exts)
	: settings(sp)
	, extensions(std::move(exts))

	{}

session_params::session_params(session_params const&) = default;
session_params::session_params(session_params&&) = default;
session_params::~session_params() = default;

session_params& session_params::operator=(session_params const&) & = default;
session_params& session_params::operator=(session_params&&) & = default;

TORRENT_VERSION_NAMESPACE_3_END

session_params read_session_params(bdecode_node const& e, save_state_flags_t const flags)
{
	session_params params;

	if (e.type() != bdecode_node::dict_t) return params;


	if (flags & session_handle::save_settings)
	{
		bdecode_node settings = e.dict_find_dict("settings");
		if (settings) params.settings = load_pack_from_dict(settings);
	}


#ifndef TORRENT_DISABLE_EXTENSIONS
	if (flags & session::save_extension_state)
	{
		auto ext = e.dict_find_dict("extensions");
		if (ext)
		{
			for (int i = 0; i < ext.dict_size(); ++i)
			{
				bdecode_node val;
				string_view key;
				std::tie(key, val) = ext.dict_at(i);
				if (val.type() != bdecode_node::string_t) continue;
				params.ext_state[std::string(key)] = std::string(val.string_value());
			}
		}
	}
#endif

	if (flags & session_handle::save_ip_filter)
	{
		auto const v4 = e.dict_find_list("ip_filter4");
		ip_filter load;
		if (v4)
		{
			int const count = v4.list_size();
			for (int i = 0; i < count; ++i)
			{
				auto const str = v4.list_string_value_at(i);
				if (str.size() < 4 + 4 + 4) continue;
				char const* ptr = str.data();
				auto const first = aux::read_v4_address(ptr);
				auto const last = aux::read_v4_address(ptr);
				auto const f = aux::read_uint32(ptr);
				// ignore invalid entries
				if (first > last) continue;
				load.add_rule(first, last, f);
			}
		}

		auto const v6 = e.dict_find_list("ip_filter6");
		if (v6)
		{
			int const count = v6.list_size();
			for (int i = 0; i < count; ++i)
			{
				auto const str = v6.list_string_value_at(i);
				if (str.size() < 16 + 16 + 4) continue;
				char const* ptr = str.data();
				auto const first = aux::read_v6_address(ptr);
				auto const last = aux::read_v6_address(ptr);
				auto const f = aux::read_uint32(ptr);
				// ignore invalid entries
				if (first > last) continue;
				load.add_rule(first, last, f);
			}
		}

		if (!load.empty())
		{
			params.ip_filter = std::move(load);
		}
	}

	return params;
}

session_params read_session_params(span<char const> buf
	, save_state_flags_t const flags)
{
	return read_session_params(bdecode(buf), flags);
}


/*

	serialized session_params file like this:


	{
		'extensions': {},
		'ip_filter4': [ '00000000ffffffff00000000' ],
		'ip_filter6': [ '00000000000000000000000000000000ffffffffffffffffffffffffffffffff00000000' ],
		'settings': { 'alert_mask': 73, 'enable_natpmp': 0, 'enable_upnp': 0 }
	}

	GTK client settings are write in there

*/
entry write_session_params(session_params const& sp, save_state_flags_t const flags)
{
	entry e;
	/* bencode settings_pack settings items
	   structure like below :
		'settings': { 'alert_mask': 73, 'enable_natpmp': 0, 'enable_upnp': 0 }
	*/
	
	if (flags & session_handle::save_settings)
	{
		save_settings_to_dict(sp.settings, e["settings"].dict());
	}

#ifndef TORRENT_DISABLE_EXTENSIONS
	if (flags & session::save_extension_state)
	{
		auto& ext = e["extensions"].dict();
		for (auto const& val : sp.ext_state) ext[val.first] = val.second;
	}
#endif

	if (flags & session_handle::save_ip_filter)
	{
		std::vector<ip_range<address_v4>> v4;
		std::vector<ip_range<address_v6>> v6;
		std::tie(v4, v6) = sp.ip_filter.export_filter();
		if (!v4.empty())
		{
			auto& v4_list = e["ip_filter4"].list();
			for (auto const& ent : v4)
			{
				v4_list.emplace_back();
				auto ptr = std::back_inserter(v4_list.back().string());
				aux::write_address(ent.first, ptr);
				aux::write_address(ent.last, ptr);
				aux::write_uint32(ent.flags, ptr);
			}
		}
		if (!v6.empty())
		{
			auto& v6_list = e["ip_filter6"].list();
			for (auto const& ent : v6)
			{
				v6_list.emplace_back();
				auto ptr = std::back_inserter(v6_list.back().string());
				aux::write_address(ent.first, ptr);
				aux::write_address(ent.last, ptr);
				aux::write_uint32(ent.flags, ptr);
			}
		}
	}

	return e;
}

std::vector<char> write_session_params_buf(session_params const& sp, save_state_flags_t const flags)
{
	auto const e = write_session_params(sp, flags);
	std::vector<char> ret;
	bencode(std::back_inserter(ret), e);
	return ret;
}

}

