/*

Copyright (c) 2015-2020, Arvid Norberg
Copyright (c) 2016-2018, Alden Torres
Copyright (c) 2017, Pavel Pimenov
Copyright (c) 2017, Steven Siloti
Copyright (c) 2020, 2022, Vladimir Golovnev (glassez)
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

#include <cstdint>

#include "libtorrent/bdecode.hpp"
#include "libtorrent/read_resume_data.hpp"
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/socket_io.hpp" // for read_*_endpoint()
#include "libtorrent/hasher.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/aux_/numeric_cast.hpp"
#include "libtorrent/download_priority.hpp" // for default_priority

namespace libtorrent {

namespace {

	void apply_flag(torrent_flags_t& current_flags
		, bdecode_node const& n
		, char const* name
		, torrent_flags_t const flag)
	{
		if (n.dict_find_int_value(name, (flag & torrent_flags::default_flags) ? 1 : 0) == 0)
		{
			current_flags &= ~flag;
		}
		else
		{
			current_flags |= flag;
		}
	}

} // anonyous namespace

	add_torrent_params read_resume_data(bdecode_node const& rd, error_code& ec
		, int const piece_limit)
	{
		add_torrent_params ret;
		if (rd.type() != bdecode_node::dict_t)
		{
			ec = errors::not_a_dictionary;
			return ret;
		}

		if (bdecode_node const alloc = rd.dict_find_string("allocation"))
		{
			ret.storage_mode = (alloc.string_value() == "allocate"
				|| alloc.string_value() == "full")
				? storage_mode_allocate : storage_mode_sparse;
		}

		if (rd.dict_find_string_value("file-format")
			!= "libtorrent resume file")
		{
			ec = errors::invalid_file_tag;
			return ret;
		}

		std::int64_t const file_version = rd.dict_find_int_value("file-version", 1);

		if (file_version != 1 && file_version != 2)
		{
			ec = errors::invalid_file_tag;
			return ret;
		}

		auto info_hash = rd.dict_find_string_value("info-hash");
		// auto info_hash2 = rd.dict_find_string_value("info-hash2");
		if (info_hash.size() != std::size_t(sha1_hash::size())
			/*&& info_hash2.size() != std::size_t(sha256_hash::size())*/)
		{
			ec = errors::missing_info_hash;
			return ret;
		}

		ret.name = rd.dict_find_string_value("name").to_string();

		if (info_hash.size() == 20)
			ret.info_hashes.v1.assign(info_hash.data());
		// if (info_hash2.size() == 32)
		// 	ret.info_hashes.v2.assign(info_hash2.data());

		bdecode_node const info = rd.dict_find_dict("info");
		if (info)
		{
			// verify the info-hash of the metadata stored in the resume file matches
			// the torrent we're loading
			info_hash_t const resume_ih(hasher(info.data_section()).final()
				/*, hasher256(info.data_section()).final()*/);

			// if url is set, the info_hash is not actually the info-hash of the
			// torrent, but the hash of the URL, until we have the full torrent
			// only require the info-hash to match if we actually passed in one
			if ((!ret.info_hashes.has_v1() || resume_ih.v1 == ret.info_hashes.v1)
			/* && (!ret.info_hashes.has_v2() || resume_ih.v2 == ret.info_hashes.v2)*/
			)
			{
				ret.ti = std::make_shared<torrent_info>(resume_ih);

				error_code err;
				if (!ret.ti->parse_info_section(info, err, piece_limit))
				{
					ec = err;
				}
				else
				{
					// time_t might be 32 bit if we're unlucky, but there isn't
					// much to do about it
					ret.ti->internal_set_creation_date(static_cast<std::time_t>(
						rd.dict_find_int_value("creation date", 0)));
					ret.ti->internal_set_creator(rd.dict_find_string_value("created by", ""));
					ret.ti->internal_set_comment(rd.dict_find_string_value("comment", ""));
				}
			}
			else
			{
				ec = errors::mismatching_info_hash;
			}
		}

#if TORRENT_ABI_VERSION < 3
		ret.info_hash = ret.info_hashes.get();
