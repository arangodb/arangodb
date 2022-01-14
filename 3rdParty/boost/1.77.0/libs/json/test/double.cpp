//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json/stream_parser.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <iostream>
#include <random>
#include <cinttypes>

#include "parse-vectors.hpp"
#include "test.hpp"
#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

template<std::size_t N, class... Args>
void
sprintf(char (&buf)[N],
    char const* format, Args&&... args)
{
#ifdef _MSC_VER
    sprintf_s(buf, format,
        std::forward<Args>(args)...);
#else
    std::sprintf(buf, format,
        std::forward<Args>(args)...);
#endif
}

class double_test
{
public:
    struct f_boost
    {
        static
        string_view
        name() noexcept
        {
            return "boost";
        }

        double
        operator()(string_view s) const
        {
            BOOST_TEST_CHECKPOINT();
            error_code ec;
            stream_parser p;
            p.write(s.data(), s.size(), ec);
            if(BOOST_TEST(! ec))
                p.finish(ec);
            if(! BOOST_TEST(! ec))
                return 0;
            auto const jv = p.release();
            double const d = jv.as_double();
            grind_double(s, d);
            return d;
        }
    };

    bool
    within_1ulp(double x, double y)
    {
        std::uint64_t bx, by;
        std::memcpy(&bx, &x, sizeof(x));
        std::memcpy(&by, &y, sizeof(y));

        auto diff = bx - by;
        switch (diff)
        {
        case 0:
        case 1:
        case 0xffffffffffffffff:
            return true;
        default:
            break;
        }
        return false;
    }

    static
    value
    from_string_test(
        string_view s,
        storage_ptr sp = {},
        const parse_options& po = parse_options())
    {
        stream_parser p(storage_ptr(), po);
        error_code ec;
        p.reset(std::move(sp));
        p.write(s.data(), s.size(), ec);
        if(BOOST_TEST(! ec))
            p.finish(ec);
        BOOST_TEST(! ec);
        return p.release();
    }

    void
    static
    check_round_trip(value const& jv1,
        const parse_options& po = parse_options())
    {
        auto const s2 =
            //to_string_test(jv1); // use this if serializer is broken
            serialize(jv1);
        auto jv2 =
            from_string_test(s2, {}, po);
        BOOST_TEST(equal(jv1, jv2));
    }

    template<class F>
    void
    static
    grind_one(
        string_view s,
        storage_ptr sp,
        F const& f,
        const parse_options& po = parse_options())
    {
        auto const jv =
            from_string_test(s, sp, po);
        f(jv, po);
    }

    static
    void
    grind_one(string_view s)
    {
        auto const jv =
            from_string_test(s);
        check_round_trip(jv);
    }

    template<class F>
    static
    void
    grind(string_view s, F const& f,
        const parse_options& po = parse_options())
    {
        try
        {
            grind_one(s, {}, f, po);

            fail_loop([&](storage_ptr const& sp)
            {
                grind_one(s, sp, f, po);
            });

            if(s.size() > 1)
            {
                // Destroy the stream_parser at every
                // split point to check leaks.
                for(std::size_t i = 1;
                    i < s.size(); ++i)
                {
                    fail_resource mr;
                    mr.fail_max = 0;
                    stream_parser p(storage_ptr(), po);
                    error_code ec;
                    p.reset(&mr);
                    p.write(s.data(), i, ec);
                    if(BOOST_TEST(! ec))
                        p.write(
                            s.data() + i,
                            s.size() - i, ec);
                    if(BOOST_TEST(! ec))
                        p.finish(ec);
                    if(BOOST_TEST(! ec))
                        f(p.release(), po);
                }
            }
        }
        catch(std::exception const&)
        {
            BOOST_TEST_FAIL();
        }
    }

    static
    void
    grind(string_view s,
        const parse_options& po = parse_options())
    {
        grind(s,
            [](value const& jv, const parse_options& po)
            {
                check_round_trip(jv, po);
            }, po);
    }

    static
    void
    grind_double(string_view s, double v)
    {
        grind(s,
            [v](value const& jv, const parse_options&)
            {
                if(! BOOST_TEST(jv.is_double()))
                    return;
                BOOST_TEST(jv.get_double() == v);
            });
    }

    // Verify that f converts to the
    // same double produced by `strtod`.
    // Requires `s` is not represented by an integral type.
    template<class F>
    void
    fc(std::string const& s, F const& f)
    {
        char* str_end;
        double const need =
            std::strtod(s.c_str(), &str_end);
        // BOOST_TEST(str_end == &s.back() + 1);
        double const got = f(s);
        auto same = got == need;
        auto close = same ?
            true : within_1ulp(got, need);

        if( !BOOST_TEST(close) )
        {
            std::cerr << "Failure on '" << s << "': " << got << " != " << need << "\n";
        }
    }

    void
    fc(std::string const& s)
    {
        fc(s, f_boost{});
        fc(s + std::string( 64, ' ' ), f_boost{});
    }

