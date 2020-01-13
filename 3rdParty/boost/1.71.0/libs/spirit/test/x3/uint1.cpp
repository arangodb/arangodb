/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser
    Copyright (c) 2011      Bryce Lelbach

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include "uint.hpp"
#include <boost/spirit/home/x3.hpp>

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    ///////////////////////////////////////////////////////////////////////////
    //  unsigned tests
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::x3::uint_;
        unsigned u;

        BOOST_TEST(test("123456", uint_));
        BOOST_TEST(test_attr("123456", uint_, u));
        BOOST_TEST(u == 123456);

        BOOST_TEST(test(max_unsigned, uint_));
        BOOST_TEST(test_attr(max_unsigned, uint_, u));
        BOOST_TEST(u == UINT_MAX);

        BOOST_TEST(!test(unsigned_overflow, uint_));
        BOOST_TEST(!test_attr(unsigned_overflow, uint_, u));
    }

    ///////////////////////////////////////////////////////////////////////////
    //  binary tests
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::x3::bin;
        unsigned u;

        BOOST_TEST(test("11111110", bin));
        BOOST_TEST(test_attr("11111110", bin, u));
        BOOST_TEST(u == 0xFE);

        BOOST_TEST(test(max_binary, bin));
        BOOST_TEST(test_attr(max_binary, bin, u));
        BOOST_TEST(u == UINT_MAX);

        BOOST_TEST(!test(binary_overflow, bin));
        BOOST_TEST(!test_attr(binary_overflow, bin, u));
    }

    ///////////////////////////////////////////////////////////////////////////
    //  octal tests
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::x3::oct;
        unsigned u;

        BOOST_TEST(test("12545674515", oct));
        BOOST_TEST(test_attr("12545674515", oct, u));
        BOOST_TEST(u == 012545674515);

        BOOST_TEST(test(max_octal, oct));
        BOOST_TEST(test_attr(max_octal, oct, u));
        BOOST_TEST(u == UINT_MAX);

        BOOST_TEST(!test(octal_overflow, oct));
        BOOST_TEST(!test_attr(octal_overflow, oct, u));
    }

    ///////////////////////////////////////////////////////////////////////////
    //  hex tests
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::x3::hex;
        unsigned u;

        BOOST_TEST(test("95BC8DF", hex));
        BOOST_TEST(test_attr("95BC8DF", hex, u));
        BOOST_TEST(u == 0x95BC8DF);

        BOOST_TEST(test("abcdef12", hex));
        BOOST_TEST(test_attr("abcdef12", hex, u));
        BOOST_TEST(u == 0xabcdef12);

        BOOST_TEST(test(max_hex, hex));
        BOOST_TEST(test_attr(max_hex, hex, u));
        BOOST_TEST(u == UINT_MAX);

        BOOST_TEST(!test(hex_overflow, hex));
        BOOST_TEST(!test_attr(hex_overflow, hex, u));
    }

    ///////////////////////////////////////////////////////////////////////////
    //  limited fieldwidth
    ///////////////////////////////////////////////////////////////////////////
    {
        unsigned u;
        using boost::spirit::x3::uint_parser;

        uint_parser<unsigned, 10, 1, 3> uint3;
        BOOST_TEST(test("123456", uint3, false));
        BOOST_TEST(test_attr("123456", uint3, u, false));
        BOOST_TEST(u == 123);

        uint_parser<unsigned, 10, 2, 4> uint4;
        BOOST_TEST(test("123456", uint4, false));
        BOOST_TEST(test_attr("123456", uint4, u, false));
        BOOST_TEST(u == 1234);

        char const * first = "0000000";
        char const * last  = first + std::strlen(first);
        uint_parser<unsigned, 10, 4, 4> uint_exact4;
        BOOST_TEST(boost::spirit::x3::parse(first, last, uint_exact4, u)
            && first != last && (last-first == 3) && u == 0);

        first = "0001400";
        last  = first + std::strlen(first);
        BOOST_TEST(boost::spirit::x3::parse(first, last, uint_exact4, u)
            && first != last && (last-first == 3) && u == 1);

        BOOST_TEST(!test("1", uint4));
        BOOST_TEST(!test_attr("1", uint4, u));
        BOOST_TEST(test_attr("014567", uint4, u, false) && u == 145);
    }

    ///////////////////////////////////////////////////////////////////////////
    //  action tests
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::x3::_attr;
        using boost::spirit::x3::uint_;
        using boost::spirit::x3::ascii::space;
        int n;

        auto f = [&](auto& ctx){ n = _attr(ctx); };

        BOOST_TEST(test("123", uint_[f]));
        BOOST_TEST(n == 123);
        BOOST_TEST(test("   456", uint_[f], space));
        BOOST_TEST(n == 456);
    }

    ///////////////////////////////////////////////////////////////////////////
    // Check overflow is parse error
    ///////////////////////////////////////////////////////////////////////////
    {
        boost::spirit::x3::uint_parser<boost::uint8_t> uint8_;
        boost::uint8_t u8;

        BOOST_TEST(!test_attr("999", uint8_, u8));
        BOOST_TEST(!test_attr("256", uint8_, u8));
        BOOST_TEST(test_attr("255", uint8_, u8));

        boost::spirit::x3::uint_parser<boost::uint16_t> uint16_;
        boost::uint16_t u16;

        BOOST_TEST(!test_attr("99999", uint16_, u16));
        BOOST_TEST(!test_attr("65536", uint16_, u16));
        BOOST_TEST(test_attr("65535", uint16_, u16));

        boost::spirit::x3::uint_parser<boost::uint32_t> uint32_;
        boost::uint32_t u32;

        BOOST_TEST(!test_attr("9999999999", uint32_, u32));
        BOOST_TEST(!test_attr("4294967296", uint32_, u32));
        BOOST_TEST(test_attr("4294967295", uint32_, u32));

        boost::spirit::x3::uint_parser<boost::int8_t> u_int8_;

        BOOST_TEST(!test_attr("999", u_int8_, u8));
        BOOST_TEST(!test_attr("-1", u_int8_, u8));
        BOOST_TEST(!test_attr("128", u_int8_, u8));
        BOOST_TEST(test_attr("127", u_int8_, u8));
        BOOST_TEST(test_attr("0", u_int8_, u8));

        boost::spirit::x3::uint_parser<boost::int16_t> u_int16_;

        BOOST_TEST(!test_attr("99999", u_int16_, u16));
        BOOST_TEST(!test_attr("-1", u_int16_, u16));
        BOOST_TEST(!test_attr("32768", u_int16_, u16));
        BOOST_TEST(test_attr("32767", u_int16_, u16));
        BOOST_TEST(test_attr("0", u_int16_, u16));

        boost::spirit::x3::uint_parser<boost::int32_t> u_int32_;

        BOOST_TEST(!test_attr("9999999999", u_int32_, u32));
        BOOST_TEST(!test_attr("-1", u_int32_, u32));
        BOOST_TEST(!test_attr("2147483648", u_int32_, u32));
        BOOST_TEST(test_attr("2147483647", u_int32_, u32));
        BOOST_TEST(test_attr("0", u_int32_, u32));
    }

    ///////////////////////////////////////////////////////////////////////////
    //  custom uint tests
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::x3::uint_;
        using boost::spirit::x3::uint_parser;
        custom_uint u;

        BOOST_TEST(test_attr("123456", uint_, u));
        uint_parser<custom_uint, 10, 1, 2> uint2;
        BOOST_TEST(test_attr("12", uint2, u));
    }

    return boost::report_errors();
}