#endif

		// bdecode_node const trees = rd.dict_find_list("trees");
		// if (trees)
		// {
		// 	ret.merkle_trees.reserve(trees.list_size());
		// 	ret.verified_leaf_hashes.reserve(trees.list_size());
		// 	for (int i = 0; i < trees.list_size(); ++i)
		// 	{
		// 		auto de = trees.list_at(i);
		// 		if (de.type() != bdecode_node::dict_t)
		// 			break;
		// 		auto dh = de.dict_find_string("hashes");
		// 		if (!dh || dh.string_length() % 32 != 0) break;

		// 		ret.merkle_trees.emplace_back();
		// 		ret.merkle_trees.back().reserve(dh.string_value().size() / 32);
		// 		for (auto hashes = dh.string_value();
		// 			!hashes.empty(); hashes = hashes.substr(32))
		// 		{
		// 			ret.merkle_trees.back().emplace_back(hashes);
		// 		}

		// 		if (bdecode_node const verified = de.dict_find_string("verified"))
		// 		{
		// 			string_view const str = verified.string_value();
		// 			if (file_version == 1)
		// 			{
		// 				ret.verified_leaf_hashes.emplace_back(str.size());
		// 				auto& v = ret.verified_leaf_hashes.back();
		// 				for (std::size_t j = 0; j < str.size(); ++j)
		// 				{
		// 					if (str[j] == '1')
		// 						v[j] = true;
		// 				}
		// 			}
		// 			else
		// 			{
		// 				ret.verified_leaf_hashes.emplace_back(str.size() * 8);
		// 				auto& v = ret.verified_leaf_hashes.back();
		// 				for (std::size_t j = 0; j < v.size(); ++j)
		// 				{
		// 					if (str[j / 8] & (0x80 >> (j % 8)))
		// 						v[j] = true;
		// 				}
		// 			}
		// 		}

		// 		if (bdecode_node const mask = de.dict_find_string("mask"))
		// 		{
		// 			string_view const str = mask.string_value();
		// 			if (file_version == 1)
		// 			{
		// 				ret.merkle_tree_mask.emplace_back(str.size());
		// 				auto& m = ret.merkle_tree_mask.back();
		// 				for (std::size_t j = 0; j < str.size(); ++j)
		// 				{
		// 					if (str[j] == '1')
		// 						m[j] = true;
		// 				}
		// 			}
		// 			else
		// 			{
		// 				ret.merkle_tree_mask.emplace_back(str.size() * 8);
		// 				auto& m = ret.merkle_tree_mask.back();
		// 				for (std::size_t j = 0; j < m.size(); ++j)
		// 				{
		// 					if (str[j / 8] & (0x80 >> (j % 8)))
		// 						m[j] = true;
		// 				}
		// 			}
		// 		}
		// 	}
		// }

		ret.total_uploaded = rd.dict_find_int_value("total_uploaded");
		ret.total_downloaded = rd.dict_find_int_value("total_downloaded");

		ret.active_time = int(rd.dict_find_int_value("active_time"));
		ret.finished_time = int(rd.dict_find_int_value("finished_time"));
		ret.seeding_time = int(rd.dict_find_int_value("seeding_time"));

		ret.last_seen_complete = std::time_t(rd.dict_find_int_value("last_seen_complete"));

		ret.last_download = std::time_t(rd.dict_find_int_value("last_download", 0));
		ret.last_upload = std::time_t(rd.dict_find_int_value("last_upload", 0));

		// scrape data cache
		ret.num_complete = int(rd.dict_find_int_value("num_complete", -1));
		ret.num_incomplete = int(rd.dict_find_int_value("num_incomplete", -1));
		ret.num_downloaded = int(rd.dict_find_int_value("num_downloaded", -1));

		// torrent settings
		ret.max_uploads = int(rd.dict_find_int_value("max_uploads", -1));
		ret.max_connections = int(rd.dict_find_int_value("max_connections", -1));
		ret.upload_limit = int(rd.dict_find_int_value("upload_rate_limit", -1));
		ret.download_limit = int(rd.dict_find_int_value("download_rate_limit", -1));

		// torrent flags
		apply_flag(ret.flags, rd, "seed_mode", torrent_flags::seed_mode);
		apply_flag(ret.flags, rd, "upload_mode", torrent_flags::upload_mode);

		apply_flag(ret.flags, rd, "apply_ip_filter", torrent_flags::apply_ip_filter);
		apply_flag(ret.flags, rd, "paused", torrent_flags::paused);
		apply_flag(ret.flags, rd, "auto_managed", torrent_flags::auto_managed);


		apply_flag(ret.flags, rd, "sequential_download", torrent_flags::sequential_download);
		apply_flag(ret.flags, rd, "stop_when_ready", torrent_flags::stop_when_ready);
		apply_flag(ret.flags, rd, "disable_lsd", torrent_flags::disable_lsd);
		apply_flag(ret.flags, rd, "disable_pex", torrent_flags::disable_pex);

		ret.save_path = rd.dict_find_string_value("save_path").to_string();

