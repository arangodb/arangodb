//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

// checks issue https://svn.boost.org/trac/boost/ticket/5563 in particular
// the ability of the framework to detect new fixture signatures.

#define BOOST_TEST_MODULE test_fixture_detect_setup_teardown
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/framework.hpp>

using namespace boost::unit_test;

class fixture_without {
public:
    fixture_without() {}
    ~fixture_without() {}
};

class fixture_with {
public:
    fixture_with() {}
    void setup() {}
    void teardown() {}
    ~fixture_with() {}
};

class fixture_with_child : public fixture_with {
public:
    fixture_with_child() {}
    ~fixture_with_child() {}
};

BOOST_AUTO_TEST_CASE( fixture_setup_teardown_detect )
{
    BOOST_CHECK(!impl_fixture::has_setup<fixture_without>::value);
    BOOST_CHECK(!impl_fixture::has_setup<fixture_without>::value);

    fixture_without obj;
    setup_conditional(obj);
    teardown_conditional(obj);
}

BOOST_AUTO_TEST_CASE( fixture_setup_teardown_detect_both )
{
    BOOST_CHECK(impl_fixture::has_setup<fixture_with>::value);
    BOOST_CHECK(impl_fixture::has_setup<fixture_with>::value);

    fixture_with obj;
    setup_conditional(obj);
    teardown_conditional(obj);
}

#if defined(BOOST_NO_CXX11_DECLTYPE) || defined(BOOST_NO_CXX11_TRAILING_RESULT_TYPES)

BOOST_AUTO_TEST_CASE( fixture_setup_teardown_detect_both_from_child )
{
    // cannot detect this with the C++03 approach
    BOOST_CHECK(!impl_fixture::has_setup<fixture_with_child>::value);
    BOOST_CHECK(!impl_fixture::has_setup<fixture_with_child>::value);

    fixture_with_child obj;
    setup_conditional(obj);
    teardown_conditional(obj);
}

#endif
