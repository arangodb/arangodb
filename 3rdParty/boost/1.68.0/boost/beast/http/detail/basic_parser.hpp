//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_DETAIL_BASIC_PARSER_HPP
#define BOOST_BEAST_HTTP_DETAIL_BASIC_PARSER_HPP

#include <boost/beast/core/static_string.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/detail/cpu_info.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/detail/rfc7230.hpp>
#include <boost/config.hpp>
#include <boost/version.hpp>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace boost {
namespace beast {
namespace http {
namespace detail {

struct basic_parser_base
{
    // limit on the size of the obs-fold buffer
    //
    // https://stackoverflow.com/questions/686217/maximum-on-http-header-values
    //
    static std::size_t constexpr max_obs_fold = 4096;

    enum class state
    {
        nothing_yet = 0,
        start_line,
        fields,
        body0,
        body,
        body_to_eof0,
        body_to_eof,
        chunk_header0,
        chunk_header,
        chunk_body,
        complete
    };

    static
    bool
    is_pathchar(char c)
    {
        // VFALCO This looks the same as the one below...

        // TEXT = <any OCTET except CTLs, and excluding LWS>
        static bool constexpr tab[256] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //  16
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  32
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  48
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  64
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  80
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  96
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // 112
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 128
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 144
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 160
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 176
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 192
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 208
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 224
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  // 240
        };
        return tab[static_cast<unsigned char>(c)];
    }

    static
    inline
    bool
    unhex(unsigned char& d, char c)
    {
        static signed char constexpr tab[256] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //   0
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  16
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  32
             0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1, //  48
            -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  64
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  80
            -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  96
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 112

            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 128
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 144
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 160
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 176
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 192
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 208
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 224
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 240
        };
        d = static_cast<unsigned char>(
            tab[static_cast<unsigned char>(c)]);
        return d != static_cast<unsigned char>(-1);
    }

    static
    bool
    is_digit(char c)
    {
        return static_cast<unsigned char>(c-'0') < 10;
    }

    static
    bool
    is_print(char c)
    {
        return static_cast<unsigned char>(c-32) < 95;
    }

    template<class FwdIt>
    static
    FwdIt
    trim_front(FwdIt it, FwdIt const& end)
    {
        while(it != end)
        {
            if(*it != ' ' && *it != '\t')
                break;
            ++it;
        }
        return it;
    }

    template<class RanIt>
    static
    RanIt
    trim_back(
        RanIt it, RanIt const& first)
    {
        while(it != first)
        {
            auto const c = it[-1];
            if(c != ' ' && c != '\t')
                break;
            --it;
        }
        return it;
    }

    static
    string_view
    make_string(char const* first, char const* last)
    {
        return {first, static_cast<
            std::size_t>(last - first)};
    }

    //--------------------------------------------------------------------------

    static
    std::pair<char const*, bool>
    find_fast(
        char const* buf,
        char const* buf_end,
        char const* ranges,
        size_t ranges_size)
    {
        bool found = false;
        boost::ignore_unused(buf_end, ranges, ranges_size);
        return {buf, found};
    }

    // VFALCO Can SIMD help this?
    static
    char const*
    find_eol(
        char const* it, char const* last,
            error_code& ec)
    {
        for(;;)
        {
            if(it == last)
            {
                ec.assign(0, ec.category());
                return nullptr;
            }
            if(*it == '\r')
            {
                if(++it == last)
                {
                    ec.assign(0, ec.category());
                    return nullptr;
                }
                if(*it != '\n')
                {
                    ec = error::bad_line_ending;
                    return nullptr;
                }
                ec.assign(0, ec.category());
                return ++it;
            }
            // VFALCO Should we handle the legacy case
            // for lines terminated with a single '\n'?
            ++it;
        }
    }

    static
    char const*
    find_eom(char const* p, char const* last)
    {
        for(;;)
        {
            if(p + 4 > last)
                return nullptr;
            if(p[3] != '\n')
            {
                if(p[3] == '\r')
                    ++p;
                else
                    p += 4;
            }
            else if(p[2] != '\r')
            {
                p += 4;
            }
            else if(p[1] != '\n')
            {
                p += 2;
            }
            else if(p[0] != '\r')
            {
                p += 2;
            }
            else
            {
                return p + 4;
            }
        }
    }

    //--------------------------------------------------------------------------

