/*=============================================================================
    Copyright (c) 2001-2013 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_TEST_FEBRUARY_01_2007_0605PM)
#define BOOST_SPIRIT_TEST_FEBRUARY_01_2007_0605PM

#include <boost/spirit/home/x3/core/parse.hpp>
#include <boost/utility/string_view.hpp>
#include <iostream>

namespace spirit_test
{
    template <typename Char, typename Parser>
    bool test(Char const* in, Parser const& p, bool full_match = true)
    {
        Char const* last = in;
        while (*last)
            last++;
        return boost::spirit::x3::parse(in, last, p)
            && (!full_match || (in == last));
    }

    template <typename Char, typename Parser>
    bool test(boost::basic_string_view<Char, std::char_traits<Char>> in,
              Parser const& p, bool full_match = true)
    {
        auto const last = in.end();
        auto pos        = in.begin();

        return boost::spirit::x3::parse(pos, last, p) && (!full_match || (pos == last));
    }

    template <typename Char, typename Parser, typename Skipper>
    bool test(Char const* in, Parser const& p
        , Skipper const& s, bool full_match = true)
    {
        Char const* last = in;
        while (*last)
            last++;
        return boost::spirit::x3::phrase_parse(in, last, p, s)
            && (!full_match || (in == last));
    }

    template <typename Char, typename Parser>
    bool test_failure(Char const* in, Parser const& p)
    {
        Char const * const start = in;
        Char const* last = in;
        while (*last)
            last++;

        return !boost::spirit::x3::parse(in, last, p) && (in == start);
    }

    template <typename Char, typename Parser>
    bool test_failure(boost::basic_string_view<Char, std::char_traits<Char>> const in,
                      Parser const& p)
    {
        auto pos = in.begin();
        return !boost::spirit::x3::parse(pos, in.end(), p) && (pos == in.begin());
    }

    template <typename Char, typename Parser, typename Attr>
    bool test_attr(Char const* in, Parser const& p
        , Attr& attr, bool full_match = true)
    {
        Char const* last = in;
        while (*last)
            last++;
        return boost::spirit::x3::parse(in, last, p, attr)
            && (!full_match || (in == last));
    }

    template <typename Char, typename Parser, typename Attr, typename Skipper>
    bool test_attr(Char const* in, Parser const& p
        , Attr& attr, Skipper const& s, bool full_match = true)
    {
        Char const* last = in;
        while (*last)
            last++;
        return boost::spirit::x3::phrase_parse(in, last, p, s, attr)
            && (!full_match || (in == last));
    }

    template <typename Char, typename Parser>
    bool binary_test(Char const* in, std::size_t size, Parser const& p,
        bool full_match = true)
    {
        Char const* last = in + size;
        return boost::spirit::x3::parse(in, last, p)
            && (!full_match || (in == last));
    }

    template <typename Char, typename Parser, typename Skipper>
    bool binary_test(Char const* in, std::size_t size, Parser const& p,
        Skipper const& s, bool full_match = true)
    {
        Char const* last = in + size;
        return boost::spirit::x3::phrase_parse(in, last, p, s)
            && (!full_match || (in == last));
    }

    template <typename Char, typename Parser, typename Attr>
    bool binary_test_attr(Char const* in, std::size_t size, Parser const& p,
        Attr& attr, bool full_match = true)
    {
        Char const* last = in + size;
        return boost::spirit::x3::parse(in, last, p, attr)
            && (!full_match || (in == last));
    }

    template <typename Char, typename Parser, typename Attr, typename Skipper>
    bool binary_test_attr(Char const* in, std::size_t size, Parser const& p,
        Attr& attr, Skipper const& s, bool full_match = true)
    {
        Char const* last = in + size;
        return boost::spirit::x3::phrase_parse(in, last, p, s, attr)
            && (!full_match || (in == last));
    }


    template <typename... T>
    constexpr bool always_true(T&&...) { return true; }

    template <typename Parser>
    constexpr bool test_ctors(Parser const& p)
    {
        return always_true(
                   static_cast<Parser>(static_cast<Parser&&>(  // test move ctor
                       static_cast<Parser>(p))));              // test copy ctor
    }
}

# define BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(...) \
    static_assert(::spirit_test::test_ctors(__VA_ARGS__), "")

#endif
