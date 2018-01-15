/*=============================================================================
    Copyright (c) 2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "string_view.hpp"
#include <stdexcept>

namespace quickbook
{
    struct glob_error : std::runtime_error
    {
        explicit glob_error(char const* error) :
            std::runtime_error(error) {}
    };

    // Is this path a glob? Throws glob_error if glob is invalid.
    bool check_glob(quickbook::string_view);

    // pre: glob is valid (call check_glob first on user data).
    bool glob(quickbook::string_view const& pattern,
            quickbook::string_view const& filename);

    std::size_t find_glob_char(quickbook::string_view,
            std::size_t start = 0);
    std::string glob_unescape(quickbook::string_view);
}
