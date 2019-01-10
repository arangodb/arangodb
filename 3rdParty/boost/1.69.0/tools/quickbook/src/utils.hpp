/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_QUICKBOOK_UTILS_HPP)
#define BOOST_SPIRIT_QUICKBOOK_UTILS_HPP

#include <ostream>
#include <string>
#include "string_view.hpp"

namespace quickbook
{
    namespace detail
    {
        std::string decode_string(quickbook::string_view);
        std::string encode_string(quickbook::string_view);
        void print_char(char ch, std::ostream& out);
        void print_string(quickbook::string_view str, std::ostream& out);
        std::string make_identifier(quickbook::string_view);

        // URI escape string
        std::string escape_uri(quickbook::string_view);

        // URI escape string, leaving characters generally used in URIs.
        std::string partially_escape_uri(quickbook::string_view);

        // Defined in id_xml.cpp. Just because.
        std::string linkify(
            quickbook::string_view source, quickbook::string_view linkend);
    }
}

#endif // BOOST_SPIRIT_QUICKBOOK_UTILS_HPP
