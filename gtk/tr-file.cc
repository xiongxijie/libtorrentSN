// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <string>
#include <string_view>
#include <vector>

#include "tr-error.h"
#include "tr-file.h"
#include "tr-assert.h"

std::vector<std::string> tr_sys_dir_get_files(
    std::string_view folder,
    std::function<bool(std::string_view)> const& test,
    tr_error* error)
{
    if (auto const info = tr_sys_path_get_info(folder); !info || !info->isFolder())
    {
        return {};
    }

    auto const odir = tr_sys_dir_open(folder, error);
    if (odir == TR_BAD_SYS_DIR)
    {
        return {};
    }

    auto filenames = std::vector<std::string>{};
    for (;;)
    {
        char const* const name = tr_sys_dir_read_name(odir, error);

        if (name == nullptr)
        {
            tr_sys_dir_close(odir, error);
            return filenames;
        }

        if (test(name))
        {
            filenames.emplace_back(name);
        }
    }
}
