//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/basic_parser_impl.hpp>

#include <memory>
#include <string>
#include <utility>

#include "parse-vectors.hpp"
#include "test.hpp"
#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

BOOST_STATIC_ASSERT( std::is_nothrow_destructible<basic_parser<int>>::value );

namespace base64 {

std::size_t constexpr
decoded_size(std::size_t n)
{
    return n / 4 * 3; // requires n&3==0, smaller
}

signed char const*
get_inverse()
{
    static signed char constexpr tab[] = {
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //   0-15
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //  16-31
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, //  32-47
         52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, //  48-63
         -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, //  64-79
         15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, //  80-95
         -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, //  96-111
         41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, // 112-127
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 128-143
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 144-159
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 160-175
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 176-191
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 192-207
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 208-223
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 224-239
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  // 240-255
    };
    return &tab[0];
}

std::pair<std::size_t, std::size_t>
decode(void* dest, char const* src, std::size_t len)
{
    char* out = static_cast<char*>(dest);
    auto in = reinterpret_cast<unsigned char const*>(src);
    unsigned char c3[3], c4[4];
    int i = 0;
    int j = 0;

    auto const inverse = base64::get_inverse();

    while(len-- && *in != '=')
    {
        auto const v = inverse[*in];
        if(v == -1)
            break;
        ++in;
        c4[i] = v;
        if(++i == 4)
        {
            c3[0] =  (c4[0]        << 2) + ((c4[1] & 0x30) >> 4);
            c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
            c3[2] = ((c4[2] & 0x3) << 6) +   c4[3];

            for(i = 0; i < 3; i++)
                *out++ = c3[i];
            i = 0;
        }
    }

    if(i)
    {
        c3[0] = ( c4[0]        << 2) + ((c4[1] & 0x30) >> 4);
        c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
        c3[2] = ((c4[2] & 0x3) << 6) +   c4[3];

        for(j = 0; j < i - 1; j++)
            *out++ = c3[j];
    }

    return {out - static_cast<char*>(dest),
        in - reinterpret_cast<unsigned char const*>(src)};
}

} // base64

namespace {

bool
validate( string_view s )
{
    // Parse with the null parser and return false on error
    null_parser p;
    error_code ec;
    p.write( s.data(), s.size(), ec );
    if( ec )
        return false;

    // The string is valid JSON.
    return true;
}

parse_options
make_options(
    bool comments,
    bool commas,
    bool utf8)
{
    parse_options opt;
    opt.allow_comments = comments;
    opt.allow_trailing_commas = commas;
    opt.allow_invalid_utf8 = utf8;
    return opt;
}

} // (anon)

class basic_parser_test
{
public:
    ::test_suite::log_type log;

    void
    grind_one(
        string_view s,
        bool good,
        parse_options po)
    {
        error_code ec;
        fail_parser p(po);
        p.write(false,
            s.data(), s.size(), ec);
        BOOST_TEST((good && !ec) ||
            (! good && ec));
    }

    void
    grind_one(
        string_view s,
        bool good,
        const std::vector<parse_options>& configs)
    {
        for (const parse_options& po : configs)
            grind_one(s, good, po);
    }

    void
    grind(
        string_view s,
        bool good,
        parse_options po)
    {
        grind_one(s, good, po);

        // split/errors matrix
        if(! s.empty())
        {
            for(std::size_t i = 1;
                i < s.size(); ++i)
            {
                for(std::size_t j = 1;;++j)
                {
                    error_code ec;
                    fail_parser p(j, po);
                    p.write(true, s.data(), i, ec);
                    if(ec == error::test_failure)
                        continue;
                    if(! ec)
                    {
                        p.write(false, s.data() + i,
                            s.size() - i, ec);
                        if(ec == error::test_failure)
                            continue;
                    }
                    BOOST_TEST((good && !ec) || (
                        ! good && ec));
                    break;
                }
            }
        }

        // split/exceptions matrix
        if(! s.empty())
        {
            for(std::size_t i = 1;
                i < s.size(); ++i)
            {
                for(std::size_t j = 1;;++j)
                {
                    error_code ec;
                    throw_parser p(j, po);
                    try
                    {
                        p.write(
                            true, s.data(), i, ec);
                        if(! ec)
                            p.write(
                                false, s.data() + i,
                                    s.size() - i, ec);
                        BOOST_TEST((good && !ec) || (
                            ! good && ec));
                        break;
                    }
                    catch(test_exception const&)
                    {
                        continue;
                    }
                    catch(std::exception const& e)
                    {
                        BOOST_TEST_FAIL();
                        log << "  " <<
                            e.what() << std::endl;
                    }
                }
            }
        }
    }

    void
    grind(
        string_view s,
        bool good,
        const std::vector<parse_options>& configs)
    {
        for (const parse_options& po : configs)
            grind(s, good, po);
    }

    void
    bad(string_view s)
    {
        grind(s, false, parse_options());
    }

    void
    good(string_view s)
    {
        grind(s, true, parse_options());
    }

    void
    bad(
        string_view s,
        const parse_options& po)
    {
        grind(s, false, po);
    }

    void
    good(
        string_view s,
        const parse_options& po)
    {
        grind(s, true, po);
    }

    void
    bad_one(
        string_view s,
        const parse_options& po)
    {
        grind_one(s, false, po);
    }

    void
    good_one(
        string_view s,
        const parse_options& po)
    {
        grind_one(s, true, po);
    }

    void
    bad_one(string_view s)
    {
        grind_one(s, false, parse_options());
    }

    void
    good_one(string_view s)
    {
        grind_one(s, true, parse_options());
    }

    //------------------------------------------------------

    void
    testNull()
    {
        good("null");
        good(" null");
        good("null ");
        good("\tnull");
        good("null\t");
        good("\r\n\t null\r\n\t ");

        bad ("n     ");
        bad ("nu    ");
        bad ("nul   ");
        bad ("n---  ");
        bad ("nu--  ");
        bad ("nul-  ");

        bad ("NULL");
        bad ("Null");
        bad ("nulls");
    }

    void
    testBoolean()
    {
        good("true");
        good(" true");
        good("true ");
        good("\ttrue");
        good("true\t");
        good("\r\n\t true\r\n\t ");

        bad ("t     ");
        bad ("tr    ");
        bad ("tru   ");
        bad ("t---  ");
        bad ("tr--  ");
        bad ("tru-  ");
        bad ("TRUE");
        bad ("True");
        bad ("truer");

        good("false");
        good(" false");
        good("false ");
        good("\tfalse");
        good("false\t");
        good("\r\n\t false\r\n\t ");

        bad ("f     ");
        bad ("fa    ");
        bad ("fal   ");
        bad ("fals  ");
        bad ("f---- ");
        bad ("fa--- ");
        bad ("fal-- ");
        bad ("fals- ");
        bad ("FALSE");
        bad ("False");
        bad ("falser");
    }

