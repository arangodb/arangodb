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
#include <boost/spirit/include/classic_chset.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_numerics.hpp>
#include <boost/spirit/include/phoenix1_binders.hpp>
#include <boost/spirit/include/phoenix1_primitives.hpp>

namespace quickbook
{
    namespace detail
    {
        namespace cl = boost::spirit::classic;
        namespace ph = phoenix;

        struct xml_decode_grammar : cl::grammar<xml_decode_grammar>
        {
            std::string& result;
            xml_decode_grammar(std::string& result_) : result(result_) {}

            void append_char(char c) const { result += c; }

            void append_escaped_char(unsigned int c) const
            {
                if (c < 0x80) {
                    result += static_cast<char>(c);
                }
                else if (c < 0x800) {
                    char e[] = {static_cast<char>(0xc0 + (c >> 6)),
                                static_cast<char>(0x80 + (c & 0x3f)), '\0'};
                    result += e;
                }
                else if (c < 0x10000) {
                    char e[] = {static_cast<char>(0xe0 + (c >> 12)),
                                static_cast<char>(0x80 + ((c >> 6) & 0x3f)),
                                static_cast<char>(0x80 + (c & 0x3f)), '\0'};
                    result += e;
                }
                else if (c < 0x110000) {
                    char e[] = {static_cast<char>(0xf0 + (c >> 18)),
                                static_cast<char>(0x80 + ((c >> 12) & 0x3f)),
                                static_cast<char>(0x80 + ((c >> 6) & 0x3f)),
                                static_cast<char>(0x80 + (c & 0x3f)), '\0'};
                    result += e;
                }
                else {
                    result += "\xEF\xBF\xBD";
                }
            }

            template <typename Scanner> struct definition
            {
                definition(xml_decode_grammar const& self)
                {
                    // clang-format off

                    auto append_escaped_char = ph::bind(&xml_decode_grammar::append_escaped_char);
                    auto append_char = ph::bind(&xml_decode_grammar::append_char);
                    auto encoded =
                            cl::ch_p('&')
                        >>  (   "#x"
                            >>  cl::hex_p           [append_escaped_char(self, ph::arg1)]
                            >>  !cl::ch_p(';')
                            |   '#'
                            >>  cl::uint_p          [append_escaped_char(self, ph::arg1)]
                            >>  !cl::ch_p(';')
                            |   cl::str_p("amp;")   [append_char(self, '&')]
                            |   cl::str_p("apos;")  [append_char(self, '\'')]
                            |   cl::str_p("gt;")    [append_char(self, '>')]
                            |   cl::str_p("lt;")    [append_char(self, '<')]
                            |   cl::str_p("quot;")  [append_char(self, '"')]
                            );
                    auto character = cl::anychar_p [append_char(self, ph::arg1)];
                    xml_encoded = *(encoded | character);

                    // clang-format on
                }

                cl::rule<Scanner> const& start() { return xml_encoded; }
                cl::rule<Scanner> xml_encoded;
            };

          private:
            xml_decode_grammar& operator=(xml_decode_grammar const&);
        };

        std::string decode_string(quickbook::string_view str)
        {
            std::string result;
            xml_decode_grammar xml_decode(result);
            boost::spirit::classic::parse(str.begin(), str.end(), xml_decode);
            return result;
        }

        std::string encode_string(quickbook::string_view str)
        {
            std::string result;
            result.reserve(str.size());

            for (string_iterator it = str.begin(); it != str.end(); ++it) {
                switch (*it) {
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                case '&':
                    result += "&amp;";
                    break;
                case '"':
                    result += "&quot;";
                    break;
                default:
                    result += *it;
                    break;
                }
            }

            return result;
        }

        void print_char(char ch, std::ostream& out)
        {
            switch (ch) {
            case '<':
                out << "&lt;";
                break;
            case '>':
                out << "&gt;";
                break;
            case '&':
                out << "&amp;";
                break;
            case '"':
                out << "&quot;";
                break;
            default:
                out << ch;
                break;
                // note &apos; is not included. see the curse of apos:
                // http://fishbowl.pastiche.org/2003/07/01/the_curse_of_apos
            }
        }

        void print_string(quickbook::string_view str, std::ostream& out)
        {
            for (string_iterator cur = str.begin(); cur != str.end(); ++cur) {
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
                    *i = static_cast<char>(
                        std::tolower(static_cast<unsigned char>(*i)));
                }
            }

            return id;
        }

        static std::string escape_uri_impl(
            quickbook::string_view uri_param, char const* mark)
        {
            // Extra capital characters for validating percent escapes.
            static char const hex[] = "0123456789abcdefABCDEF";

            std::string uri;
            uri.reserve(uri_param.size());

            for (std::string::size_type n = 0; n < uri_param.size(); ++n) {
                if (static_cast<unsigned char>(uri_param[n]) > 127 ||
                    (!std::isalnum(static_cast<unsigned char>(uri_param[n])) &&
                     !std::strchr(mark, uri_param[n])) ||
                    (uri_param[n] == '%' &&
                     !(n + 2 < uri_param.size() &&
                       std::strchr(hex, uri_param[n + 1]) &&
                       std::strchr(hex, uri_param[n + 2])))) {
                    char escape[] = {'%', hex[uri_param[n] / 16],
                                     hex[uri_param[n] % 16], '\0'};
                    uri += escape;
                }
                else {
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
    }
}