    void
    testDouble()
    {
        grind_double("-1.010", -1.01);
        grind_double("-0.010", -0.01);
        grind_double("-0.0", -0.0);
        grind_double("-0e0", -0.0);
        grind_double( "18.4",  18.4);
        grind_double("-18.4", -18.4);
        grind_double( "18446744073709551616",  1.8446744073709552e+19);
        grind_double("-18446744073709551616", -1.8446744073709552e+19);
        grind_double( "18446744073709551616.0",  1.8446744073709552e+19);
        grind_double( "18446744073709551616.00009",  1.8446744073709552e+19);
        grind_double( "1844674407370955161600000",  1.8446744073709552e+24);
        grind_double("-1844674407370955161600000", -1.8446744073709552e+24);
        grind_double( "1844674407370955161600000.0",  1.8446744073709552e+24);
        grind_double( "1844674407370955161600000.00009",  1.8446744073709552e+24);
        grind_double( "19700720435664.186294290058937593e13",  1.9700720435664185e+26);

        grind_double( "1.0", 1.0);
        grind_double( "1.1", 1.1);
        grind_double( "1.11", 1.11);
        grind_double( "1.11111", 1.11111);
        grind_double( "11.1111", 11.1111);
        grind_double( "111.111", 111.111);

        fc("-0.9999999999999999999999");
        fc("-0.9999999999999999");
        fc("-0.9007199254740991");
        fc("-0.999999999999999");
        fc("-0.99999999999999");
        fc("-0.9999999999999");
        fc("-0.999999999999");
        fc("-0.99999999999");
        fc("-0.9999999999");
        fc("-0.999999999");
        fc("-0.99999999");
        fc("-0.9999999");
        fc("-0.999999");
        fc("-0.99999");
        fc("-0.9999");
        fc("-0.8125");
        fc("-0.999");
        fc("-0.99");
        fc("-1.0");
        fc("-0.9");
        fc("-0.0");
        fc("0.0");
        fc("0.9");
        fc("0.99");
        fc("0.999");
        fc("0.8125");
        fc("0.9999");
        fc("0.99999");
        fc("0.999999");
        fc("0.9999999");
        fc("0.99999999");
        fc("0.999999999");
        fc("0.9999999999");
        fc("0.99999999999");
        fc("0.999999999999");
        fc("0.9999999999999");
        fc("0.99999999999999");
        fc("0.999999999999999");
        fc("0.9007199254740991");
        fc("0.9999999999999999");
        fc("0.9999999999999999999999");
        fc("0.999999999999999999999999999");

        fc("-1e308");
        fc("-1e-308");
        fc("-9999e300");
        fc("-999e100");
        fc("-99e10");
        fc("-9e1");
        fc("9e1");
        fc("99e10");
        fc("999e100");
        fc("9999e300");
        fc("999999999999999999.0");
        fc("999999999999999999999.0");
        fc("999999999999999999999e5");
        fc("999999999999999999999.0e5");

        fc("0.00000000000000001");

        fc("-1e-1");
        fc("-1e0");
        fc("-1e1");
        fc("0e0");
        fc("1e0");
        fc("1e10");

        fc("0."
           "00000000000000000000000000000000000000000000000000" // 50 zeroes
           "1e50");
        fc("-0."
           "00000000000000000000000000000000000000000000000000" // 50 zeroes
           "1e50");

        fc("0."
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000" // 500 zeroes
           "1e600");
        fc("-0."
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000" // 500 zeroes
           "1e600");

        fc("0e"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000"
           "00000000000000000000000000000000000000000000000000" // 500 zeroes
        );
    }

    void checkAccuracy(const char* nm, int max_ulp)
    {
        double x = std::strtod( nm, 0 );
        double y = boost::json::parse( nm ).as_double();
        std::uint64_t bx, by;
        std::memcpy( &bx, &x, sizeof(x) );
        std::memcpy( &by, &y, sizeof(y) );
        std::int64_t diff = bx - by;
        if (!BOOST_TEST(std::abs( diff ) <= max_ulp))
            std::fprintf(stderr,
                         "%s: difference %" PRId64 " ulp\n"
                         "  strtod:       %.13a %.16g\n"
                         "  boost.json:   %.13a %.16g\n\n",
                         nm, diff, x, x, y, y );
    }

    void
    testWithinULP()
    {
        std::mt19937_64 rng;

        checkAccuracy("10199214983525025199.13135016100190689227e-308", 2);

        for( int i = 0; i < 1000000; ++i )
        {
            unsigned long long x1 = rng();
            unsigned long long x2 = rng();
            int x3 = std::uniform_int_distribution<>( -308, +308 )( rng );

            char buffer[ 128 ];
            sprintf( buffer, "%llu.%llue%d", x1, x2, x3 );

            checkAccuracy( buffer, 2 );
        }

        for( int i = -326; i <= +309; ++i )
        {
            char buffer[ 128 ];
            sprintf( buffer, "1e%d", i );

            checkAccuracy( buffer, 0 );
        }
    };

    void
    run()
    {
        testDouble();
        testWithinULP();
    }
};

TEST_SUITE(double_test, "boost.json.double");

BOOST_JSON_NS_END