    void
    testString()
    {
        good(R"jv( "x"   )jv");
        good(R"jv( "xy"  )jv");
        good(R"jv( "x y" )jv");

        // escapes
        good(R"jv(" \" ")jv");
        good(R"jv(" \\ ")jv");
        good(R"jv(" \/ ")jv");
        good(R"jv(" \b ")jv");
        good(R"jv(" \f ")jv");
        good(R"jv(" \n ")jv");
        good(R"jv(" \r ")jv");
        good(R"jv(" \t ")jv");

        // utf-16 escapes
        good(R"jv( " \u0000 "       )jv");
        good(R"jv( " \ud7ff "       )jv");
        good(R"jv( " \ue000 "       )jv");
        good(R"jv( " \uffff "       )jv");
        good(R"jv( " \ud800\udc00 " )jv");
        good(R"jv( " \udbff\udfff " )jv");
        good(R"jv( " \n\u0000     " )jv");

        // escape in key
        good(R"jv( {" \n":null} )jv");

        // incomplete
        bad ("\"");

        // illegal control character
        bad ({ "\"" "\x00" "\"", 3 });
        bad ("\"" "\x1f" "\"");
        bad ("\"" "\\n" "\x1f" "\"");

        // incomplete escape
        bad (R"jv( "\" )jv");

        // invalid escape
        bad (R"jv( "\z" )jv");

        // utf-16 escape, fast path,
        // invalid surrogate
        bad (R"jv( " \u----       " )jv");
        bad (R"jv( " \ud---       " )jv");
        bad (R"jv( " \ud8--       " )jv");
        bad (R"jv( " \ud80-       " )jv");
        // invalid low surrogate
        bad (R"jv( " \ud800------ " )jv");
        bad (R"jv( " \ud800\----- " )jv");
        bad (R"jv( " \ud800\u---- " )jv");
        bad (R"jv( " \ud800\ud--- " )jv");
        bad (R"jv( " \ud800\udc-- " )jv");
        bad (R"jv( " \ud800\udc0- " )jv");
        // illegal leading surrogate
        bad (R"jv( " \udc00       " )jv");
        bad (R"jv( " \udfff       " )jv");
        // illegal trailing surrogate
        bad (R"jv( " \ud800\udbff " )jv");
        bad (R"jv( " \ud800\ue000 " )jv");
    }

    void
    testNumber()
    {
        good("0");
        good("0                                ");
        good("0e0                              ");
        good("0E0                              ");
        good("0e00                             ");
        good("0E01                             ");
        good("0e+0                             ");
        good("0e-0                             ");
        good("0.0                              ");
        good("0.01                             ");
        good("0.0e0                            ");
        good("0.01e+0                          ");
        good("0.02E-0                          ");
        good("1                                ");
        good("12                               ");
        good("1e0                              ");
        good("1E0                              ");
        good("1e00                             ");
        good("1E01                             ");
        good("1e+0                             ");
        good("1e-0                             ");
        good("1.0                              ");
        good("1.01                             ");
        good("1.0e0                            ");
        good("1.01e+0                          ");
        good("1.02E-0                          ");
        good("1.0");

        good("-0                               ");
        good("-0e0                             ");
        good("-0E0                             ");
        good("-0e00                            ");
        good("-0E01                            ");
        good("-0e+0                            ");
        good("-0e-0                            ");
        good("-0.0                             ");
        good("-0.01                            ");
        good("-0.0e0                           ");
        good("-0.01e+0                         ");
        good("-0.02E-0                         ");
        good("-1                               ");
        good("-12                              ");
        good("-1                               ");
        good("-1e0                             ");
        good("-1E0                             ");
        good("-1e00                            ");
        good("-1E01                            ");
        good("-1e+0                            ");
        good("-1e-0                            ");
        good("-1.0                             ");
        good("-1.01                            ");
        good("-1.0e0                           ");
        good("-1.01e+0                         ");
        good("-1.02E-0                         ");
        good("-1.0");

        good("1.1e309                          ");
        good("9223372036854775807              ");
        good("-9223372036854775807             ");
        good("18446744073709551615             ");
        good("-18446744073709551615            ");

        good("1234567890123456");
        good("-1234567890123456");
        good("10000000000000000000000000");

        good("0.900719925474099178             ");

        // non-significant digits
        good("1000000000000000000000000        ");
        good("1000000000000000000000000e1      ");
        good("1000000000000000000000000.0      ");
        good("1000000000000000000000000.00     ");
        good("1000000000000000000000000.000000000001");
        good("1000000000000000000000000.0e1    ");
        good("1000000000000000000000000.0      ");

        good("1000000000.1000000000            ");

        bad("");
        bad("-                                 ");
        bad("00                                ");
        bad("01                                ");
        bad("00.                               ");
        bad("00.0                              ");
        bad("-00                               ");
        bad("-01                               ");
        bad("-00.                              ");
        bad("-00.0                             ");
        bad("1a                                ");
        bad("-a                                ");
        bad(".                                 ");
        bad("1.                                ");
        bad("1+                                ");
        bad("0.0+                              ");
        bad("0.0e+                             ");
        bad("0.0e-                             ");
        bad("0.0e0-                            ");
        bad("0.0e                              ");
        bad("1eX                               ");
        bad("1000000000000000000000000.e       ");
        bad("0.");
        bad("0.0e+");
        bad("0.0e2147483648");
    }

    void
    testArray()
    {
        good("[]");
        good("[ ]");
        good("[ \t ]");
        good("[ \"\" ]");
        good("[ \" \" ]");
        good("[ \"x\" ]");
        good("[ \"x\", \"y\" ]");
        good("[1,2,3]");
        good(" [1,2,3]");
        good("[1,2,3] ");
        good(" [1,2,3] ");
        good("[1,2,3]");
        good("[ 1,2,3]");
        good("[1 ,2,3]");
        good("[1, 2,3]");
        good("[1,2 ,3]");
        good("[1,2, 3]");
        good("[1,2,3 ]");
        good(" [  1 , 2 \t\n ,  \n3]");

        bad ("[");
        bad (" [");
        bad (" []]");
        bad ("[{]");
        bad (R"jv( [ null ; 1 ] )jv");
    }

