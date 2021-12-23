// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/result.hpp>
#   include <boost/leaf/capture.hpp>
#   include <boost/leaf/handle_errors.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

struct val
{
    int id;

    friend bool operator==( val const & a, val const & b )
    {
        return a.id==b.id;
    }

    friend std::ostream & operator<<( std::ostream & os, val const & v )
    {
        return os << v.id;
    }
};

struct base
{
};

struct derived: base
{
};

int main()
{
    {
        leaf::result<val const> r1, r2;
        leaf::result<val const> & ref = r1;
        leaf::result<val const> const & cref = r1;
        leaf::result<val const> && rvref = std::move(r1);
        leaf::result<val const> const && rvcref = std::move(r2);

        static_assert(std::is_same<decltype(ref.value()), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(cref.value()), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(rvref.value())), val const &&>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(rvcref.value())), val const &&>::value, "result type deduction bug");

        static_assert(std::is_same<decltype(*ref), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(*cref), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(*rvref)), val const &&>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(*rvcref)), val const &&>::value, "result type deduction bug");

        auto & ref_id = ref->id; static_assert(std::is_same<decltype(ref_id), int const &>::value, "result type deduction bug");
        auto & cref_id = cref->id; static_assert(std::is_same<decltype(cref_id), int const &>::value, "result type deduction bug");
        auto & rvref_id = rvref->id; static_assert(std::is_same<decltype(rvref_id), int const &>::value, "result type deduction bug");
        auto & rvcref_id = rvcref->id; static_assert(std::is_same<decltype(rvcref_id), int const &>::value, "result type deduction bug");
    }

    {
        leaf::result<val> r1, r2;
        leaf::result<val> & ref = r1;
        leaf::result<val> const & cref = r1;
        leaf::result<val> && rvref = std::move(r1);
        leaf::result<val> const && rvcref = std::move(r2);

        static_assert(std::is_same<decltype(ref.value()), val &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(cref.value()), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(rvref.value())), val &&>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(rvcref.value())), val const &&>::value, "result type deduction bug");

        static_assert(std::is_same<decltype(*ref), val &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(*cref), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(*rvref)), val &&>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(*rvcref)), val const &&>::value, "result type deduction bug");

        auto & ref_id = ref->id; static_assert(std::is_same<decltype(ref_id), int &>::value, "result type deduction bug");
        auto & cref_id = cref->id; static_assert(std::is_same<decltype(cref_id), int const &>::value, "result type deduction bug");
        auto & rvref_id = rvref->id; static_assert(std::is_same<decltype(rvref_id), int &>::value, "result type deduction bug");
        auto & rvcref_id = rvcref->id; static_assert(std::is_same<decltype(rvcref_id), int const &>::value, "result type deduction bug");
    }

    {
        val v;
        leaf::result<val const &> r1(v), r2(v);
        leaf::result<val const &> & ref = r1;
        leaf::result<val const &> const & cref = r1;
        leaf::result<val const &> && rvref = std::move(r1);
        leaf::result<val const &> const && rvcref = std::move(r2);

        static_assert(std::is_same<decltype(ref.value()), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(cref.value()), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(rvref.value())), val const &&>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(rvcref.value())), val const &&>::value, "result type deduction bug");

        static_assert(std::is_same<decltype(*ref), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(*cref), val const &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(*rvref)), val const &&>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(*rvcref)), val const &&>::value, "result type deduction bug");

        auto & ref_id = ref->id; static_assert(std::is_same<decltype(ref_id), int const &>::value, "result type deduction bug");
        auto & cref_id = cref->id; static_assert(std::is_same<decltype(cref_id), int const &>::value, "result type deduction bug");
        auto & rvref_id = rvref->id; static_assert(std::is_same<decltype(rvref_id), int const &>::value, "result type deduction bug");
        auto & rvcref_id = rvcref->id; static_assert(std::is_same<decltype(rvcref_id), int const &>::value, "result type deduction bug");
    }

    {
        val v;
        leaf::result<val &> r1(v), r2(v);
        leaf::result<val &> & ref = r1;
        leaf::result<val &> const & cref = r1;
        leaf::result<val &> && rvref = std::move(r1);
        leaf::result<val &> const && rvcref = std::move(r2);

        static_assert(std::is_same<decltype(ref.value()), val &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(cref.value()), val &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(rvref.value())), val &&>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(rvcref.value())), val &&>::value, "result type deduction bug");

        static_assert(std::is_same<decltype(*ref), val &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(*cref), val &>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(*rvref)), val &&>::value, "result type deduction bug");
        static_assert(std::is_same<decltype(std::move(*rvcref)), val &&>::value, "result type deduction bug");

        auto & ref_id = ref->id; static_assert(std::is_same<decltype(ref_id), int &>::value, "result type deduction bug");
        auto & cref_id = cref->id; static_assert(std::is_same<decltype(cref_id), int &>::value, "result type deduction bug");
        auto & rvref_id = rvref->id; static_assert(std::is_same<decltype(rvref_id), int &>::value, "result type deduction bug");
        auto & rvcref_id = rvcref->id; static_assert(std::is_same<decltype(rvcref_id), int &>::value, "result type deduction bug");
    }

    // Mutable:

    {
        val x = { 42 };
        leaf::result<val const &> r(x);
        BOOST_TEST(r);
        val a = r.value();
        BOOST_TEST_EQ(a, x);
        val b = *r;
        BOOST_TEST_EQ(b, x);
    }
    {
        val x = { 42 };
        leaf::result<val const &> r(x);
        BOOST_TEST(r);
        val const & a = r.value();
        BOOST_TEST_EQ(&a, &x);
        val const & b = *r;
        BOOST_TEST_EQ(&b, &x);
    }
    {
        val x = { 42 };
        leaf::result<val const &> r(x);
        BOOST_TEST(r);
        auto & a = r.value();
        BOOST_TEST_EQ(&a, &x);
        auto & b = *r;
        BOOST_TEST_EQ(&b, &x);
    }

    {
        val x = { 42 };
        leaf::result<val &> r(x);
        BOOST_TEST(r);
        val a = r.value();
        BOOST_TEST_EQ(a, x);
        val b = *r;
        BOOST_TEST_EQ(b, x);
        int id = x.id;
        BOOST_TEST_EQ(id+1, ++r->id);
    }
    {
        val x = { 42 };
        leaf::result<val &> r(x);
        BOOST_TEST(r);
        val & a = r.value();
        BOOST_TEST_EQ(&a, &x);
        val & b = *r;
        BOOST_TEST_EQ(&b, &x);
        int id = x.id;
        BOOST_TEST_EQ(id+1, ++r->id);
    }
    {
        val x = { 42 };
        leaf::result<val &> r(x);
        BOOST_TEST(r);
        auto & a = r.value();
        BOOST_TEST_EQ(&a, &x);
        auto & b = *r;
        BOOST_TEST_EQ(&b, &x);
        int id = x.id;
        BOOST_TEST_EQ(id+1, ++r->id);
    }

    // Const:

    {
        val x = { 42 };
        leaf::result<val const &> const r(x);
        BOOST_TEST(r);
        val a = r.value();
        BOOST_TEST_EQ(a, x);
        val b = *r;
        BOOST_TEST_EQ(b, x);
    }
    {
        val x = { 42 };
        leaf::result<val const &> const r(x);
        BOOST_TEST(r);
        val const & a = r.value();
        BOOST_TEST_EQ(&a, &x);
        val const & b = *r;
        BOOST_TEST_EQ(&b, &x);
    }
    {
        val x = { 42 };
        leaf::result<val const &> const r(x);
        BOOST_TEST(r);
        auto & a = r.value();
        BOOST_TEST_EQ(&a, &x);
        auto & b = *r;
        BOOST_TEST_EQ(&b, &x);
    }

    {
        val x = { 42 };
        leaf::result<val &> const r(x);
        BOOST_TEST(r);
        val a = r.value();
        BOOST_TEST_EQ(a, x);
        val b = *r;
        BOOST_TEST_EQ(b, x);
        int id = x.id;
        BOOST_TEST_EQ(id+1, ++r->id);
    }
    {
        val x = { 42 };
        leaf::result<val &> const r(x);
        BOOST_TEST(r);
        val & a = r.value();
        BOOST_TEST_EQ(&a, &x);
        val & b = *r;
        BOOST_TEST_EQ(&b, &x);
    }
    {
        val x = { 42 };
        leaf::result<val &> const r(x);
        BOOST_TEST(r);
        auto & a = r.value();
        BOOST_TEST_EQ(&a, &x);
        auto & b = *r;
        BOOST_TEST_EQ(&b, &x);
        int id = x.id;
        BOOST_TEST_EQ(id+1, ++r->id);
    }

    // Hierarchy

    {
        derived d;
        leaf::result<base &> r = d;
        BOOST_TEST_EQ(&r.value(), &d);
        BOOST_TEST_EQ(&*r, &d);
        BOOST_TEST_EQ(r.operator->(), &d);
    }

    {
        derived d;
        leaf::result<base *> r = &d;
        BOOST_TEST_EQ(r.value(), &d);
        BOOST_TEST_EQ(*r, &d);
        BOOST_TEST_EQ(*r.operator->(), &d);
    }

    return boost::report_errors();
}
