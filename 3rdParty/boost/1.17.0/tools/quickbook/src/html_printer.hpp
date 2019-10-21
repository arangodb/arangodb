/*=============================================================================
Copyright (c) 2017 Daniel James

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_BOOSTBOOK_HTML_PRINTER_HPP)
#define BOOST_QUICKBOOK_BOOSTBOOK_HTML_PRINTER_HPP

#include <string>
#include "string_view.hpp"

namespace quickbook
{
    namespace detail
    {
        struct html_printer;

        void open_tag(html_printer&, quickbook::string_view name);
        void close_tag(html_printer&, quickbook::string_view name);
        void tag_attribute(
            html_printer&,
            quickbook::string_view name,
            quickbook::string_view value);
        void tag_start(html_printer&, quickbook::string_view name);
        void tag_end(html_printer&);
        void tag_end_self_close(html_printer&);

        struct html_printer
        {
            std::string html;
        };
    }
}

#endif