    void
    testObject()
    {
        good("{}");
        good("{ }");
        good("{ \t }");
        good("{\"x\":null}");
        good("{ \"x\":null}");
        good("{\"x\" :null}");
        good("{\"x\": null}");
        good("{\"x\":null }");
        good("{ \"x\" : null }");
        good("{ \"x\" : {} }");
        good("{ \"x\" : [] }");
        good("{ \"x\" : { \"y\" : null } }");
        good("{ \"x\" : [{}] }");
        good("{\"x\\ny\\u0022\":null}");
        good("{ \"x\":1, \"y\":null}");
        good("{\"x\":1,\"y\":2,\"z\":3}");
        good(" {\"x\":1,\"y\":2,\"z\":3}");
        good("{\"x\":1,\"y\":2,\"z\":3} ");
        good(" {\"x\":1,\"y\":2,\"z\":3} ");
        good("{ \"x\":1,\"y\":2,\"z\":3}");
        good("{\"x\" :1,\"y\":2,\"z\":3}");
        good("{\"x\":1 ,\"y\":2,\"z\":3}");
        good("{\"x\":1,\"y\" :2,\"z\":3}");
        good("{\"x\":1,\"y\": 2,\"z\":3}");
        good("{\"x\":1,\"y\":2 ,\"z\":3}");
        good("{\"x\":1,\"y\":2, \"z\":3}");
        good("{\"x\":1,\"y\":2, \"z\" :3}");
        good("{\"x\":1,\"y\":2, \"z\": 3}");
        good("{\"x\":1,\"y\":2, \"z\":3 }");
        good(" \t { \"x\" \n  :   1, \"y\" :2, \"z\" : 3} \n");

        good("[{\"x\":[{\"y\":null}]}]");

        bad ("{");
        bad (" {");
        bad (" {}}");
        bad ("{{}}");
        bad ("{[]}");

        bad (R"jv( {"x";null} )jv");
        bad (R"jv( {"x":null . "y":0} )jv");
    }

    void
    testParser()
    {
        auto const check =
        [this]( string_view s,
            bool done)
        {
            fail_parser p;
            error_code ec;
            p.write_some(
                true,
                s.data(), s.size(),
                ec);
            if(! BOOST_TEST(! ec))
            {
                log << "    failed to parse: " << s << '\n';
                return;
            }
            BOOST_TEST(done == p.done());
        };

        // done()

        check("{}", true);
        check("{} ", true);
        check("{}x", true);
        check("{} x", true);

        check("[]", true);
        check("[] ", true);
        check("[]x", true);
        check("[] x", true);

        check("\"a\"", true);
        check("\"a\" ", true);
        check("\"a\"x", true);
        check("\"a\" x", true);

        check("0", false);
        check("0 ", true);
        check("0x", true);
        check("0 x", true);
        check("0.", false);
        check("0.0", false);
        check("0.0 ", true);
        check("0.0 x", true);

        check("true", true);
        check("true ", true);
        check("truex", true);
        check("true x", true);

        check("false", true);
        check("false ", true);
        check("falsex", true);
        check("false x", true);

        check("null", true);
        check("null ", true);
        check("nullx", true);
        check("null x", true);

        // flush
        {
            {
                for(auto esc :
                    { "\\\"", "\\\\", "\\/", "\\b",
                      "\\f", "\\n", "\\r", "\\t", "\\u0000"
                    })
                {
                    std::string const big =
                        "\\\"" + std::string(
                        BOOST_JSON_STACK_BUFFER_SIZE-4, '*') + esc;
                    std::string const s =
                        "{\"" + big + "\":\"" + big + "\"}";
                    good_one(s);
                }
            }
            {
                std::string big;
                big = "\\\"" +
                    std::string(
                        BOOST_JSON_STACK_BUFFER_SIZE+ 1, '*');
                std::string s;
                s = "{\"" + big + "\":\"" + big + "\"}";
                good_one(s);
            }
        }

        // no input
        {
            error_code ec;
            fail_parser p;
            p.write(false, nullptr, 0, ec);
            BOOST_TEST(ec);
        }
    }

    void
    testMembers()
    {
        fail_parser p;
        std::size_t n;
        error_code ec;
        n = p.write_some(true, "null", 4, ec );
        if(BOOST_TEST(! ec))
        {
            BOOST_TEST(n == 4);
            BOOST_TEST(p.done());
            n = p.write_some(false, " \t42", 4, ec);
            BOOST_TEST(n == 2);
            BOOST_TEST(! ec);
        }
        p.reset();
        n = p.write_some(false, "[1,2,3]", 7, ec);
        if(BOOST_TEST(! ec))
        {
            BOOST_TEST(n == 7);
            BOOST_TEST(p.done());
        }
    }

    void
    testParseVectors()
    {
        std::vector<parse_options> all_configs =
        {
            make_options(false, false, true),
            make_options(true, false, true),
            make_options(false, true, true),
            make_options(true, true, true),
            make_options(false, false, false),
            make_options(true, false, false),
            make_options(false, true, false),
            make_options(true, true, false)
        };
        parse_vectors pv;
        for(auto const& v : pv)
        {
            // skip these , because basic_parser
            // doesn't have a max_depth setting.
            if( v.name == "structure_100000_opening_arrays" ||
                v.name == "structure_open_array_object")
            {
                continue;
            }
            for (const parse_options& po : all_configs)
            {
                if(v.result == 'i')
                {
                    error_code ec;
                    fail_parser p(po);
                    p.write(
                        false,
                        v.text.data(),
                        v.text.size(),
                        ec);
                    if(! ec)
                        good_one(v.text, po);
                    else
                        bad_one(v.text, po);
                }
                else if(v.result == 'y')
                    good_one(v.text, po);
                else
                    bad_one(v.text, po);
            }
        }
    }

    // https://github.com/boostorg/json/issues/13
    void
    testIssue13()
    {
        validate("\"~QQ36644632   {n");
    }

