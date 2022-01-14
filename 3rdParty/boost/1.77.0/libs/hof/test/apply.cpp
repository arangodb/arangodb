/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    apply.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/apply.hpp>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{    
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::apply(binary_class(), 1, 2) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::apply(binary_class(), 1, 2) == 3);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::apply(boost::hof::apply, binary_class(), 1, 2) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::apply(boost::hof::apply, binary_class(), 1, 2) == 3);
}

struct member_sum_f
{
    int i;
    constexpr member_sum_f(int x) : i(x)
    {}

    constexpr int add(int x) const
    {
        return i+x;
    }
};

struct member_sum_f_derived
: member_sum_f
{
    constexpr member_sum_f_derived(int x) : member_sum_f(x)
    {}
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::apply(&member_sum_f::add, member_sum_f(1), 2) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::apply(&member_sum_f::add, member_sum_f_derived(1), 2) == 3);

#ifdef __clang__
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::apply(&member_sum_f::add, member_sum_f(1), 2) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::apply(&member_sum_f::add, member_sum_f_derived(1), 2) == 3);
#endif

    static_assert(std::is_base_of<member_sum_f, member_sum_f>::value, "Base of failed");
    std::unique_ptr<member_sum_f> msp(new member_sum_f(1));
    BOOST_HOF_TEST_CHECK(boost::hof::apply(&member_sum_f::add, msp, 2) == 3);

    std::unique_ptr<member_sum_f_derived> mspd(new member_sum_f_derived(1));
    BOOST_HOF_TEST_CHECK(boost::hof::apply(&member_sum_f::add, mspd, 2) == 3);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::apply(&member_sum_f::i, member_sum_f(3)) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::apply(&member_sum_f::i, member_sum_f_derived(3)) == 3);

#ifdef __clang__
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::apply(&member_sum_f::i, member_sum_f(3)) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::apply(&member_sum_f::i, member_sum_f_derived(3)) == 3);
#endif

    std::unique_ptr<member_sum_f> msp(new member_sum_f(3));
    BOOST_HOF_TEST_CHECK(boost::hof::apply(&member_sum_f::i, msp) == 3);

    std::unique_ptr<member_sum_f_derived> mspd(new member_sum_f_derived(3));
    BOOST_HOF_TEST_CHECK(boost::hof::apply(&member_sum_f::i, mspd) == 3);
}


struct mem_hash
{
    mutable unsigned int hash;

    mem_hash(): hash(0) {}

    int f0() { f1(17); return 0; }
    int g0() const { g1(17); return 0; }

    int f1(int a1) { hash = (hash * 17041 + a1) % 32768; return 0; }
    int g1(int a1) const { hash = (hash * 17041 + a1 * 2) % 32768; return 0; }

    int f2(int a1, int a2) { f1(a1); f1(a2); return 0; }
    int g2(int a1, int a2) const { g1(a1); g1(a2); return 0; }

    int f3(int a1, int a2, int a3) { f2(a1, a2); f1(a3); return 0; }
    int g3(int a1, int a2, int a3) const { g2(a1, a2); g1(a3); return 0; }

    int f4(int a1, int a2, int a3, int a4) { f3(a1, a2, a3); f1(a4); return 0; }
    int g4(int a1, int a2, int a3, int a4) const { g3(a1, a2, a3); g1(a4); return 0; }

    int f5(int a1, int a2, int a3, int a4, int a5) { f4(a1, a2, a3, a4); f1(a5); return 0; }
    int g5(int a1, int a2, int a3, int a4, int a5) const { g4(a1, a2, a3, a4); g1(a5); return 0; }

    int f6(int a1, int a2, int a3, int a4, int a5, int a6) { f5(a1, a2, a3, a4, a5); f1(a6); return 0; }
    int g6(int a1, int a2, int a3, int a4, int a5, int a6) const { g5(a1, a2, a3, a4, a5); g1(a6); return 0; }

    int f7(int a1, int a2, int a3, int a4, int a5, int a6, int a7) { f6(a1, a2, a3, a4, a5, a6); f1(a7); return 0; }
    int g7(int a1, int a2, int a3, int a4, int a5, int a6, int a7) const { g6(a1, a2, a3, a4, a5, a6); g1(a7); return 0; }

