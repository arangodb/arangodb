// Boost.TypeErasure library
//
// Copyright 2015 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// $Id$

#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/operators.hpp>
#include <boost/type_erasure/dynamic_any_cast.hpp>
#include <boost/type_erasure/any_cast.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/map.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

using namespace boost::type_erasure;

template<class T = _self>
struct common : ::boost::mpl::vector<
    copy_constructible<T>,
    typeid_<T>
> {};

struct fixture
{
    fixture()
    {
        register_binding<common<>, int>();
        register_binding<incrementable<>, int>();
        register_binding<addable<_self, _self, _a> >(make_binding<boost::mpl::map<boost::mpl::pair<_self, int>, boost::mpl::pair<_a, int> > >());
    }
};

BOOST_GLOBAL_FIXTURE(fixture);

BOOST_AUTO_TEST_CASE(test_identical)
{
    any<common<> > x(1);
    any<common<> > y = dynamic_any_cast<any<common<> > >(x);
    BOOST_CHECK_EQUAL(any_cast<int>(y), 1);
}

BOOST_AUTO_TEST_CASE(test_downcast)
{
    any<common<> > x(1);
    typedef any< ::boost::mpl::vector<common<>, incrementable<> > > incrementable_any;
    incrementable_any y = dynamic_any_cast<incrementable_any>(x);
    ++y;
    BOOST_CHECK_EQUAL(any_cast<int>(y), 2);
}

BOOST_AUTO_TEST_CASE(test_cross_cast)
{
    any< ::boost::mpl::vector<common<>, decrementable<> > > x(1);
    typedef any< ::boost::mpl::vector<common<>, incrementable<> > > incrementable_any;
    incrementable_any y = dynamic_any_cast<incrementable_any>(x);
    ++y;
    BOOST_CHECK_EQUAL(any_cast<int>(y), 2);
}

BOOST_AUTO_TEST_CASE(test_cast_placeholder)
{
    any<common<> > x(1);
    typedef any< ::boost::mpl::vector<common<_a>, incrementable<_a> >, _a> incrementable_any;
    incrementable_any y = dynamic_any_cast<incrementable_any>(x);
    ++y;
    BOOST_CHECK_EQUAL(any_cast<int>(y), 2);
}

BOOST_AUTO_TEST_CASE(test_throw)
{
    any<common<> > x("42");
    typedef any< ::boost::mpl::vector<common<_a>, incrementable<_a> >, _a> incrementable_any;
    BOOST_CHECK_THROW(dynamic_any_cast<incrementable_any>(x), bad_any_cast);
}

// make sure that a function registered with _self can
// be found with _a.
BOOST_AUTO_TEST_CASE(test_other_placeholder)
{
    any<common<_a>, _a> x(1);
    typedef any< ::boost::mpl::vector<common<_a>, incrementable<_a> >, _a> incrementable_any;
    incrementable_any y = dynamic_any_cast<incrementable_any>(x);
    ++y;
    BOOST_CHECK_EQUAL(any_cast<int>(y), 2);
}

// Casting to a value only requires the target to provide
// a copy constructor.
BOOST_AUTO_TEST_CASE(test_add_copy)
{
    any< ::boost::mpl::vector<destructible<>, typeid_<> > > x(1);
    any<common<> > y = dynamic_any_cast<any<common<> > >(x);
    BOOST_CHECK_EQUAL(any_cast<int>(y), 1);
}

template<class T, class U>
struct choose_second
{
    typedef U type;
};

BOOST_AUTO_TEST_CASE(test_deduced)
{
    typedef deduced<choose_second<_self, int> > _p2;
    any< ::boost::mpl::vector<common<>, common<_p2> > > x(1);
    typedef ::boost::mpl::vector<common<>, common<_p2>, incrementable<_p2>, addable<_self, _self, _p2> > dest_concept;
    any<dest_concept> y = dynamic_any_cast<any<dest_concept> >(x);
    any<dest_concept, _p2> z = y + y;
    ++z;
    BOOST_CHECK_EQUAL(any_cast<int>(z), 3);
}

BOOST_AUTO_TEST_CASE(test_multiple_placeholders)
{
    typedef ::boost::mpl::map< ::boost::mpl::pair<_a, int>, boost::mpl::pair<_b, int> > init_map;
    any< ::boost::mpl::vector<common<_a>, common<_b> >, _a> x(1, make_binding<init_map>());
    typedef ::boost::mpl::vector<common<_a>, common<_b>, incrementable<_b>, addable<_a, _a, _b> > dest_concept;
    typedef ::boost::mpl::map< ::boost::mpl::pair<_a, _a>, ::boost::mpl::pair<_b, _b> > placeholder_map;
    any<dest_concept, _a> y = dynamic_any_cast<any<dest_concept, _a> >(x, make_binding<placeholder_map>());
    any<dest_concept, _b> z = y + y;
    ++z;
    BOOST_CHECK_EQUAL(any_cast<int>(z), 3);
}

BOOST_AUTO_TEST_CASE(test_multiple_placeholders_switch)
{
    typedef ::boost::mpl::map< ::boost::mpl::pair<_a, int>, boost::mpl::pair<_b, int> > init_map;
    any< ::boost::mpl::vector<common<_a>, common<_b> >, _a> x(1, make_binding<init_map>());
    typedef ::boost::mpl::vector<common<_c>, common<_d>, incrementable<_d>, addable<_c, _c, _d> > dest_concept;
    typedef ::boost::mpl::map< ::boost::mpl::pair<_c, _a>, ::boost::mpl::pair<_d, _b> > placeholder_map;
    any<dest_concept, _c> y = dynamic_any_cast<any<dest_concept, _c> >(x, make_binding<placeholder_map>());
    any<dest_concept, _d> z = y + y;
    ++z;
    BOOST_CHECK_EQUAL(any_cast<int>(z), 3);
}

