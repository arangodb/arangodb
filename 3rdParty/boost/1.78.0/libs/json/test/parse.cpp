//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/parse.hpp>

#include <boost/json/serialize.hpp>

#include <string>

#include "test.hpp"
#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

class parse_test
{
public:
    void
    good(string_view s)
    {
        error_code ec;
        auto jv = parse(s, ec);
        if(! BOOST_TEST(! ec))
            return;
        BOOST_TEST(
            serialize(jv) == s);
    }

    void
    bad(string_view s)
    {
        error_code ec;
        auto jv = parse(s, ec);
        BOOST_TEST(ec);
    }

    void
    testParse()
    {
        good("null");
        good("[1,2,3]");
        bad ("[1,2,3] #");
        bad ("555415214748364655415E2147483646");
        bad ("9.88874836020e-2147483640");
    }

    void
    testMemoryUsage()
    {
        // string
        {
            std::string s(10000, '*');
            s.insert(s.begin(), '\"');
            s.append(1, '\"');

            fail_resource mr1;
            value jv1 = parse(s, &mr1);

            fail_resource mr2;
            value jv2(jv1, &mr2);
            BOOST_TEST(mr1.bytes == mr2.bytes);

            fail_resource mr3;
            value jv3(&mr3);
            jv3 = jv1;
            BOOST_TEST(mr1.bytes == mr3.bytes);
        }

        // array
        {
            {
                fail_resource mr1;
                value jv1 = parse("[1]", &mr1);

                fail_resource mr2;
                value jv2(jv1, &mr2);
                BOOST_TEST(mr1.bytes == mr2.bytes);

                fail_resource mr3;
                value jv3(&mr3);
                jv3 = jv1;
                BOOST_TEST(mr1.bytes == mr3.bytes);
            }
            {
                std::string s;
                s = "1,2,3,4";
                s = s + "," + s;
                s = s + "," + s;
                s = s + "," + s;
                s = s + "," + s;
                s = s + "," + s;
                s = s + "," + s;
                s = s + "," + s;
                s = s + "," + s;
                s = "[[" + s + "]]";

                fail_resource mr1;
                value jv1 = parse(s, &mr1);

                fail_resource mr2;
                value jv2(jv1, &mr2);
                BOOST_TEST(mr1.bytes == mr2.bytes);

                fail_resource mr3;
                value jv3(&mr3);
                jv3 = jv1;
                BOOST_TEST(mr1.bytes == mr3.bytes);
            }
        }

        // object
        {
            std::string s;
            s = "{\"0\":null";
            for(int i = 1; i < 1000; ++i)
                s +=
                    ",\"" +
                    std::to_string(i) +
                    "\":null";
            s.append("}");

            fail_resource mr1;
            value jv1 = parse(s, &mr1);

            fail_resource mr2;
            value jv2(jv1, &mr2);
            BOOST_TEST(mr1.bytes == mr2.bytes);

            fail_resource mr3;
            value jv3(&mr3);
            jv3 = jv1;
            BOOST_TEST(mr1.bytes == mr3.bytes);
        }
    }

    void
    run()
    {
        testParse();
        testMemoryUsage();
    }
};

TEST_SUITE(parse_test, "boost.json.parse");

BOOST_JSON_NS_END
