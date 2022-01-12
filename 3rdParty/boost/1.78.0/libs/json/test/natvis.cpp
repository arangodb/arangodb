//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include <boost/json/value.hpp>

#include <boost/json/monotonic_resource.hpp>
#include <boost/json/static_resource.hpp>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

class natvis_test
{
public:
    static
    value
    make_key(string_view s)
    {
        value jv;
        detail::access::construct_value(
            &jv, s, storage_ptr());
        return jv;
    }

    void
    run()
    {
        // These variables may be inspected in the
        // Visual Studio IDE to verify that the .natvis
        // file is working correctly

        value jv0 = nullptr;
        value jv1 = true;
        value jv2 = -1;
        value jv3 = 42u;
        value jv4 = 3.141;
        value jv5 = "string";
        value jv6 = make_key("key");
        value jv7 = "dynamically-allocated-string";
        value jv8 = { 1, 2, 3 };
        value jv9 = { {"a",1}, {"b",2}, {"c",3} };

        string& x0 = jv5.get_string(); (void)x0;
        string& x1 = jv6.get_string(); (void)x1;
        string& x2 = jv7.get_string(); (void)x2;
        array&  x3 = jv8.get_array();  (void)x3;
        object& x4 = jv9.get_object(); (void)x4;

        array y0;
        y0.emplace_back(jv0);
        y0.emplace_back(jv1);
        y0.emplace_back(jv2);
        y0.emplace_back(jv4);
        y0.emplace_back(jv5);
        y0.emplace_back(jv6);
        y0.emplace_back(jv7);
        y0.emplace_back(jv8);
        y0.emplace_back(jv9);

        object y1;
        y1.emplace("a", jv0);
        y1.emplace("b", jv1);
        y1.emplace("c", jv2);
        y1.emplace("d", jv4);
        y1.emplace("f", jv5);
        y1.emplace("g", jv6);
        y1.emplace("h", jv7);
        y1.emplace("i", jv8);
        y1.emplace("j", jv9);

        key_value_pair& z0 = y1.begin()[0]; (void)z0;
        key_value_pair& z1 = y1.begin()[1]; (void)z1;
        key_value_pair& z2 = y1.begin()[2]; (void)z2;
        key_value_pair& z3 = y1.begin()[3]; (void)z3;
        key_value_pair& z4 = y1.begin()[4]; (void)z4;
        key_value_pair& z5 = y1.begin()[5]; (void)z5;
        key_value_pair& z6 = y1.begin()[6]; (void)z6;
        key_value_pair& z7 = y1.begin()[7]; (void)z7;
        key_value_pair& z8 = y1.begin()[8]; (void)z8;

        {
            auto sp = y1.storage();
        }
        {
            unsigned char buf[1024];
            storage_ptr sp;
            sp = make_shared_resource<
                monotonic_resource>();
            sp = make_shared_resource<
                static_resource>(buf);
            sp = {};
        }
        {
            unsigned char buf[1024];
            monotonic_resource mr1;
            static_resource mr2(buf);
            storage_ptr sp;

            sp = &mr1;
            sp = &mr2;
            sp = {};
        }
    }
};

TEST_SUITE(natvis_test, "json.natvis");

BOOST_JSON_NS_END