    int f8(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8) { f7(a1, a2, a3, a4, a5, a6, a7); f1(a8); return 0; }
    int g8(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8) const { g7(a1, a2, a3, a4, a5, a6, a7); g1(a8); return 0; }
};

BOOST_HOF_TEST_CASE()
{

    mem_hash x;

    mem_hash const & rcx = x;
    mem_hash const * pcx = &x;

    std::shared_ptr<mem_hash> sp(new mem_hash);

    boost::hof::apply(&mem_hash::f0, x);
    boost::hof::apply(&mem_hash::f0, &x);
    boost::hof::apply(&mem_hash::f0, sp);

    boost::hof::apply(&mem_hash::g0, x);
    boost::hof::apply(&mem_hash::g0, rcx);
    boost::hof::apply(&mem_hash::g0, &x);
    boost::hof::apply(&mem_hash::g0, pcx);
    boost::hof::apply(&mem_hash::g0, sp);

    boost::hof::apply(&mem_hash::f1, x, 1);
    boost::hof::apply(&mem_hash::f1, &x, 1);
    boost::hof::apply(&mem_hash::f1, sp, 1);

    boost::hof::apply(&mem_hash::g1, x, 1);
    boost::hof::apply(&mem_hash::g1, rcx, 1);
    boost::hof::apply(&mem_hash::g1, &x, 1);
    boost::hof::apply(&mem_hash::g1, pcx, 1);
    boost::hof::apply(&mem_hash::g1, sp, 1);

    boost::hof::apply(&mem_hash::f2, x, 1, 2);
    boost::hof::apply(&mem_hash::f2, &x, 1, 2);
    boost::hof::apply(&mem_hash::f2, sp, 1, 2);

    boost::hof::apply(&mem_hash::g2, x, 1, 2);
    boost::hof::apply(&mem_hash::g2, rcx, 1, 2);
    boost::hof::apply(&mem_hash::g2, &x, 1, 2);
    boost::hof::apply(&mem_hash::g2, pcx, 1, 2);
    boost::hof::apply(&mem_hash::g2, sp, 1, 2);

    boost::hof::apply(&mem_hash::f3, x, 1, 2, 3);
    boost::hof::apply(&mem_hash::f3, &x, 1, 2, 3);
    boost::hof::apply(&mem_hash::f3, sp, 1, 2, 3);

    boost::hof::apply(&mem_hash::g3, x, 1, 2, 3);
    boost::hof::apply(&mem_hash::g3, rcx, 1, 2, 3);
    boost::hof::apply(&mem_hash::g3, &x, 1, 2, 3);
    boost::hof::apply(&mem_hash::g3, pcx, 1, 2, 3);
    boost::hof::apply(&mem_hash::g3, sp, 1, 2, 3);

    boost::hof::apply(&mem_hash::f4, x, 1, 2, 3, 4);
    boost::hof::apply(&mem_hash::f4, &x, 1, 2, 3, 4);
    boost::hof::apply(&mem_hash::f4, sp, 1, 2, 3, 4);

    boost::hof::apply(&mem_hash::g4, x, 1, 2, 3, 4);
    boost::hof::apply(&mem_hash::g4, rcx, 1, 2, 3, 4);
    boost::hof::apply(&mem_hash::g4, &x, 1, 2, 3, 4);
    boost::hof::apply(&mem_hash::g4, pcx, 1, 2, 3, 4);
    boost::hof::apply(&mem_hash::g4, sp, 1, 2, 3, 4);

    boost::hof::apply(&mem_hash::f5, x, 1, 2, 3, 4, 5);
    boost::hof::apply(&mem_hash::f5, &x, 1, 2, 3, 4, 5);
    boost::hof::apply(&mem_hash::f5, sp, 1, 2, 3, 4, 5);

    boost::hof::apply(&mem_hash::g5, x, 1, 2, 3, 4, 5);
    boost::hof::apply(&mem_hash::g5, rcx, 1, 2, 3, 4, 5);
    boost::hof::apply(&mem_hash::g5, &x, 1, 2, 3, 4, 5);
    boost::hof::apply(&mem_hash::g5, pcx, 1, 2, 3, 4, 5);
    boost::hof::apply(&mem_hash::g5, sp, 1, 2, 3, 4, 5);

    boost::hof::apply(&mem_hash::f6, x, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&mem_hash::f6, &x, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&mem_hash::f6, sp, 1, 2, 3, 4, 5, 6);

    boost::hof::apply(&mem_hash::g6, x, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&mem_hash::g6, rcx, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&mem_hash::g6, &x, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&mem_hash::g6, pcx, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&mem_hash::g6, sp, 1, 2, 3, 4, 5, 6);

    boost::hof::apply(&mem_hash::f7, x, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&mem_hash::f7, &x, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&mem_hash::f7, sp, 1, 2, 3, 4, 5, 6, 7);

    boost::hof::apply(&mem_hash::g7, x, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&mem_hash::g7, rcx, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&mem_hash::g7, &x, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&mem_hash::g7, pcx, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&mem_hash::g7, sp, 1, 2, 3, 4, 5, 6, 7);

    boost::hof::apply(&mem_hash::f8, x, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&mem_hash::f8, &x, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&mem_hash::f8, sp, 1, 2, 3, 4, 5, 6, 7, 8);

    boost::hof::apply(&mem_hash::g8, x, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&mem_hash::g8, rcx, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&mem_hash::g8, &x, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&mem_hash::g8, pcx, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&mem_hash::g8, sp, 1, 2, 3, 4, 5, 6, 7, 8);

    BOOST_HOF_TEST_CHECK(boost::hof::apply(&mem_hash::hash, x) == 17610 && boost::hof::apply(&mem_hash::hash, sp) == 2155);
}

