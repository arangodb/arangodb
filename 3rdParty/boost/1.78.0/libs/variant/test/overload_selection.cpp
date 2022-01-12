//-----------------------------------------------------------------------------
// boost-libs variant/test/variant_get_test.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


// This test suite was created to cover issues reported in:
//      https://svn.boost.org/trac/boost/ticket/5871
//      https://svn.boost.org/trac/boost/ticket/11602

#include "boost/variant/variant.hpp"
#include "boost/variant/recursive_variant.hpp"
#include "boost/core/lightweight_test.hpp"

#include <string>
#include <list>

struct A{};
struct B{};
struct C{};
struct D{};


bool foo(const boost::variant<A, B>& ) {
    return false;
}

bool foo(const boost::variant<C, D>& ) {
    return true;
}

void test_overload_selection_variant_constructor() {
    D d;
    BOOST_TEST(foo(d));

    boost::variant<B, A> v;
    BOOST_TEST(!foo(v));
}

// Pre msvc-14.0 could not dustinguish between multiple assignment operators:
//      warning C4522: 'assignment_tester' : multiple assignment operators specified
//      error C2666: variant::operator =' : 3 overloads have similar conversions
// Old versions of GCC have same issue:
//      error: variant::operator=(const T&) cannot be overloaded
#if (defined(__GNUC__) && (__GNUC__ < 4)) || (defined(_MSC_VER) && _MSC_VER < 1900)

void test_overload_selection_variant_assignment() {
    BOOST_TEST(true);
}

#else

struct assignment_tester: boost::variant<C, D>, boost::variant<B, A> {
    using boost::variant<B, A>::operator=;
    using boost::variant<C, D>::operator=;
};

void test_overload_selection_variant_assignment() {
    A a;
    assignment_tester tester;
    tester = a;
    const int which0 = static_cast< boost::variant<B, A>& >(tester).which();
    BOOST_TEST(which0 == 1);

    boost::variant<A, B> b;
    b = B();
    tester = b;
    const int which1 = static_cast< boost::variant<B, A>& >(tester).which();
    BOOST_TEST(which1 == 0);
}

#endif

typedef boost::variant<int> my_variant;

struct convertible {
    operator my_variant() const {
        return my_variant();
    }
};

void test_implicit_conversion_operator() {
    // https://svn.boost.org/trac/boost/ticket/8555
    my_variant y = convertible();
    BOOST_TEST(y.which() == 0);
}

struct X: boost::variant< int > {};
class V1: public boost::variant<float,double> {};

struct AB: boost::variant<A, B> {};

void test_derived_from_variant_construction() {
    // https://svn.boost.org/trac/boost/ticket/7120
    X x;
    boost::variant<X> y(x);
    BOOST_TEST(y.which() == 0);

    // https://svn.boost.org/trac/boost/ticket/10278
    boost::variant<V1, std::string> v2 = V1();
    BOOST_TEST(v2.which() == 0);

    // https://svn.boost.org/trac/boost/ticket/12155
    AB ab;
    boost::variant<AB, C> ab_c(ab);
    BOOST_TEST(ab_c.which() == 0);

    boost::variant<A, B> a_b(ab);
    BOOST_TEST(a_b.which() == 0);

    boost::variant<B, C, A> b_c_a1(static_cast<boost::variant<A, B>& >(ab));
    BOOST_TEST(b_c_a1.which() == 2);


//  Following conversion seems harmful as it may lead to slicing:
//  boost::variant<B, C, A> b_c_a(ab);
//  BOOST_TEST(b_c_a.which() == 2);
}

void test_derived_from_variant_assignment() {
    // https://svn.boost.org/trac/boost/ticket/7120
    X x;
    boost::variant<X> y;
    y = x;
    BOOST_TEST(y.which() == 0);

    // https://svn.boost.org/trac/boost/ticket/10278
    boost::variant<V1, std::string> v2;
    v2 = V1();
    BOOST_TEST(v2.which() == 0);

    // https://svn.boost.org/trac/boost/ticket/12155
    AB ab;
    boost::variant<AB, C> ab_c;
    ab_c = ab;
    BOOST_TEST(ab_c.which() == 0);

    boost::variant<A, B> a_b;
    a_b = ab;
    BOOST_TEST(a_b.which() == 0);

    boost::variant<B, C, A> b_c_a1;
    b_c_a1 = static_cast<boost::variant<A, B>& >(ab);
    BOOST_TEST(b_c_a1.which() == 2);


//  Following conversion seems harmful as it may lead to slicing:
//  boost::variant<B, C, A> b_c_a;
//  b_c_a = ab;
//  BOOST_TEST(b_c_a.which() == 2);
}


// http://thread.gmane.org/gmane.comp.lib.boost.devel/267757
struct info {
    struct nil_ {};

    typedef
        boost::variant<
            nil_
          , std::string
          , boost::recursive_wrapper<info>
          , boost::recursive_wrapper<std::pair<info, info> >
          , boost::recursive_wrapper<std::list<info> >
        >
    value_type;
    value_type v;

    inline void test_on_incomplete_types() {
        info i0;
        i0.v = "Hello";
        BOOST_TEST(i0.v.which() == 1);

        info::value_type v0 = "Hello";
        BOOST_TEST(v0.which() == 1);

        info::value_type v1("Hello");
        BOOST_TEST(v1.which() == 1);

        info::value_type v2 = i0;
        BOOST_TEST(v2.which() == 2);

        info::value_type v3(i0);
        BOOST_TEST(v3.which() == 2);

        v0 = v3;
        BOOST_TEST(v0.which() == 2);

        v3 = v1;
        BOOST_TEST(v3.which() == 1);

        v3 = nil_();
        BOOST_TEST(v3.which() == 0);
    }
};



int main()
{
    test_overload_selection_variant_constructor();
    test_overload_selection_variant_assignment();
    test_implicit_conversion_operator();
    test_derived_from_variant_construction();
    test_derived_from_variant_assignment();
    info().test_on_incomplete_types();

    return boost::report_errors();
}