template<class T>
T as_rvalue(const T& arg) { return arg; }
template<class T>
const T& as_const(const T& arg) { return arg; }

BOOST_AUTO_TEST_CASE(test_val)
{
    any<common<> > x(1);
    // value
    any<common<> > y1 = dynamic_any_cast<any<common<> > >(x);
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(y1));
    any<common<> > y2 = dynamic_any_cast<any<common<> > >(as_rvalue(x));
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(y2));
    any<common<> > y3 = dynamic_any_cast<any<common<> > >(as_const(x));
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(y3));

    // lvalue reference
    any<common<>, _self&> r(x);
    any<common<> > z1 = dynamic_any_cast<any<common<> > >(r);
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(z1));
    any<common<> > z2 = dynamic_any_cast<any<common<> > >(as_rvalue(r));
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(z2));
    any<common<> > z3 = dynamic_any_cast<any<common<> > >(as_const(r));
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(z3));

    // const reference
    any<common<>, const _self&> cr(x);
    any<common<> > w1 = dynamic_any_cast<any<common<> > >(cr);
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(w1));
    any<common<> > w2 = dynamic_any_cast<any<common<> > >(as_rvalue(cr));
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(w2));
    any<common<> > w3 = dynamic_any_cast<any<common<> > >(as_const(cr));
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(w3));

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    // rvalue reference
    any<common<>, _self&&> rr(std::move(x));
    any<common<> > v1 = dynamic_any_cast<any<common<> > >(rr);
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(v1));
    any<common<> > v2 = dynamic_any_cast<any<common<> > >(as_rvalue(rr));
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(v2));
    any<common<> > v3 = dynamic_any_cast<any<common<> > >(as_const(rr));
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(v3));
#endif
}

BOOST_AUTO_TEST_CASE(test_ref)
{
    // A non-const reference can only bind to a few cases
    any<common<> > x(1);
    any<common<>, _self&> y = dynamic_any_cast<any<common<>, _self&> >(x);
    BOOST_CHECK_EQUAL(any_cast<int*>(&x), any_cast<int*>(&y));

    any<common<>, _self&> z = dynamic_any_cast<any<common<>, _self&> >(y);
    BOOST_CHECK_EQUAL(any_cast<int*>(&x), any_cast<int*>(&z));
    any<common<>, _self&> w = dynamic_any_cast<any<common<>, _self&> >(as_rvalue(y));
    BOOST_CHECK_EQUAL(any_cast<int*>(&x), any_cast<int*>(&w));
    any<common<>, _self&> v = dynamic_any_cast<any<common<>, _self&> >(as_const(y));
    BOOST_CHECK_EQUAL(any_cast<int*>(&x), any_cast<int*>(&v));
}

BOOST_AUTO_TEST_CASE(test_cref)
{
    any<common<> > x(1);
    typedef any<common<>, const _self&> dest_type;

    // value
    dest_type y1 = dynamic_any_cast<dest_type>(x);
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&y1));
    // as_rvalue creates a temporary
    BOOST_CHECK_EQUAL(any_cast<int>(x), any_cast<int>(dynamic_any_cast<dest_type>(as_rvalue(x))));
    dest_type y3 = dynamic_any_cast<dest_type>(as_const(x));
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&y3));

    // lvalue reference
    any<common<>, _self&> r(x);
    dest_type z1 = dynamic_any_cast<dest_type>(r);
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&z1));
    dest_type z2 = dynamic_any_cast<dest_type>(as_rvalue(r));
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&z2));
    dest_type z3 = dynamic_any_cast<dest_type>(as_const(r));
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&z3));

    // const reference
    any<common<>, const _self&> cr(x);
    dest_type w1 = dynamic_any_cast<dest_type>(cr);
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&w1));
    dest_type w2 = dynamic_any_cast<dest_type>(as_rvalue(cr));
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&w2));
    dest_type w3 = dynamic_any_cast<dest_type>(as_const(cr));
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&w3));

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    // rvalue reference
    any<common<>, _self&&> rr(std::move(x));
    dest_type v1 = dynamic_any_cast<dest_type>(rr);
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&v1));
    dest_type v2 = dynamic_any_cast<dest_type>(as_rvalue(rr));
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&v2));
    dest_type v3 = dynamic_any_cast<dest_type>(as_const(rr));
    BOOST_CHECK_EQUAL(any_cast<const int*>(&x), any_cast<const int*>(&v3));
#endif
}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

BOOST_AUTO_TEST_CASE(test_rref)
{
    any<common<> > x(1);
    typedef any<common<>, _self&&> dest_type;

    // value
    dest_type y2 = dynamic_any_cast<dest_type>(std::move(x));
    BOOST_CHECK_EQUAL(any_cast<int*>(&x), any_cast<int*>(&y2));

    // rvalue reference
    any<common<>, _self&&> rr(std::move(x));
    dest_type v2 = dynamic_any_cast<dest_type>(std::move(rr));
    BOOST_CHECK_EQUAL(any_cast<int*>(&x), any_cast<int*>(&v2));
}

#endif