    void
    testIssue20()
    {
        string_view s =
            "WyL//34zOVx1ZDg0ZFx1ZGM4M2RcdWQ4M2RcdWRlM2M4dWRlMTlcdWQ4M2RcdWRlMzlkZWUzOVx1"
            "ZDg0ZFx1ZGM4M2RcdWQ4M2RcdWRlMzlcXHVkY2M4M1x1ZDg5ZFx1ZGUzOVx1ZDgzZFx1ZGUzOWRb"
            "IGZhbHNlLDMzMzMzMzMzMzMzMzMzMzMzNDMzMzMzMTY1MzczNzMwLDMzMzMzMzMzMzMzMzMzMzMz"
            "MzM3ODAsMzMzMzMzMzMzMzM0MzMzMzMxNjUzNzM3MzAsMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMz"
            "MzM3ODAsMzMzMzMzMzMzMzMzMzQzMzMzMzE2NTM3MzczMCwzMzMzMzMzMzMzMzMzMzMzMzMzNzgw"
            "LDMzMzMzMzM4MzU1MzMwNzQ3NDYwLDMzMTY2NTAwMDAzMzMzMzMwNzQ3MzMzMzMzMzc3OSwzMzMz"
            "MzMzMzMzMzMzMzMzNDMzMzMzMzMwNzQ3NDYwLDMzMzMzMzMzMzMzMzMzMzMzMzMzNzgwLDMzMzMz"
            "MzMzMzMzMzMzMzA4ODM1NTMzMDc0Mzc4MCwzMzMzMzMzMzMzMzMzMzMwODgzNTUzMzA3NDc0NjAs"
            "MzMzMzMzMzMxNjY1MDAwMDMzMzMzNDc0NjAsMzMzMzMzMzMzMzMzMzMzMzMzMzc4MCwzMzMzMzMz"
            "MzMzMzM3MzMzMzE2NjUwMDAwMzMzMzMzMDc0NzMzMzMzMzM3NzksMzMzMzMzMzMzMzMzMzMzMzQz"
            "MzMzMzMwNzQ3NDYwLDMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzc4MCwzMzMzMzMzMzMzNzgw"
            "LDMzMzMzMzMzMzMzMzA4ODM1NTMzMDc0NzQ2MCwzMzE2NjUwMDAwMzMzMzMzMDc0NzMzMzMzMzM3"
            "NzksMzMzMzMzMzMzMzMzMzMzMzQzMzMzMzMwNzQ3NDYwLDMzMzMzMzMzMzMzMzMzMzMzMzMzNzgw"
            "LDMzMzMzMzMzMzMzMzMzMzA4ODM1NTMzMDc0Mzc4MCwzMzMzMzMzMzMzMzMzMzMzMzMwODgzNTUz"
            "MzA3NDM3ODAsMzMzMzMzMzMzMzMzMzMzMDg4MzU1MzMwNzQ3NDYwLDMzMzMzMzMzMzMzMDczMzM3"
            "NDc0NjAsMzMzMzMzMzMzMzMzMzMzMzMzNzgwLDMzMzMzMzMzMzMzMzA4ODM1NTMzMDc0NzQ2MCwz"
            "MzE2NjUwMDAwMzMzMzMzMDc0NzMzMzMzMzM3NzksMzMzMzMzMzMzMzMzMzMzMzQzMzMzMzMzMDc0"
            "NzQ2MCwzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzM3ODAsMzMzMzMzMzMzMzMzMzMzMDg4"
            "MzU1MzMwNzQzNzgwLDMzMzMzMzMzMzMzMzMzMzA4ODM1NTMzMDc0NzQ2MCwzMzMzMzMzMzMzMzMz"
            "MzMzMzM0MjQ3LDMzMzMzMzMzMzMzMzMzMzQzMzMzMzMzMzMzMzMzMzM3MzMzMzQzMzMzMzMzMDc0"
            "NzQ2MCwzMzMzMzMzMzMzMzMzMzMzMzMzNzgwLDMzMzMzMzMzMzMzMzA4ODM1NTMzMDc0NzQ2MCwz"
            "MzE2NjUwMDAwMzMzMzMzMDc0NzMzMzMzMzM3NzksMzMzMzMzMzMzMzMzMzMzMzQzMzMzMzMwNzQ3"
            "NDYwLDMzMzMzMzMzMzMzMzMzMzMzMzMzNzgwLDMzMzMzMzMzMzMzMzMzMzA4ODM1NTMzMDc0Mzc4"
            "MCwzMzMzMzMzMzMzMzMzMzMwODgzNTUzMzA3NDc0NjAsMzMzMzMzMzMzLDMzMzMzMzMzMzMzMzMz"
            "MzMzMzM3ODAsMzMzMzMzMzMzMzc4MCwzMzMzMzMzMzMzMzMwODgzNTUzMzA3NDc0NjAsMzMxNjY1"
            "MDAwMDMzMzMzMzA3NDczMzMzMzMzNzc5LDMzMzMzMzMzMzM3ODAsMzMzMzMzMzgzNTUzMzA3NDc0"
            "NjAsMzMxNjY1MDAwMDMzMzMzMzA3NDczMzMzMzMzNzc5LDMzMzMzMzMzMzMzMzMzMzM0MzMzMzMz"
            "MzA3NDc0NjAsMzMzMzMzMzMzMzMzMzMzMzMzMzM3ODAsMzMzMzMzMzMzMzMzMzMzMDg4MzU1MzMw"
            "NzQzNzgwLDMzMzMzMzMzMzMzMzMzMzA4ODM1NTMzMDc0NzQ2MCwzMzMzMzMzMzE2NjUwMDAwMzMz"
            "MzM0NzQ2MCwzMzMzMzMzMzMzMzMzMzMzMzMzNzgwLDMzMzMzMzMzMzMzMzM0MzMzMzMxNjUzNzM3"
            "MzAsMzMzMzMzMzMzMzMzMzMzMzMzMzc4MCwzMzMzMzMzODM1NTMzMDc0NzQ2MCwzMzE2NjUwMDAw"
            "MzMzMzMzMDc0NzMzMzMzMzM3NzksMzMzMzMzMzMzMzMzMzMzMzQzMzMzMzMzMDc0NzQ2MCwzMzMz"
            "MzMzMzMzMzMzMzMzMzMzMzc4MCwzMzMzMzMzMzMzMzMzMzMwODgzNTUzMzA3NDM3ODAsMzMzMzMz"
            "MzMzMzMzMzMzMDg4MzU1MzMwNzQ3NDYwLDMzMzMzMzMzMTY2NTAwMDAzMzMzMzQ3NDYwLDMzMzMz"
            "MzMzMzMzMzMzMzMzMzM3ODAsMzMzMzMzMzMzMzMzNzMzMzM0MzMzMzMzMzA3NDc0NjAsMzMzMzMz"
            "MzMzMzMzMzMzMzMzMzc4MCwzMzMzMzMzMzMzMzMwODgzNTUzMzA3NDc0NjAsMzMxNjY1MDAwMDMz"
            "MzMzMzA3NDczMzMzMzMzNzc5LDMzMzMzMzMzMzMzMzMzMzM0MzMzMzNcdWQ4N2RcdWRlZGV1ZGM4"
            "ZGUzOVx1ZDg0ZFx1ZGM4M2RcdWQ4OGRcdWRlMzlcdWQ4OWRcdWRlMjM5MzMzZWUzOVxk";
        auto const len = base64::decoded_size(s.size());
        std::unique_ptr<char[]> p(new char[len]);
        base64::decode(
            p.get(), s.data(), s.size());
        string_view const js(p.get(), len);
        BOOST_TEST(! validate(js));
    }

