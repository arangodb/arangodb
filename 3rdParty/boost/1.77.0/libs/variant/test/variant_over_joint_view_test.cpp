// Copyright (c) 2017
// Mikhail Maximov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// The test is base on https://svn.boost.org/trac/boost/ticket/8554 
// variant was not able to extract types from mpl::joint_view

#include <string>

#include "boost/config.hpp"
#include "boost/core/lightweight_test.hpp"

#include "boost/variant.hpp"
#include "boost/mpl/joint_view.hpp"
#include "boost/mpl/insert_range.hpp"
#include "boost/mpl/set.hpp"

template<class T, class Variant>
void check_exception_on_get(Variant& v) {
    try {
        boost::get<T>(v);
        BOOST_ERROR("Expected exception boost::bad_get, but got nothing.");
    } catch (boost::bad_get&) { //okay it is expected behaviour
    } catch (...) { BOOST_ERROR("Expected exception boost::bad_get, but got something else."); }
}

void test_joint_view() {
    typedef boost::variant<int> v1;
    typedef boost::variant<std::string> v2;
    typedef boost::make_variant_over<boost::mpl::joint_view<v1::types, v2::types>::type>::type v3;

    v1 a = 1;
    v2 b = "2";
    v3 c = a;
    BOOST_TEST(boost::get<int>(c) == 1);
    BOOST_TEST(c.which() == 0);
    v3 d = b;
    BOOST_TEST(boost::get<std::string>(d) == "2");
    BOOST_TEST(d.which() == 1);
    check_exception_on_get<std::string>(c);
    check_exception_on_get<int>(d);
}

void test_set() {
    typedef boost::mpl::set2< std::string, int > types;
    typedef boost::make_variant_over< types >::type v;

    v a = 1;
    BOOST_TEST(boost::get<int>(a) == 1);
    check_exception_on_get<std::string>(a);
    a = "2";
    BOOST_TEST(boost::get<std::string>(a) == "2");
    check_exception_on_get<int>(a);
}

int main()
{
    test_joint_view();
    test_set();
    return boost::report_errors();
}