    static
    char const*
    parse_token_to_eol(
        char const* p,
        char const* last,
        char const*& token_last,
        error_code& ec)
    {
        for(;; ++p)
        {
            if(p >= last)
            {
                ec = error::need_more;
                return p;
            }
            if(BOOST_UNLIKELY(! is_print(*p)))
                if((BOOST_LIKELY(static_cast<
                        unsigned char>(*p) < '\040') &&
                    BOOST_LIKELY(*p != '\011')) ||
                    BOOST_UNLIKELY(*p == '\177'))
                    goto found_control;
        }
    found_control:
        if(BOOST_LIKELY(*p == '\r'))
        {
            if(++p >= last)
            {
                ec = error::need_more;
                return last;
            }
            if(*p++ != '\n')
            {
                ec = error::bad_line_ending;
                return last;
            }
            token_last = p - 2;
        }
    #if 0
        // VFALCO This allows `\n` by itself
        //        to terminate a line
        else if(*p == '\n')
        {
            token_last = p;
            ++p;
        }
    #endif
        else
        {
            // invalid character
            return nullptr;
        }
        return p;
    }

    template<class Iter, class T>
    static
    typename std::enable_if<
        std::numeric_limits<T>::is_integer &&
        ! std::numeric_limits<T>::is_signed, bool>::type
    parse_dec(Iter it, Iter last, T& v)
    {
        if(it == last)
            return false;
        T tmp = 0;
        do
        {
            if((! is_digit(*it)) ||
                tmp > (std::numeric_limits<T>::max)() / 10)
                return false;
            tmp *= 10;
            T const d = *it - '0';
            if((std::numeric_limits<T>::max)() - tmp < d)
                return false;
            tmp += d;
        }
        while(++it != last);
        v = tmp;
        return true;
    }

    template<class Iter, class Unsigned>
    static
    bool
    parse_hex(Iter& it, Unsigned& v)
    {
        unsigned char d;
        if(! unhex(d, *it))
            return false;
        v = d;
        for(;;)
        {
            if(! unhex(d, *++it))
                break;
            auto const v0 = v;
            v = 16 * v + d;
            if(v < v0)
                return false;
        }
        return true;
    }

    static
    bool
    parse_crlf(char const*& it)
    {
        if( it[0] != '\r' || it[1] != '\n')
            return false;
        it += 2;
        return true;
    }

    static
    void
    parse_method(
        char const*& it, char const* last,
        string_view& result, error_code& ec)
    {
        // parse token SP
        auto const first = it;
        for(;; ++it)
        {
            if(it + 1 > last)
            {
                ec = error::need_more;
                return;
            }
            if(! detail::is_token_char(*it))
                break;
        }
        if(it + 1 > last)
        {
            ec = error::need_more;
            return;
        }
        if(*it != ' ')
        {
            ec = error::bad_method;
            return;
        }
        if(it == first)
        {
            // cannot be empty
            ec = error::bad_method;
            return;
        }
        result = make_string(first, it++);
    }

    static
    void
    parse_target(
        char const*& it, char const* last,
        string_view& result, error_code& ec)
    {
        // parse target SP
        auto const first = it;
        for(;; ++it)
        {
            if(it + 1 > last)
            {
                ec = error::need_more;
                return;
            }
            if(! is_pathchar(*it))
                break;
        }
        if(it + 1 > last)
        {
            ec = error::need_more;
            return;
        }
        if(*it != ' ')
        {
            ec = error::bad_target;
            return;
        }
        if(it == first)
        {
            // cannot be empty
            ec = error::bad_target;
            return;
        }
        result = make_string(first, it++);
    }

    static
    void
    parse_version(
        char const*& it, char const* last,
        int& result, error_code& ec)
    {
        if(it + 8 > last)
        {
            ec = error::need_more;
            return;
        }
        if(*it++ != 'H')
        {
            ec = error::bad_version;
            return;
        }
        if(*it++ != 'T')
        {
            ec = error::bad_version;
            return;
        }
        if(*it++ != 'T')
        {
            ec = error::bad_version;
            return;
        }
        if(*it++ != 'P')
        {
            ec = error::bad_version;
            return;
        }
        if(*it++ != '/')
        {
            ec = error::bad_version;
            return;
        }
        if(! is_digit(*it))
        {
            ec = error::bad_version;
            return;
        }
        result = 10 * (*it++ - '0');
        if(*it++ != '.')
        {
            ec = error::bad_version;
            return;
        }
        if(! is_digit(*it))
        {
            ec = error::bad_version;
            return;
        }
        result += *it++ - '0';
    }