    void
    testIssue113()
    {
        string_view s =
            "\"\\r\\n section id='description'>\\r\\nAll        mbers form the uncountable set "
            "\\u211D.  Among its subsets, relatively simple are the convex sets, each expressed "
            "as a range between two real numbers <i>a</i> and <i>b</i> where <i>a</i> \\u2264 <i>"
            "b</i>.  There are actually four cases for the meaning of \\\"between\\\", depending "
            "on open or closed boundary:\\r\\n\\r\\n<ul>\\r\\n  <li>[<i>a</i>, <i>b</i>]: {<i>"
            "x</i> | <i>a</i> \\u2264 <i>x</i> and <i>x</i> \\u2264 <i>b</i> }</li>\\r\\n  <li>"
            "(<i>a</i>, <i>b</i>): {<i>x</i> | <i>a</i> < <i>x</i> and <i>x</i> < <i>b</i> }"
            "</li>\\r\\n  <li>[<i>a</i>, <i>b</i>): {<i>x</i> | <i>a</i> \\u2264 <i>x</i> and "
            "<i>x</i> < <i>b</i> }</li>\\r\\n  <li>(<i>a</i>, <i>b</i>]: {<i>x</i> | <i>a</i> "
            "< <i>x</i> and <i>x</i> \\u2264 <i>b</i> }</li>\\r\\n</ul>\\r\\n\\r\\nNote that "
            "if <i>a</i> = <i>b</i>, of the four only [<i>a</i>, <i>a</i>] would be non-empty."
            "\\r\\n\\r\\n<strong>Task</strong>\\r\\n\\r\\n<ul>\\r\\n  <li>Devise a way to "
            "represent any set of real numbers, for the definition of \\\"any\\\" in the "
            "implementation notes below.</li>\\r\\n  <li>Provide methods for these common "
            "set operations (<i>x</i> is a real number; <i>A</i> and <i>B</i> are sets):</li>"
            "\\r\\n  <ul>\\r\\n    <li>\\r\\n      <i>x</i> \\u2208 <i>A</i>: determine if <i>"
            "x</i> is an element of <i>A</i><br>\\r\\n      example: 1 is in [1, 2), while 2, "
            "3, ... are not.\\r\\n    </li>\\r\\n    <li>\\r\\n      <i>A</i> \\u222A <i>B</i>: "
            "union of <i>A</i> and <i>B</i>, i.e. {<i>x</i> | <i>x</i> \\u2208 <i>A</i> or <i>x"
            "</i> \\u2208 <i>B</i>}<br>\\r\\n      example: [0, 2) \\u222A (1, 3) = [0, 3); "
            "[0, 1) \\u222A (2, 3] = well, [0, 1) \\u222A (2, 3]\\r\\n    </li>\\r\\n    <li>"
            "\\r\\n      <i>A</i> \\u2229 <i>B</i>: intersection of <i>A</i> and <i>B</i>, i.e. "
            "{<i>x</i> | <i>x</i> \\u2208 <i>A</i> and <i>x</i> \\u2208 <i>B</i>}<br>\\r\\n      "
            "example: [0, 2) \\u2229 (1, 3) = (1, 2); [0, 1) \\u2229 (2, 3] = empty set\\r\\n    "
            "</li>\\r\\n    <li>\\r\\n      <i>A</i> - <i>B</i>: difference between <i>A</i> and "
            "<i>B</i>, also written as <i>A</i> \\\\ <i>B</i>, i.e. {<i>x</i> | <i>x</i> \\u2208 "
            "<i>A</i> and <i>x</i> \\u2209 <i>B</i>}<br>\\r\\n      example: [0, 2) \\u2212 (1, "
            "3) = [0, 1]\\r\\n    </li>\\r\\n  </ul>\\r\\n</ul>\\r\\n</section>\\r\\n\"\n";
        good_one(s);
    }

    class comment_parser
    {
        struct handler
        {
            constexpr static std::size_t max_object_size = std::size_t(-1);
            constexpr static std::size_t max_array_size = std::size_t(-1);
            constexpr static std::size_t max_key_size = std::size_t(-1);
            constexpr static std::size_t max_string_size = std::size_t(-1);

            std::string captured = "";
            bool on_document_begin( error_code& ) { return true; }
            bool on_document_end( error_code& ) { return true; }
            bool on_object_begin( error_code& ) { return true; }
            bool on_object_end( std::size_t, error_code& ) { return true; }
            bool on_array_begin( error_code& ) { return true; }
            bool on_array_end( std::size_t, error_code& ) { return true; }
            bool on_key_part( string_view, std::size_t, error_code& ) { return true; }
            bool on_key( string_view, std::size_t, error_code& ) { return true; }
            bool on_string_part( string_view, std::size_t, error_code& ) { return true; }
            bool on_string( string_view, std::size_t, error_code& ) { return true; }
            bool on_number_part( string_view, error_code&) { return true; }
            bool on_int64( std::int64_t, string_view, error_code& ) { return true; }
            bool on_uint64( std::uint64_t, string_view, error_code& ) { return true; }
            bool on_double( double, string_view, error_code& ) { return true; }
            bool on_bool( bool, error_code& ) { return true; }
            bool on_null( error_code& ) { return true; }
            bool on_comment_part( string_view s, error_code& )
            {
                captured.append(s.data(), s.size());
                return true;
            }
            bool on_comment( string_view s, error_code& )
            {
                captured.append(s.data(), s.size());
                return true;
            }
        };

        basic_parser<handler> p_;

    public:
        comment_parser()
            : p_(make_options(true, false, false))
        {
        }

        std::size_t
        write(
            char const* data,
            std::size_t size,
            error_code& ec)
        {
            auto const n = p_.write_some(
                false, data, size, ec);
            if(! ec && n < size)
                ec = error::extra_data;
            return n;
        }

        string_view
        captured() const noexcept
        {
            return p_.handler().captured;
        }
    };