struct hash_base
{
    mutable unsigned int hash;

    hash_base(): hash(0) {}

    int f0() { f1(17); return 0; }
    int g0() const { g1(17); return 0; }

    int f1(int a1) { hash = (hash * 17041 + a1) % 32768; return 0; }
    int g1(int a1) const { hash = (hash * 17041 + a1 * 2) % 32768; return 0; }

    int f2(int a1, int a2) { f1(a1); f1(a2); return 0; }
    int g2(int a1, int a2) const { g1(a1); g1(a2); return 0; }

    int f3(int a1, int a2, int a3) { f2(a1, a2); f1(a3); return 0; }
    int g3(int a1, int a2, int a3) const { g2(a1, a2); g1(a3); return 0; }

    int f4(int a1, int a2, int a3, int a4) { f3(a1, a2, a3); f1(a4); return 0; }
    int g4(int a1, int a2, int a3, int a4) const { g3(a1, a2, a3); g1(a4); return 0; }

    int f5(int a1, int a2, int a3, int a4, int a5) { f4(a1, a2, a3, a4); f1(a5); return 0; }
    int g5(int a1, int a2, int a3, int a4, int a5) const { g4(a1, a2, a3, a4); g1(a5); return 0; }

    int f6(int a1, int a2, int a3, int a4, int a5, int a6) { f5(a1, a2, a3, a4, a5); f1(a6); return 0; }
    int g6(int a1, int a2, int a3, int a4, int a5, int a6) const { g5(a1, a2, a3, a4, a5); g1(a6); return 0; }

    int f7(int a1, int a2, int a3, int a4, int a5, int a6, int a7) { f6(a1, a2, a3, a4, a5, a6); f1(a7); return 0; }
    int g7(int a1, int a2, int a3, int a4, int a5, int a6, int a7) const { g6(a1, a2, a3, a4, a5, a6); g1(a7); return 0; }

    int f8(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8) { f7(a1, a2, a3, a4, a5, a6, a7); f1(a8); return 0; }
    int g8(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8) const { g7(a1, a2, a3, a4, a5, a6, a7); g1(a8); return 0; }
};

struct derived_hash: public hash_base
{
};

