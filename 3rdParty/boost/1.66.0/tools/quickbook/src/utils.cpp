/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include "utils.hpp"

#include <cctype>
#include <cstring>
#include <map>

namespace quickbook { namespace detail
{
    std::string encode_string(quickbook::string_view str)
    {
        std::string result;
        result.reserve(str.size());

        for (string_iterator it = str.begin();
            it != str.end(); ++it)
        {
            switch (*it)
            {
                case '<': result += "&lt;";    break;
                case '>': result += "&gt;";    break;
                case '&': result += "&amp;";   break;
                case '"': result += "&quot;";  break;
                default:  result += *it;       break;
            }
        }

        return result;
    }

    void print_char(char ch, std::ostream& out)
    {
        switch (ch)
        {
            case '<': out << "&lt;";    break;
            case '>': out << "&gt;";    break;
            case '&': out << "&amp;";   break;
            case '"': out << "&quot;";  break;
            default:  out << ch;        break;
            // note &apos; is not included. see the curse of apos:
            // http://fishbowl.pastiche.org/2003/07/01/the_curse_of_apos
        }
    }

    void print_string(quickbook::string_view str, std::ostream& out)
    {
        for (string_iterator cur = str.begin();
            cur != str.end(); ++cur)
        {
            print_char(*cur, out);
        }
    }

    std::string make_identifier(quickbook::string_view text)
    {
        std::string id(text.begin(), text.end());
        for (std::string::iterator i = id.begin(); i != id.end(); ++i) {
            if (!std::isalnum(static_cast<unsigned char>(*i))) {
                *i = '_';
            }
            else {
                *i = static_cast<char>(std::tolower(static_cast<unsigned char>(*i)));
            }
        }

        return id;
    }

    static std::string escape_uri_impl(quickbook::string_view uri_param, char const* mark)
    {
        // Extra capital characters for validating percent escapes.
        static char const hex[] = "0123456789abcdefABCDEF";

        std::string uri;
        uri.reserve(uri_param.size());

        for (std::string::size_type n = 0; n < uri_param.size(); ++n)
        {
            if (static_cast<unsigned char>(uri_param[n]) > 127 ||
                    (!std::isalnum(static_cast<unsigned char>(uri_param[n])) &&
                     !std::strchr(mark, uri_param[n])) ||
                    (uri_param[n] == '%' && !(n + 2 < uri_param.size() &&
                                        std::strchr(hex, uri_param[n+1]) &&
                                        std::strchr(hex, uri_param[n+2]))))
            {
                char escape[] = { '%', hex[uri_param[n] / 16], hex[uri_param[n] % 16], '\0' };
                uri += escape;
            }
            else
            {
                uri += uri_param[n];
            }
        }

        return uri;
    }

    std::string escape_uri(quickbook::string_view uri_param)
    {
        std::string uri(uri_param.begin(), uri_param.end());
        return escape_uri_impl(uri_param, "-_.!~*'()?\\/");
    }

    std::string partially_escape_uri(quickbook::string_view uri_param)
    {
        return escape_uri_impl(uri_param, "-_.!~*'()?\\/:&=#%+");
    }
}}