    void
    testComments()
    {
        parse_options disabled;
        parse_options enabled;
        enabled.allow_comments = true;

        const auto replace_and_test =
            [&](string_view s)
        {
            static std::vector<string_view> comments =
            {
                "//\n",
                "//    \n",
                "//aaaa\n",
                "//    aaaa\n",
                "//    /* \n",
                "//    /**/ \n",
                "/**/",
                "/*//*/",
                "/*/*/",
                "/******/",
                "/***    ***/",
                "/**aaaa***/",
                "/***    aaaa***/"
            };

            std::string formatted = "";
            std::string just_comments = "";
            std::size_t guess = std::count(
                s.begin(), s.end(), '@') * 12;
            formatted.reserve(guess + s.size());
            just_comments.reserve(guess);
            std::size_t n = 0;
            for (char c : s)
            {
                if (c == '@')
                {
                    string_view com =
                        comments[((formatted.size() + n) % s.size()) % comments.size()];
                    formatted.append(com.data(), n = com.size());
                    just_comments.append(com.data(), com.size());
                    continue;
                }
                formatted += c;
            }
            bad(formatted, disabled);
            good(formatted, enabled);

            {
                // test the handler
                comment_parser p;
                error_code ec;
                p.write( formatted.data(), formatted.size(), ec );
                BOOST_TEST(! ec);
                BOOST_TEST(p.captured() == just_comments);
            }
        };

        replace_and_test("@1");
        replace_and_test("1@");
        replace_and_test("@1@");
        replace_and_test("[@1]");
        replace_and_test("[1@]");
        replace_and_test("[1,2@]");
        replace_and_test("[1,@2]");
        replace_and_test("[1@,2]");
        replace_and_test("@[@1@,@2@]@");
        replace_and_test("{@\"a\":1}");
        replace_and_test("{\"a\"@:1}");
        replace_and_test("{\"a\":1@}");
        replace_and_test("{\"a\":1@,\"b\":2}");
        replace_and_test("{\"a\":1,@\"b\":2}");
        replace_and_test("@{@\"a\"@:@1@,@\"b\"@:@2@}");

        // no following token
        bad("1/", enabled);
        // bad second token
        bad("1/x", enabled);
        // no comment close
        bad("1/*", enabled);
        bad("1/**", enabled);
        bad("[1 //, 2]", enabled);

        // just comment
        bad("//\n", enabled);
        bad("//", enabled);
        bad("/**/", enabled);

        // no newline at EOF
        good("1//", enabled);
    }

    void
    testAllowTrailing()
    {
        parse_options disabled;
        parse_options enabled;
        enabled.allow_trailing_commas = true;

        bad("[1,]", disabled);
        good("[1,]", enabled);

        bad("[1,[],]", disabled);
        good("[1,[],]", enabled);

        bad("[1,{},]", disabled);
        good("[1,{},]", enabled);

        bad("[1,{\"a\":1,},]", disabled);
        good("[1,{\"a\":1,},]", enabled);

        bad("{\"a\":1,}", disabled);
        good("{\"a\":1,}", enabled);

        bad("{\"a\":[1,],}", disabled);
        good("{\"a\":[1,],}", enabled);

        bad("{\"a\":[],}", disabled);
        good("{\"a\":[],}", enabled);

        bad("{\"a\":[{}, [1,]],}", disabled);
        good("{\"a\":[{}, [1,]],}", enabled);

        bad("[[[[[[[],],],],],],]", disabled);
        good("[[[[[[[],],],],],],]", enabled);

        bad("{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{},},},},},},}", disabled);
        good("{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{},},},},},},}", enabled);
    }

    class utf8_parser
    {
        struct handler
        {
            constexpr static std::size_t max_object_size = std::size_t(-1);
            constexpr static std::size_t max_array_size = std::size_t(-1);
            constexpr static std::size_t max_key_size = std::size_t(-1);
            constexpr static std::size_t max_string_size = std::size_t(-1);

            std::string captured = "";
            bool on_document_begin( error_code& ) { return true; }
            bool on_document_end( error_code& ) { return true; }
            bool on_object_begin( error_code& ) { return true; }
            bool on_object_end( std::size_t, error_code& ) { return true; }
            bool on_array_begin( error_code& ) { return true; }
            bool on_array_end( std::size_t, error_code& ) { return true; }
            bool on_key_part( string_view, std::size_t, error_code& ) { return true; }
            bool on_key( string_view, std::size_t, error_code& ) { return true; }
            bool on_string_part( string_view sv, std::size_t, error_code& )
            {
                captured.append(sv.data(), sv.size());
                return true;
            }
            bool on_string( string_view sv, std::size_t, error_code& )
            {
                captured.append(sv.data(), sv.size());
                return true;
            }
            bool on_number_part( string_view, error_code&) { return true; }
            bool on_int64( std::int64_t, string_view, error_code& ) { return true; }
            bool on_uint64( std::uint64_t, string_view, error_code& ) { return true; }
            bool on_double( double, string_view, error_code& ) { return true; }
            bool on_bool( bool, error_code& ) { return true; }
            bool on_null( error_code& ) { return true; }
            bool on_comment_part( string_view, error_code& ) { return true; }
            bool on_comment( string_view, error_code& ) { return true; }
        };

        basic_parser<handler> p_;

    public:
        utf8_parser()
            : p_(parse_options())
        {
        }

        std::size_t
        write(
            bool more,
            char const* data,
            std::size_t size,
            error_code& ec)
        {
            auto const n = p_.write_some(
                more, data, size, ec);
            if(! ec && n < size)
                ec = error::extra_data;
            return n;
        }

        string_view
        captured() const noexcept
        {
            return p_.handler().captured;
        }
    };

