// Boost.TypeErasure library
//
// Copyright 2012 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// $Id$

#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/member.hpp>
#include <boost/mpl/vector.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

using namespace boost::type_erasure;

BOOST_TYPE_ERASURE_MEMBER((ns)(ns2)(has_fun), fun, 0)

struct model {
    explicit model(int v) : val(v) {}
    int f1() { return val; }
    int f1(int i) { return val + i; }
    int val;
};

BOOST_TYPE_ERASURE_MEMBER((global_has_f1_0), f1, 0)

BOOST_AUTO_TEST_CASE(test_global_has_f1_0) {
    typedef ::boost::mpl::vector<
        global_has_f1_0<int()>,
        copy_constructible<> > concept_type;
    model m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
}

BOOST_AUTO_TEST_CASE(test_global_has_f1_0_ref) {
    typedef ::boost::mpl::vector<
        global_has_f1_0<int()>,
        copy_constructible<> > concept_type;
    model m(10);
    const any<concept_type, _self&> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
}

BOOST_TYPE_ERASURE_MEMBER((ns1)(ns2)(ns_has_f1_0), f1, 0)

BOOST_AUTO_TEST_CASE(test_ns_has_f1_0) {
    typedef ::boost::mpl::vector<
        ns1::ns2::ns_has_f1_0<int()>,
        copy_constructible<> > concept_type;
    model m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
}

struct model_const {
    explicit model_const(int v) : val(v) {}
    int f1() const { return val; }
    int f1(int i) const { return val + i; }
    int val;
};

BOOST_AUTO_TEST_CASE(test_global_has_f1_0_const) {
    typedef ::boost::mpl::vector<
        ns1::ns2::ns_has_f1_0<int(), const _self>,
        copy_constructible<> > concept_type;
    model_const m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
}

BOOST_AUTO_TEST_CASE(test_global_has_f1_0_const_ref) {
    typedef ::boost::mpl::vector<
        ns1::ns2::ns_has_f1_0<int(), const _self>,
        copy_constructible<> > concept_type;
    model_const m(10);
    const any<concept_type, _self&> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
}

BOOST_AUTO_TEST_CASE(test_global_has_f1_0_const_cref) {
    typedef ::boost::mpl::vector<
        ns1::ns2::ns_has_f1_0<int(), const _self>,
        copy_constructible<> > concept_type;
    model_const m(10);
    const any<concept_type, const _self&> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
}

BOOST_AUTO_TEST_CASE(test_global_has_f1_0_void) {
    typedef ::boost::mpl::vector<
        global_has_f1_0<void()>,
        copy_constructible<> > concept_type;
    model m(10);
    any<concept_type> x(m);
    x.f1();
}

BOOST_TYPE_ERASURE_MEMBER((global_has_f1_1), f1, 1)

BOOST_AUTO_TEST_CASE(test_global_has_f1_overload) {
    typedef ::boost::mpl::vector<
        global_has_f1_0<int()>,
        global_has_f1_1<int(int)>,
        copy_constructible<> > concept_type;
    model m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
    BOOST_CHECK_EQUAL(x.f1(5), 15);
}

BOOST_AUTO_TEST_CASE(test_global_has_f1_overload_const) {
    typedef ::boost::mpl::vector<
        global_has_f1_0<int(), const _self>,
        global_has_f1_1<int(int), const _self>,
        copy_constructible<> > concept_type;
    model_const m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
    BOOST_CHECK_EQUAL(x.f1(5), 15);
}

struct model_overload_const_non_const {
    int f1() { return 1; }
    int f1() const { return 2; }
};

BOOST_AUTO_TEST_CASE(test_global_has_f1_overload_const_non_const) {
    typedef ::boost::mpl::vector<
        global_has_f1_0<int(), _self>,
        global_has_f1_0<int(), const _self>,
        copy_constructible<> > concept_type;
    model_overload_const_non_const m;
    any<concept_type> x1(m);
    BOOST_CHECK_EQUAL(x1.f1(), 1);
    const any<concept_type> x2(m);
    BOOST_CHECK_EQUAL(x2.f1(), 2);
}

BOOST_AUTO_TEST_CASE(test_global_has_f1_overload_const_non_const_ref) {
    typedef ::boost::mpl::vector<
        global_has_f1_0<int(), const _self>, // FIXME: This is order sensitive
        global_has_f1_0<int(), _self>,
        copy_constructible<> > concept_type;
    model_overload_const_non_const m;
    any<concept_type, _self&> x1(m);
    BOOST_CHECK_EQUAL(x1.f1(), 1);
    const any<concept_type, _self&> x2(m);
    BOOST_CHECK_EQUAL(x2.f1(), 1);
}

BOOST_AUTO_TEST_CASE(test_global_has_f1_overload_const_non_const_cref) {
    typedef ::boost::mpl::vector<
        global_has_f1_0<int(), _self>,
        global_has_f1_0<int(), const _self>,
        copy_constructible<> > concept_type;
    model_overload_const_non_const m;
    any<concept_type, const _self&> x1(m);
    BOOST_CHECK_EQUAL(x1.f1(), 2);
    const any<concept_type, const _self&> x2(m);
    BOOST_CHECK_EQUAL(x2.f1(), 2);
}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

BOOST_AUTO_TEST_CASE(test_global_has_f1_rv) {
    typedef ::boost::mpl::vector<
        global_has_f1_1<int(int&&)>,
        copy_constructible<> > concept_type;
    model m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(5), 15);
}

BOOST_AUTO_TEST_CASE(test_global_has_f1_rv_const) {
    typedef ::boost::mpl::vector<
        global_has_f1_1<int(int&&), const _self>,
        copy_constructible<> > concept_type;
    model_const m(10);
    const any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(5), 15);
}

#endif


#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES) && \
    !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && \
    !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && \
    !defined(BOOST_NO_CXX11_DECLTYPE) && \
    !BOOST_WORKAROUND(BOOST_MSVC, == 1800)

namespace ns3 {
BOOST_TYPE_ERASURE_MEMBER(f1);
}

BOOST_AUTO_TEST_CASE(test_simple_member_0) {
    typedef ::boost::mpl::vector<
        ns3::has_f1<int()>,
        copy_constructible<> > concept_type;
    model m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(), 10);
}

BOOST_AUTO_TEST_CASE(test_simple_member_1) {
    typedef ::boost::mpl::vector<
        ns3::has_f1<int(int)>,
        copy_constructible<> > concept_type;
    model m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(5), 15);
}

namespace ns3 {
BOOST_TYPE_ERASURE_MEMBER(has_f1_ex, f1);
}

BOOST_AUTO_TEST_CASE(test_named_member_1) {
    typedef ::boost::mpl::vector<
        ns3::has_f1_ex<int(int)>,
        copy_constructible<> > concept_type;
    model m(10);
    any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(5), 15);
}

BOOST_AUTO_TEST_CASE(test_named_member_1_const) {
    typedef ::boost::mpl::vector<
        ns3::has_f1_ex<int(int) const>,
        copy_constructible<> > concept_type;
    model_const m(10);
    const any<concept_type> x(m);
    BOOST_CHECK_EQUAL(x.f1(5), 15);
}

#endif