    static
    void
    parse_status(
        char const*& it, char const* last,
        unsigned short& result, error_code& ec)
    {
        // parse 3(digit) SP
        if(it + 4 > last)
        {
            ec = error::need_more;
            return;
        }
        if(! is_digit(*it))
        {
            ec = error::bad_status;
            return;
        }
        result = 100 * (*it++ - '0');
        if(! is_digit(*it))
        {
            ec = error::bad_status;
            return;
        }
        result += 10 * (*it++ - '0');
        if(! is_digit(*it))
        {
            ec = error::bad_status;
            return;
        }
        result += *it++ - '0';
        if(*it++ != ' ')
        {
            ec = error::bad_status;
            return;
        }
    }
    
    void
    parse_reason(
        char const*& it, char const* last,
        string_view& result, error_code& ec)
    {
        auto const first = it;
        char const* token_last = nullptr;
        auto p = parse_token_to_eol(
            it, last, token_last, ec);
        if(ec)
            return;
        if(! p)
        {
            ec = error::bad_reason;
            return;
        }
        result = make_string(first, token_last);
        it = p;
    }

    template<std::size_t N>
    void
    parse_field(
        char const*& p,
        char const* last,
        string_view& name,
        string_view& value,
        static_string<N>& buf,
        error_code& ec)
    {
    /*  header-field    = field-name ":" OWS field-value OWS

        field-name      = token
        field-value     = *( field-content / obs-fold )
        field-content   = field-vchar [ 1*( SP / HTAB ) field-vchar ]
        field-vchar     = VCHAR / obs-text

        obs-fold        = CRLF 1*( SP / HTAB )
                        ; obsolete line folding
                        ; see Section 3.2.4

        token           = 1*<any CHAR except CTLs or separators>
        CHAR            = <any US-ASCII character (octets 0 - 127)>
        sep             = "(" | ")" | "<" | ">" | "@"
                        | "," | ";" | ":" | "\" | <">
                        | "/" | "[" | "]" | "?" | "="
                        | "{" | "}" | SP | HT
    */
        static char const* is_token =
            "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            "\0\1\0\1\1\1\1\1\0\0\1\1\0\1\1\0\1\1\1\1\1\1\1\1\1\1\0\0\0\0\0\0"
            "\0\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\0\0\1\1"
            "\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\1\0\1\0"
            "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

        // name
        BOOST_ALIGNMENT(16) static const char ranges1[] =
            "\x00 "  /* control chars and up to SP */
            "\"\""   /* 0x22 */
            "()"     /* 0x28,0x29 */
            ",,"     /* 0x2c */
            "//"     /* 0x2f */
            ":@"     /* 0x3a-0x40 */
            "[]"     /* 0x5b-0x5d */
            "{\377"; /* 0x7b-0xff */
        auto first = p;
        bool found;
        std::tie(p, found) = find_fast(
            p, last, ranges1, sizeof(ranges1)-1);
        if(! found && p >= last)
        {
            ec = error::need_more;
            return;
        }
        for(;;)
        {
            if(*p == ':')
                break;
            if(! is_token[static_cast<
                unsigned char>(*p)])
            {
                ec = error::bad_field;
                return;
            }
            ++p;
            if(p >= last)
            {
                ec = error::need_more;
                return;
            }
        }
        if(p == first)
        {
            // empty name
            ec = error::bad_field;
            return;
        }
        name = make_string(first, p);
        ++p; // eat ':'
        char const* token_last = nullptr;
        for(;;)
        {
            // eat leading ' ' and '\t'
            for(;;++p)
            {
                if(p + 1 > last)
                {
                    ec = error::need_more;
                    return;
                }
                if(! (*p == ' ' || *p == '\t'))
                    break;
            }
            // parse to CRLF
            first = p;
            p = parse_token_to_eol(p, last, token_last, ec);
            if(ec)
                return;
            if(! p)
            {
                ec = error::bad_value;
                return;
            }
            // Look 1 char past the CRLF to handle obs-fold.
            if(p + 1 > last)
            {
                ec = error::need_more;
                return;
            }
            token_last =
                trim_back(token_last, first);
            if(*p != ' ' && *p != '\t')
            {
                value = make_string(first, token_last);
                return;
            }
            ++p;
            if(token_last != first)
                break;
        }
        buf.resize(0);
        buf.append(first, token_last);
        BOOST_ASSERT(! buf.empty());
        try
        {
            for(;;)
            {
                // eat leading ' ' and '\t'
                for(;;++p)
                {
                    if(p + 1 > last)
                    {
                        ec = error::need_more;
                        return;
                    }
                    if(! (*p == ' ' || *p == '\t'))
                        break;
                }
                // parse to CRLF
                first = p;
                p = parse_token_to_eol(p, last, token_last, ec);
                if(ec)
                    return;
                if(! p)
                {
                    ec = error::bad_value;
                    return;
                }
                // Look 1 char past the CRLF to handle obs-fold.
                if(p + 1 > last)
                {
                    ec = error::need_more;
                    return;
                }
                token_last = trim_back(token_last, first);
                if(first != token_last)
                {
                    buf.push_back(' ');
                    buf.append(first, token_last);
                }
                if(*p != ' ' && *p != '\t')
                {
                    value = {buf.data(), buf.size()};
                    return;
                }
                ++p;
            }
        }
        catch(std::length_error const&)
        {
            ec = error::header_limit;
            return;
        }
    }