    void
    testUTF8Validation()
    {
        good("\"\xc2\x80----------\"");
        good("\"\xc2\xbf----------\"");
        good("\"\xdf\x80----------\"");
        good("\"\xdf\xbf----------\"");

        good("\"\xcf\x90----------\"");

        good("\"\xe0\xa0\x80----------\"");
        good("\"\xe0\xa0\xbf----------\"");
        good("\"\xe0\xbf\x80----------\"");
        good("\"\xe0\xbf\xbf----------\"");

        good("\"\xe0\xb0\x90----------\"");

        good("\"\xe1\x80\x80----------\"");
        good("\"\xe1\xbf\x80----------\"");
        good("\"\xec\x80\x80----------\"");
        good("\"\xec\xbf\x80----------\"");
        good("\"\xe1\x80\xbf----------\"");
        good("\"\xe1\xbf\xbf----------\"");
        good("\"\xec\x80\xbf----------\"");
        good("\"\xec\xbf\xbf----------\"");

        good("\"\xe6\x90\x90----------\"");

        good("\"\xed\x80\x80----------\"");
        good("\"\xed\x80\xbf----------\"");
        good("\"\xed\x9f\x80----------\"");
        good("\"\xed\x9f\xbf----------\"");

        good("\"\xed\x90\x90----------\"");

        good("\"\xee\x80\x80----------\"");
        good("\"\xee\xbf\x80----------\"");
        good("\"\xef\x80\x80----------\"");
        good("\"\xef\xbf\x80----------\"");
        good("\"\xee\x80\xbf----------\"");
        good("\"\xee\xbf\xbf----------\"");
        good("\"\xef\x80\xbf----------\"");
        good("\"\xef\xbf\xbf----------\"");

        good("\"\xee\x90\x90----------\"");
        good("\"\xef\x90\x90----------\"");

        good("\"\xf0\x90\x80\x80----------\"");
        good("\"\xf0\x90\xbf\x80----------\"");
        good("\"\xf0\x90\xbf\xbf----------\"");
        good("\"\xf0\x90\x80\xbf----------\"");
        good("\"\xf0\xbf\x80\x80----------\"");
        good("\"\xf0\xbf\xbf\x80----------\"");
        good("\"\xf0\xbf\xbf\xbf----------\"");
        good("\"\xf0\xbf\x80\xbf----------\"");

        good("\"\xf0\xA0\x90\x90----------\"");

        good("\"\xf4\x80\x80\x80----------\"");
        good("\"\xf4\x80\xbf\x80----------\"");
        good("\"\xf4\x80\xbf\xbf----------\"");
        good("\"\xf4\x80\x80\xbf----------\"");
        good("\"\xf4\x8f\x80\x80----------\"");
        good("\"\xf4\x8f\xbf\x80----------\"");
        good("\"\xf4\x8f\xbf\xbf----------\"");
        good("\"\xf4\x8f\x80\xbf----------\"");

        good("\"\xf4\x88\x90\x90----------\"");

        good("\"\xf1\x80\x80\x80----------\"");
        good("\"\xf1\x80\xbf\x80----------\"");
        good("\"\xf1\x80\xbf\xbf----------\"");
        good("\"\xf1\x80\x80\xbf----------\"");
        good("\"\xf1\xbf\x80\x80----------\"");
        good("\"\xf1\xbf\xbf\x80----------\"");
        good("\"\xf1\xbf\xbf\xbf----------\"");
        good("\"\xf1\xbf\x80\xbf----------\"");
        good("\"\xf3\x80\x80\x80----------\"");
        good("\"\xf3\x80\xbf\x80----------\"");
        good("\"\xf3\x80\xbf\xbf----------\"");
        good("\"\xf3\x80\x80\xbf----------\"");
        good("\"\xf3\xbf\x80\x80----------\"");
        good("\"\xf3\xbf\xbf\x80----------\"");
        good("\"\xf3\xbf\xbf\xbf----------\"");
        good("\"\xf3\xbf\x80\xbf----------\"");

        good("\"\xf2\x90\x90\x90----------\"");

        bad("\"\xc0\x80----------\"");
        bad("\"\xc2\xc0----------\"");
        bad("\"\xef\x80----------\"");
        bad("\"\xdf\x70----------\"");

        bad("\"\xff\x90----------\"");

        bad("\"\xe0\x9f\x80----------\"");
        bad("\"\xe0\xa0\xfe----------\"");
        bad("\"\xc0\xff\xff----------\"");
        bad("\"\xc0\xbf\x76----------\"");

        bad("\"\xe0\xde\x90----------\"");

        bad("\"\xe1\x80\x7f----------\"");
        bad("\"\xe1\x7f\x80----------\"");
        bad("\"\xec\xff\x80----------\"");
        bad("\"\xef\x7f\x80----------\"");
        bad("\"\xe1\x80\xff----------\"");
        bad("\"\xe1\xbf\x0f----------\"");
        bad("\"\xec\x01\xff----------\"");
        bad("\"\xec\xff\xff----------\"");

        bad("\"\xe6\x60\x90----------\"");

        bad("\"\xed\x7f\x80----------\"");
        bad("\"\xed\xa0\xbf----------\"");
        bad("\"\xed\xbf\x80----------\"");
        bad("\"\xed\x9f\x7f----------\"");

        bad("\"\xed\xce\xbf----------\"");

        bad("\"\xee\x7f\x80----------\"");
        bad("\"\xee\xcc\x80----------\"");
        bad("\"\xef\x80\xcc----------\"");
        bad("\"\xef\xbf\x0a----------\"");
        bad("\"\xee\x50\xbf----------\"");
        bad("\"\xee\xef\xbf----------\"");
        bad("\"\xef\xf0\xff----------\"");
        bad("\"\xef\xaa\xee----------\"");

        bad("\"\xc0\x90\x90----------\"");
        bad("\"\xc1\x90\x90----------\"");

        bad("\"\xff\x90\x80\x80----------\"");
        bad("\"\xfe\x90\xbf\x80----------\"");
        bad("\"\xfd\x90\xbf\xbf----------\"");
        bad("\"\xf0\xff\x80\xbf----------\"");
        bad("\"\xf0\xfe\x80\x80----------\"");
        bad("\"\xf0\xfd\xbf\x80----------\"");
        bad("\"\xf0\x90\x80\xff----------\"");
        bad("\"\xf0\x90\x5f\x80----------\"");

        bad("\"\xf4\x70\x80\x80----------\"");
        bad("\"\xf4\x80\x70\x80----------\"");
        bad("\"\xf4\x80\xbf\x70----------\"");
        bad("\"\xf4\xce\x80\xbf----------\"");
        bad("\"\xf4\x8f\xce\x80----------\"");
        bad("\"\xf4\x8f\xbf\xce----------\"");

        bad("\"\xf1\x7f\xbf\xbf----------\"");
        bad("\"\xf2\x80\x7f\xbf----------\"");
        bad("\"\xf3\x80\xbf\xce----------\"");

        // utf8 after escape
        good("\"\\u0000 \xf3\xbf\x80\xbf\xf3\xbf\x80\xbf\"");
        good("\"\\ud7ff\xf4\x80\xbf\xbf       \"");
        good("\"\\ue000            \xef\xbf\x80\"");
        good("\"\xef\xbf\x80 \\uffff \xef\xbf\x80\"");
        good("\"\xc2\x80\xc2\x80\xc2\x80\xc2\x80\xc2\x80\\ud800\\udc00 \"");
        good("\"\\udbff\\udfff \xe1\x80\xbf  \\udbff\\udfff \xe1\x80\xbf\"");
        good("\"\\u0000\xe1\x80\xbf     \"");
        bad("\"\\t\\t\xf4\x70\x80\x80----------\"");
        bad("\"\\n\xf4\x80\x70\x80----------\"");
        bad("\"\\n\xf4\x80\xbf\x70-\\n\xf4\x80\xbf\x70\"");

        const auto check =
            [this](string_view expected)
        {
            good(expected);
            for (std::size_t write_size : {2, 4, 8})
            {
                utf8_parser p;
                for(std::size_t i = 0; i < expected.size(); i += write_size)
                {
                    error_code ec;
                    write_size = (std::min)(write_size, expected.size() - i);
                    auto more = (i < expected.size() - write_size);
                    auto written = p.write(more,
                            expected.data() + i, write_size, ec);
                    BOOST_TEST(written == write_size);
                    BOOST_TEST(! ec);
                }
                BOOST_TEST(p.captured() ==
                    expected.substr(1, expected.size() - 2));
            }
        };

        check("\"\xd1\x82\"");
        check("\"\xd1\x82\xd0\xb5\xd1\x81\xd1\x82\"");
        check("\"\xc3\x0b1""and\xc3\xba\"");
    }

