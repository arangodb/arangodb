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
    std::string encode_string(boost::string_ref str)
    {
        std::string result;
        result.reserve(str.size());

        for (boost::string_ref::const_iterator it = str.begin();
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

    void print_string(boost::string_ref str, std::ostream& out)
    {
        for (boost::string_ref::const_iterator cur = str.begin();
            cur != str.end(); ++cur)
        {
            print_char(*cur, out);
        }
    }

    char filter_identifier_char(char ch)
    {
        if (!std::isalnum(static_cast<unsigned char>(ch)))
            ch = '_';
        return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    static std::string escape_uri_impl(std::string& uri_param, char const* mark)
    {
        // Extra capital characters for validating percent escapes.
        static char const hex[] = "0123456789abcdefABCDEF";

        std::string uri;
        uri.swap(uri_param);

        for (std::string::size_type n = 0; n < uri.size(); ++n)
        {
            if (static_cast<unsigned char>(uri[n]) > 127 ||
                    (!std::isalnum(static_cast<unsigned char>(uri[n])) &&
                     !std::strchr(mark, uri[n])) ||
                    (uri[n] == '%' && !(n + 2 < uri.size() &&
                                        std::strchr(hex, uri[n+1]) &&
                                        std::strchr(hex, uri[n+2]))))
            {
                char escape[] = { hex[uri[n] / 16], hex[uri[n] % 16] };
                uri.insert(n + 1, escape, 2);
                uri[n] = '%';
                n += 2;
            }
            else if (uri[n] == '%')
            {
                n += 2;
            }
        }

        return uri;
    }

    std::string escape_uri(std::string uri_param)
    {
        // TODO: I don't understand this choice of characters.....
        return escape_uri_impl(uri_param, "-_.!~*'()?\\/");
    }

    std::string partially_escape_uri(std::string uri_param)
    {
        return escape_uri_impl(uri_param, "-_.!~*'()?\\/:&=#%+");
    }
}}