BOOST_HOF_TEST_CASE()
{

    derived_hash x;

    derived_hash const & rcx = x;
    derived_hash const * pcx = &x;

    std::shared_ptr<derived_hash> sp(new derived_hash);

    boost::hof::apply(&derived_hash::f0, x);
    boost::hof::apply(&derived_hash::f0, &x);
    boost::hof::apply(&derived_hash::f0, sp);

    boost::hof::apply(&derived_hash::g0, x);
    boost::hof::apply(&derived_hash::g0, rcx);
    boost::hof::apply(&derived_hash::g0, &x);
    boost::hof::apply(&derived_hash::g0, pcx);
    boost::hof::apply(&derived_hash::g0, sp);

    boost::hof::apply(&derived_hash::f1, x, 1);
    boost::hof::apply(&derived_hash::f1, &x, 1);
    boost::hof::apply(&derived_hash::f1, sp, 1);

    boost::hof::apply(&derived_hash::g1, x, 1);
    boost::hof::apply(&derived_hash::g1, rcx, 1);
    boost::hof::apply(&derived_hash::g1, &x, 1);
    boost::hof::apply(&derived_hash::g1, pcx, 1);
    boost::hof::apply(&derived_hash::g1, sp, 1);

    boost::hof::apply(&derived_hash::f2, x, 1, 2);
    boost::hof::apply(&derived_hash::f2, &x, 1, 2);
    boost::hof::apply(&derived_hash::f2, sp, 1, 2);

    boost::hof::apply(&derived_hash::g2, x, 1, 2);
    boost::hof::apply(&derived_hash::g2, rcx, 1, 2);
    boost::hof::apply(&derived_hash::g2, &x, 1, 2);
    boost::hof::apply(&derived_hash::g2, pcx, 1, 2);
    boost::hof::apply(&derived_hash::g2, sp, 1, 2);

    boost::hof::apply(&derived_hash::f3, x, 1, 2, 3);
    boost::hof::apply(&derived_hash::f3, &x, 1, 2, 3);
    boost::hof::apply(&derived_hash::f3, sp, 1, 2, 3);

    boost::hof::apply(&derived_hash::g3, x, 1, 2, 3);
    boost::hof::apply(&derived_hash::g3, rcx, 1, 2, 3);
    boost::hof::apply(&derived_hash::g3, &x, 1, 2, 3);
    boost::hof::apply(&derived_hash::g3, pcx, 1, 2, 3);
    boost::hof::apply(&derived_hash::g3, sp, 1, 2, 3);

    boost::hof::apply(&derived_hash::f4, x, 1, 2, 3, 4);
    boost::hof::apply(&derived_hash::f4, &x, 1, 2, 3, 4);
    boost::hof::apply(&derived_hash::f4, sp, 1, 2, 3, 4);

    boost::hof::apply(&derived_hash::g4, x, 1, 2, 3, 4);
    boost::hof::apply(&derived_hash::g4, rcx, 1, 2, 3, 4);
    boost::hof::apply(&derived_hash::g4, &x, 1, 2, 3, 4);
    boost::hof::apply(&derived_hash::g4, pcx, 1, 2, 3, 4);
    boost::hof::apply(&derived_hash::g4, sp, 1, 2, 3, 4);

    boost::hof::apply(&derived_hash::f5, x, 1, 2, 3, 4, 5);
    boost::hof::apply(&derived_hash::f5, &x, 1, 2, 3, 4, 5);
    boost::hof::apply(&derived_hash::f5, sp, 1, 2, 3, 4, 5);

    boost::hof::apply(&derived_hash::g5, x, 1, 2, 3, 4, 5);
    boost::hof::apply(&derived_hash::g5, rcx, 1, 2, 3, 4, 5);
    boost::hof::apply(&derived_hash::g5, &x, 1, 2, 3, 4, 5);
    boost::hof::apply(&derived_hash::g5, pcx, 1, 2, 3, 4, 5);
    boost::hof::apply(&derived_hash::g5, sp, 1, 2, 3, 4, 5);

    boost::hof::apply(&derived_hash::f6, x, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&derived_hash::f6, &x, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&derived_hash::f6, sp, 1, 2, 3, 4, 5, 6);

    boost::hof::apply(&derived_hash::g6, x, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&derived_hash::g6, rcx, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&derived_hash::g6, &x, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&derived_hash::g6, pcx, 1, 2, 3, 4, 5, 6);
    boost::hof::apply(&derived_hash::g6, sp, 1, 2, 3, 4, 5, 6);

    boost::hof::apply(&derived_hash::f7, x, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&derived_hash::f7, &x, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&derived_hash::f7, sp, 1, 2, 3, 4, 5, 6, 7);

    boost::hof::apply(&derived_hash::g7, x, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&derived_hash::g7, rcx, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&derived_hash::g7, &x, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&derived_hash::g7, pcx, 1, 2, 3, 4, 5, 6, 7);
    boost::hof::apply(&derived_hash::g7, sp, 1, 2, 3, 4, 5, 6, 7);

    boost::hof::apply(&derived_hash::f8, x, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&derived_hash::f8, &x, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&derived_hash::f8, sp, 1, 2, 3, 4, 5, 6, 7, 8);

    boost::hof::apply(&derived_hash::g8, x, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&derived_hash::g8, rcx, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&derived_hash::g8, &x, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&derived_hash::g8, pcx, 1, 2, 3, 4, 5, 6, 7, 8);
    boost::hof::apply(&derived_hash::g8, sp, 1, 2, 3, 4, 5, 6, 7, 8);

    BOOST_HOF_TEST_CHECK(boost::hof::apply(&derived_hash::hash, x) == 17610 && boost::hof::apply(&derived_hash::hash, sp) == 2155);
}

