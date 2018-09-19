/*=============================================================================
    Copyright (c) 2017 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_BOOSTBOOK_TO_HTML_HPP)
#define BOOST_QUICKBOOK_BOOSTBOOK_TO_HTML_HPP

#include <string>
#include "path.hpp"
#include "string_view.hpp"

namespace quickbook
{
    namespace detail
    {
        struct html_options
        {
            bool chunked_output;
            boost::filesystem::path home_path;
            path_or_url boost_root_path;
            path_or_url css_path;
            path_or_url graphics_path;
            bool pretty_print;

            html_options() : chunked_output(false) {}
        };

        int boostbook_to_html(quickbook::string_view, html_options const&);
    }
}

#endif // BOOST_QUICKBOOK_BOOSTBOOK_TO_HTML_HPP