    void
    testMaxDepth()
    {
        {
            string_view s = "[[[[[]]]]]";
            parse_options opt;
            opt.max_depth = 4;
            null_parser p(opt);
            error_code ec;
            p.write(s.data(), s.size(), ec);
            BOOST_TEST(ec == error::too_deep);
        }
        {
            string_view s = "[[[[]]], [[[[]]]]]";
            parse_options opt;
            opt.max_depth = 4;
            null_parser p(opt);
            error_code ec;
            p.write(s.data(), s.size(), ec);
            BOOST_TEST(ec == error::too_deep);
        }
        {
            string_view s =
                "{\"a\":{\"b\":{\"c\":{}}},\"b\":{\"c\":{\"d\":{\"e\":{}}}}}";
            parse_options opt;
            opt.max_depth = 4;
            null_parser p(opt);
            error_code ec;
            p.write(s.data(), s.size(), ec);
            BOOST_TEST(ec == error::too_deep);
        }
        {
            string_view s =
                "{\"a\":{\"b\":{\"c\":{\"d\":{}}}}}";
            parse_options opt;
            opt.max_depth = 4;
            null_parser p(opt);
            error_code ec;
            p.write(s.data(), s.size(), ec);
            BOOST_TEST(ec == error::too_deep);
        }
    }

    class literal_parser
    {
        struct handler
        {
            constexpr static std::size_t max_object_size = std::size_t(-1);
            constexpr static std::size_t max_array_size = std::size_t(-1);
            constexpr static std::size_t max_key_size = std::size_t(-1);
            constexpr static std::size_t max_string_size = std::size_t(-1);

            std::string captured = "";
            bool on_document_begin( error_code& ) { return true; }
            bool on_document_end( error_code& ) { return true; }
            bool on_object_begin( error_code& ) { return true; }
            bool on_object_end( std::size_t, error_code& ) { return true; }
            bool on_array_begin( error_code& ) { return true; }
            bool on_array_end( std::size_t, error_code& ) { return true; }
            bool on_key_part( string_view, std::size_t, error_code& ) { return true; }
            bool on_key( string_view, std::size_t, error_code& ) { return true; }
            bool on_string_part( string_view, std::size_t, error_code& ) { return true; }
            bool on_string( string_view, std::size_t, error_code& ) { return true; }
            bool on_number_part( string_view sv, error_code&)
            {
                captured.append(sv.data(), sv.size());
                return true;
            }
            bool on_int64( std::int64_t, string_view sv, error_code& )
            {
                captured.append(sv.data(), sv.size());
                captured += 's';
                return true;
            }
            bool on_uint64( std::uint64_t, string_view sv, error_code& )
            {
                captured.append(sv.data(), sv.size());
                captured += 'u';
                return true;
            }
            bool on_double( double, string_view sv, error_code& )
            {
                captured.append(sv.data(), sv.size());
                captured += 'd';
                return true;
            }
            bool on_bool( bool, error_code& ) { return true; }
            bool on_null( error_code& ) { return true; }
            bool on_comment_part( string_view, error_code& ) { return true; }
            bool on_comment( string_view, error_code& ) { return true; }
        };

        basic_parser<handler> p_;

    public:
        literal_parser()
            : p_(make_options(true, false, false))
        {
        }

        std::size_t
        write(
            bool more,
            char const* data,
            std::size_t size,
            error_code& ec)
        {
            auto const n = p_.write_some(
                more, data, size, ec);
            if(! ec && n < size)
                ec = error::extra_data;
            return n;
        }

        string_view
        captured()
        {
            return p_.handler().captured;
        }
    };

    void
    testNumberLiteral()
    {
        const auto check =
        [](string_view expected)
        {
            string_view sv = expected;
            sv.remove_suffix(1);
            for(std::size_t i = 0;
                i < sv.size(); ++i)
            {
                literal_parser p;
                error_code ec;
                if(i != 0)
                {
                    p.write(true,
                        sv.data(), i, ec);
                }
                if(BOOST_TEST(! ec))
                {
                    p.write(false,
                        sv.data() + i,
                        sv.size() - i, ec);
                }
                BOOST_TEST(! ec);
                BOOST_TEST(p.captured() == expected);
            }
        };

        check("1s");
        check("-1s");
        check("0s");
        check("0s");
        check("123456s");
        check("-123456s");
        check("9223372036854775808u");

        check("1.0d");
        check("-1.0d");
        check("0.0d");
        check("1.0e3d");
        check("1e1d");
        check("1e+1d");
        check("1e-1d");
        check("-100000000000000000000000d");
        check("100000000000000000000000d");
        check("10000000000.10000000000000d");
        check("-10000000000.10000000000000d");
        check("1000000000000000.0e1000000d");
    }

    void
    testStickyErrors()
    {
        {
            null_parser p;
            error_code ec;
            p.write("*", 1, ec);
            BOOST_TEST(ec);
            error_code ec2;
            p.write("[]", 2, ec2);
            BOOST_TEST(ec2 == ec);
            p.reset();
            p.write("[]", 2, ec2);
            BOOST_TEST(! ec2);
        }

        // exceptions do not cause UB
        {
            throw_parser p(1);
            try
            {
                error_code ec;
                p.write(false, "null", 4, ec);
                BOOST_TEST_FAIL();
            }
            catch(std::exception const&)
            {
                error_code ec;
                p.write(false, "null", 4, ec);
                BOOST_TEST(ec == error::exception);
            }
        }
    }

    void
    run()
    {
        testNull();
        testBoolean();
        testString();
        testNumber();
        testArray();
        testObject();
        testParser();
        testMembers();
        testParseVectors();
        testIssue13();
        testIssue20();
        testIssue113();
        testAllowTrailing();
        testComments();
        testUTF8Validation();
        testMaxDepth();
        testNumberLiteral();
        testStickyErrors();
    }
};

TEST_SUITE(basic_parser_test, "boost.json.basic_parser");

BOOST_JSON_NS_END
