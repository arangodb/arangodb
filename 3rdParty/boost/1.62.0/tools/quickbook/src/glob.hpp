/*=============================================================================
    Copyright (c) 2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/utility/string_ref.hpp>
#include <stdexcept>

namespace quickbook
{
    struct glob_error : std::runtime_error
    {
        explicit glob_error(char const* error) :
            std::runtime_error(error) {}
    };

    // Is this path a glob? Throws glob_error if glob is invalid.
    bool check_glob(boost::string_ref);

    // pre: glob is valid (call check_glob first on user data).
    bool glob(boost::string_ref const& pattern,
            boost::string_ref const& filename);

    std::size_t find_glob_char(boost::string_ref,
            std::size_t start = 0);
    std::string glob_unescape(boost::string_ref);
}