struct dm_t
{
    int m;
};

BOOST_HOF_TEST_CASE()
{
    dm_t x = { 0 };

    boost::hof::apply( &dm_t::m, x ) = 401;

    BOOST_HOF_TEST_CHECK( x.m == 401 );
    BOOST_HOF_TEST_CHECK( boost::hof::apply( &dm_t::m, x ) == 401 );

    boost::hof::apply( &dm_t::m, &x ) = 502;

    BOOST_HOF_TEST_CHECK( x.m == 502 );
    BOOST_HOF_TEST_CHECK( boost::hof::apply( &dm_t::m, &x ) == 502 );

    dm_t * px = &x;

    boost::hof::apply( &dm_t::m, px ) = 603;

    BOOST_HOF_TEST_CHECK( x.m == 603 );
    BOOST_HOF_TEST_CHECK( boost::hof::apply( &dm_t::m, px ) == 603 );

    dm_t const & cx = x;
    dm_t const * pcx = &x;

    BOOST_HOF_TEST_CHECK( boost::hof::apply( &dm_t::m, cx ) == 603 );
    BOOST_HOF_TEST_CHECK( boost::hof::apply( &dm_t::m, pcx ) == 603 );

}


struct X_ref
{
    int f()
    {
        return 1;
    }

    int g() const
    {
        return 2;
    }
};

BOOST_HOF_TEST_CASE()
{
    X_ref x;

    BOOST_HOF_TEST_CHECK( boost::hof::apply( &X_ref::f, std::ref( x ) ) == 1 );
    BOOST_HOF_TEST_CHECK( boost::hof::apply( &X_ref::g, std::cref( x ) ) == 2 );
}
#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
struct copy_throws 
{
    copy_throws() {}
    copy_throws(copy_throws const&) {}
    copy_throws(copy_throws&&) noexcept {}
};

struct no_throw_fo 
{
    void operator()() noexcept {}
    void operator()(copy_throws) noexcept {}
};

struct throws_fo 
{
    void operator()() {}
};

struct member_obj 
{
    int x;
};
// Only newer versions of gcc support deducing noexcept for member function pointers
#if defined(__GNUC__) && !defined (__clang__) && ((__GNUC__ == 4 && __GNUC_MINOR__ > 8) || (__GNUC__ > 4))
struct no_throw_member_fun 
{
    void foo_nullary() noexcept {}
    void foo_unary(copy_throws) noexcept {}
};
BOOST_HOF_TEST_CASE()
{
    no_throw_member_fun obj;
    copy_throws arg;
    static_assert(noexcept(boost::hof::apply(&no_throw_member_fun::foo_nullary, obj)), "");
    static_assert(!noexcept(boost::hof::apply(&no_throw_member_fun::foo_unary, obj, arg)), "");
    static_assert(noexcept(boost::hof::apply(&no_throw_member_fun::foo_unary, obj, std::move(arg))), "");
}
#endif
BOOST_HOF_TEST_CASE()
{
    no_throw_fo obj;
    copy_throws arg;
    static_assert(noexcept(boost::hof::apply(obj)), "");
    static_assert(!noexcept(boost::hof::apply(obj, arg)), "");
    static_assert(noexcept(boost::hof::apply(obj, std::move(arg))), "");
}
BOOST_HOF_TEST_CASE()
{
    throws_fo obj;
    static_assert(!noexcept(boost::hof::apply(obj)), "");
}
BOOST_HOF_TEST_CASE()
{
    member_obj obj{42};
    static_assert(noexcept(boost::hof::apply(&member_obj::x, obj)), "");
}
#endif
