/*=============================================================================
Copyright (c) 2017 Daniel James

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "html_printer.hpp"
#include "utils.hpp"

namespace quickbook
{
    namespace detail
    {
        void open_tag(html_printer& printer, quickbook::string_view name)
        {
            tag_start(printer, name);
            tag_end(printer);
        }

        void close_tag(html_printer& printer, quickbook::string_view name)
        {
            printer.html += "</";
            printer.html.append(name.begin(), name.end());
            printer.html += ">";
        }

        void tag_start(html_printer& printer, quickbook::string_view name)
        {
            printer.html += "<";
            printer.html.append(name.begin(), name.end());
        }

        void tag_end(html_printer& printer) { printer.html += ">"; }

        void tag_end_self_close(html_printer& printer) { printer.html += "/>"; }

        void tag_attribute(
            html_printer& printer,
            quickbook::string_view name,
            quickbook::string_view value)
        {
            printer.html += " ";
            printer.html.append(name.begin(), name.end());
            printer.html += "=\"";
            printer.html.append(encode_string(value));
            printer.html += "\"";
        }
    }
}