#if TORRENT_ABI_VERSION == 1
		// deprecated in 1.2
		ret.url = rd.dict_find_string_value("url").to_string();
#endif

		bdecode_node const mapped_files = rd.dict_find_list("mapped_files");
		if (mapped_files)
		{
			for (int i = 0; i < mapped_files.list_size(); ++i)
			{
				auto new_filename = mapped_files.list_string_value_at(i);
				if (new_filename.empty()) continue;
				ret.renamed_files[file_index_t(i)] = new_filename.to_string();
			}
		}

		ret.added_time = std::time_t(rd.dict_find_int_value("added_time", 0));
		ret.completed_time = std::time_t(rd.dict_find_int_value("completed_time", 0));


		// load file priorities except if the add_torrent_param file was set to
		// override resume data
		bdecode_node const file_priority = rd.dict_find_list("file_priority");
		if (file_priority)
		{
			int const num_files = file_priority.list_size();
			ret.file_priorities.resize(aux::numeric_cast<std::size_t>(num_files)
				, default_priority);
			for (int i = 0; i < num_files; ++i)
			{
				auto const idx = static_cast<std::size_t>(i);
				ret.file_priorities[idx] = aux::clamp(
					download_priority_t(static_cast<std::uint8_t>(
						file_priority.list_int_value_at(i
							, static_cast<std::uint8_t>(default_priority))))
						, dont_download, top_priority);
				// this is suspicious, leave seed mode
				if (ret.file_priorities[idx] == dont_download)
				{
					ret.flags &= ~torrent_flags::seed_mode;
				}
			}
		}



		//==============trackers field===================
		bdecode_node const trackers = rd.dict_find_list("trackers");
		if (trackers)
		{
			// it's possible to delete the trackers from a torrent and then save
			// resume data with an empty trackers list. Since we found a trackers
			// list here, these should replace whatever we find in the .torrent
			// file.
			ret.flags |= torrent_flags::override_trackers;

			int tier = 0;
			for (int i = 0; i < trackers.list_size(); ++i)
			{
				bdecode_node const tier_list = trackers.list_at(i);
				if (!tier_list || tier_list.type() != bdecode_node::list_t)
					continue;

				for (int j = 0; j < tier_list.list_size(); ++j)
				{
					ret.trackers.push_back(tier_list.list_string_value_at(j).to_string());
					ret.tracker_tiers.push_back(tier);

				}
				++tier;
			}
		}



		//==============pieces field===================
		// some sanity checking. Maybe we shouldn't be in seed mode anymore
		if (bdecode_node const pieces = rd.dict_find_string("pieces"))
		{
			if (file_version == 1)
			{
				char const* pieces_str = pieces.string_ptr();
				int const pieces_len = pieces.string_length();
				ret.have_pieces.resize(pieces_len);
				ret.verified_pieces.resize(pieces_len);
				bool any_verified = false;
				for (piece_index_t i(0); i < ret.verified_pieces.end_index(); ++i)
				{
					// being in seed mode and missing a piece is not compatible.
					// Leave seed mode if that happens
					if (pieces_str[static_cast<int>(i)] & 1) ret.have_pieces.set_bit(i);
					else ret.have_pieces.clear_bit(i);

					if (pieces_str[static_cast<int>(i)] & 2)
					{
						ret.verified_pieces.set_bit(i);
						any_verified = true;
					}
					else
					{
						ret.verified_pieces.clear_bit(i);
					}
				}
				if (!any_verified) ret.verified_pieces.clear();
			}
			else if (file_version == 2)
			{
				string_view const str = pieces.string_value();
				ret.have_pieces.assign(str.data(), int(str.size()) * 8);
			}
		}

		// if (bdecode_node const verified = rd.dict_find_string("verified"))
		// {
		// 	string_view const str = verified.string_value();
		// 	ret.verified_pieces.assign(str.data(), int(str.size()) * 8);
		// }

		if (bdecode_node const piece_priority = rd.dict_find_string("piece_priority"))
		{
			char const* prio_str = piece_priority.string_ptr();
			ret.piece_priorities.resize(aux::numeric_cast<std::size_t>(piece_priority.string_length()));
			for (std::size_t i = 0; i < ret.piece_priorities.size(); ++i)
			{
				ret.piece_priorities[i] = download_priority_t(aux::clamp(
					static_cast<std::uint8_t>(prio_str[i])
					, static_cast<std::uint8_t>(dont_download)
					, static_cast<std::uint8_t>(top_priority)));
			}
		}


		//==============peers field===================
		int const v6_size = 18;
		int const v4_size = 6;
		using namespace libtorrent::aux; // for read_*_endpoint()
		if (bdecode_node const peers_entry = rd.dict_find_string("peers"))
		{
			char const* ptr = peers_entry.string_ptr();
			for (int i = v4_size - 1; i < peers_entry.string_length(); i += v4_size)
				ret.peers.push_back(read_v4_endpoint<tcp::endpoint>(ptr));
		}
		if (bdecode_node const peers_entry = rd.dict_find_string("peers6"))
		{
			char const* ptr = peers_entry.string_ptr();
			for (int i = v6_size - 1; i < peers_entry.string_length(); i += v6_size)
				ret.peers.push_back(read_v6_endpoint<tcp::endpoint>(ptr));
		}

		if (bdecode_node const peers_entry = rd.dict_find_string("banned_peers"))
		{
			char const* ptr = peers_entry.string_ptr();
			for (int i = v4_size; i < peers_entry.string_length(); i += v4_size)
				ret.banned_peers.push_back(read_v4_endpoint<tcp::endpoint>(ptr));
		}

		if (bdecode_node const peers_entry = rd.dict_find_string("banned_peers6"))
		{
			char const* ptr = peers_entry.string_ptr();
			for (int i = v6_size - 1; i < peers_entry.string_length(); i += v6_size)
				ret.banned_peers.push_back(read_v6_endpoint<tcp::endpoint>(ptr));
		}


		//==============unfinished field===================
		// parse unfinished pieces
		if (bdecode_node const unfinished_entry = rd.dict_find_list("unfinished"))
		{
			for (int i = 0; i < unfinished_entry.list_size(); ++i)
			{
				bdecode_node const e = unfinished_entry.list_at(i);
				if (e.type() != bdecode_node::dict_t) continue;
				piece_index_t const piece = piece_index_t(int(e.dict_find_int_value("piece", -1)));
				if (piece < piece_index_t(0)) continue;

				bdecode_node const bitmask = e.dict_find_string("bitmask");
				if (!bitmask || bitmask.string_length() == 0) continue;
				ret.unfinished_pieces[piece].assign(
					bitmask.string_ptr(), bitmask.string_length() * CHAR_BIT);
			}
		}

		// we're loading this torrent from resume data. There's no need to
		// re-save the resume data immediately.
		ret.flags &= ~torrent_flags::need_save_resume;

		return ret;
	}

	add_torrent_params read_resume_data(span<char const> buffer, error_code& ec
		, load_torrent_limits const& cfg)
	{
		int pos;
		bdecode_node rd = bdecode(buffer, ec, &pos, cfg.max_decode_depth
			, cfg.max_decode_tokens);
		if (ec) return add_torrent_params();

		return read_resume_data(rd, ec, cfg.max_pieces);
	}

	add_torrent_params read_resume_data(bdecode_node const& rd, int const piece_limit)
	{
		error_code ec;
		auto ret = read_resume_data(rd, ec, piece_limit);
		if (ec) throw system_error(ec);
		return ret;
	}

	add_torrent_params read_resume_data(span<char const> buffer
		, load_torrent_limits const& cfg)
	{
		int pos;
		error_code ec;
		bdecode_node rd = bdecode(buffer, ec, &pos, cfg.max_decode_depth
			, cfg.max_decode_tokens);
		if (ec) throw system_error(ec);

		auto ret = read_resume_data(rd, ec, cfg.max_pieces);
		if (ec) throw system_error(ec);
		return ret;
	}
}