    void
    parse_chunk_extensions(
        char const*& it,
        char const* last,
        error_code& ec)
    {
    /*
        chunk-ext       = *( BWS  ";" BWS chunk-ext-name [ BWS  "=" BWS chunk-ext-val ] )
        BWS             = *( SP / HTAB ) ; "Bad White Space"
        chunk-ext-name  = token
        chunk-ext-val   = token / quoted-string
        token           = 1*tchar
        quoted-string   = DQUOTE *( qdtext / quoted-pair ) DQUOTE
        qdtext          = HTAB / SP / "!" / %x23-5B ; '#'-'[' / %x5D-7E ; ']'-'~' / obs-text
        quoted-pair     = "\" ( HTAB / SP / VCHAR / obs-text )
        obs-text        = %x80-FF

        https://www.rfc-editor.org/errata_search.php?rfc=7230&eid=4667
    */
    loop:
        if(it == last)
        {
            ec = error::need_more;
            return;
        }
        if(*it != ' ' && *it != '\t' && *it != ';')
            return;
        // BWS
        if(*it == ' ' || *it == '\t')
        {
            for(;;)
            {
                ++it;
                if(it == last)
                {
                    ec = error::need_more;
                    return;
                }
                if(*it != ' ' && *it != '\t')
                    break;
            }
        }
        // ';'
        if(*it != ';')
        {
            ec = error::bad_chunk_extension;
            return;
        }
    semi:
        ++it; // skip ';'
        // BWS
        for(;;)
        {
            if(it == last)
            {
                ec = error::need_more;
                return;
            }
            if(*it != ' ' && *it != '\t')
                break;
            ++it;
        }
        // chunk-ext-name
        if(! detail::is_token_char(*it))
        {
            ec = error::bad_chunk_extension;
            return;
        }
        for(;;)
        {
            ++it;
            if(it == last)
            {
                ec = error::need_more;
                return;
            }
            if(! detail::is_token_char(*it))
                break;
        }
        // BWS [ ";" / "=" ]
        {
            bool bws;
            if(*it == ' ' || *it == '\t')
            {
                for(;;)
                {
                    ++it;
                    if(it == last)
                    {
                        ec = error::need_more;
                        return;
                    }
                    if(*it != ' ' && *it != '\t')
                        break;
                }
                bws = true;
            }
            else
            {
                bws = false;
            }
            if(*it == ';')
                goto semi;
            if(*it != '=')
            {
                if(bws)
                    ec = error::bad_chunk_extension;
                return;
            }
            ++it; // skip '='
        }
        // BWS
        for(;;)
        {
            if(it == last)
            {
                ec = error::need_more;
                return;
            }
            if(*it != ' ' && *it != '\t')
                break;
            ++it;
        }
        // chunk-ext-val
        if(*it != '"')
        {
            // token
            if(! detail::is_token_char(*it))
            {
                ec = error::bad_chunk_extension;
                return;
            }
            for(;;)
            {
                ++it;
                if(it == last)
                {
                    ec = error::need_more;
                    return;
                }
                if(! detail::is_token_char(*it))
                    break;
            }
        }
        else
        {
            // quoted-string
            for(;;)
            {
                ++it;
                if(it == last)
                {
                    ec = error::need_more;
                    return;
                }
                if(*it == '"')
                    break;
                if(*it == '\\')
                {
                    ++it;
                    if(it == last)
                    {
                        ec = error::need_more;
                        return;
                    }
                }
            }
            ++it;
        }
        goto loop;
    }
};

} // detail
} // http
} // beast
} // boost

#endif
