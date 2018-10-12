/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    unpack.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/unpack.hpp>
#include <boost/hof/static.hpp>
#include <boost/hof/lambda.hpp>
#include "test.hpp"

#include <memory>

static constexpr boost::hof::static_<boost::hof::unpack_adaptor<unary_class> > unary_unpack = {};
static constexpr boost::hof::static_<boost::hof::unpack_adaptor<binary_class> > binary_unpack = {};

BOOST_HOF_STATIC_AUTO unary_unpack_constexpr = boost::hof::unpack_adaptor<unary_class>();
#if BOOST_HOF_HAS_CONSTEXPR_TUPLE
BOOST_HOF_STATIC_AUTO binary_unpack_constexpr = boost::hof::unpack_adaptor<binary_class>();
#endif

BOOST_HOF_STATIC_AUTO unary_unpack_reveal = boost::hof::reveal_adaptor<boost::hof::unpack_adaptor<unary_class>>();
BOOST_HOF_STATIC_AUTO binary_unpack_reveal = boost::hof::reveal_adaptor<boost::hof::unpack_adaptor<binary_class>>();

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::unpack(unary_class())(boost::hof::pack(3))), "noexcept unpack");
    static_assert(noexcept(unary_unpack(boost::hof::pack(3))), "noexcept unpack");
    static_assert(noexcept(binary_unpack(boost::hof::pack(3), boost::hof::pack(2))), "noexcept unpack");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(unary_class())(std::make_tuple(3)));
    BOOST_HOF_TEST_CHECK(3 == unary_unpack(std::make_tuple(3)));
    BOOST_HOF_TEST_CHECK(3 == unary_unpack_reveal(std::make_tuple(3)));
    int ifu = 3;
    BOOST_HOF_TEST_CHECK(3 == unary_unpack(std::tuple<int&>(ifu)));

#if BOOST_HOF_HAS_CONSTEXPR_TUPLE
    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::unpack(unary_class())(std::make_tuple(3)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == unary_unpack_constexpr(std::make_tuple(3)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == unary_unpack_reveal(std::make_tuple(3)));
#endif
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(unary_class())(boost::hof::pack(3)));
    BOOST_HOF_TEST_CHECK(3 == unary_unpack(boost::hof::pack(3)));
    BOOST_HOF_TEST_CHECK(3 == unary_unpack_reveal(boost::hof::pack(3)));
    int ifu = 3;
    BOOST_HOF_TEST_CHECK(3 == unary_unpack(boost::hof::pack_forward(ifu)));

    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::unpack(unary_class())(boost::hof::pack(3)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == unary_unpack_constexpr(boost::hof::pack(3)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == unary_unpack_reveal(boost::hof::pack(3)));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(1, 2)));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack(std::make_tuple(1, 2)));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(1, 2)));

    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(1), std::make_tuple(2)));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack(std::make_tuple(1), std::make_tuple(2)));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(1), std::make_tuple(2)));

    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack(std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));

    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(), std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack(std::make_tuple(), std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(), std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));

    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(1), std::make_tuple(), std::make_tuple(2), std::make_tuple()));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack(std::make_tuple(1), std::make_tuple(), std::make_tuple(2), std::make_tuple()));
    BOOST_HOF_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(1), std::make_tuple(), std::make_tuple(2), std::make_tuple()));

#if BOOST_HOF_HAS_CONSTEXPR_TUPLE
    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(1, 2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_constexpr(std::make_tuple(1, 2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(1, 2)));

    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(1), std::make_tuple(2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_constexpr(std::make_tuple(1), std::make_tuple(2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(1), std::make_tuple(2)));

    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_constexpr(std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));

    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(), std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_constexpr(std::make_tuple(), std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(), std::make_tuple(1), std::make_tuple(), std::make_tuple(2)));

    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::unpack(binary_class())(std::make_tuple(1), std::make_tuple(), std::make_tuple(2), std::make_tuple()));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_constexpr(std::make_tuple(1), std::make_tuple(), std::make_tuple(2), std::make_tuple()));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_unpack_reveal(std::make_tuple(1), std::make_tuple(), std::make_tuple(2), std::make_tuple()));
#endif
}

BOOST_HOF_TEST_CASE()
{
    auto p1 = boost::hof::pack_basic(1, 2);
    static_assert(boost::hof::is_unpackable<decltype(p1)>::value, "Not unpackable");
    static_assert(boost::hof::is_unpackable<decltype((p1))>::value, "Not unpackable");

    auto p2 = boost::hof::pack_forward(1, 2);
    static_assert(boost::hof::is_unpackable<decltype(p2)>::value, "Not unpackable");
    static_assert(boost::hof::is_unpackable<decltype((p2))>::value, "Not unpackable");

    auto p3 = boost::hof::pack(1, 2);
    static_assert(boost::hof::is_unpackable<decltype(p3)>::value, "Not unpackable");
    static_assert(boost::hof::is_unpackable<decltype((p3))>::value, "Not unpackable");

    static_assert(boost::hof::is_unpackable<std::tuple<int>>::value, "Not unpackable");
    
    static_assert(!boost::hof::is_unpackable<int>::value, "Unpackable");
    static_assert(!boost::hof::is_unpackable<void>::value, "Unpackable");
}

BOOST_HOF_TEST_CASE()
{
    typedef std::tuple<int, int> tuple_type;
    static_assert(boost::hof::is_unpackable<tuple_type>::value, "Not unpackable");
    static_assert(boost::hof::is_unpackable<tuple_type&>::value, "Not unpackable");
    static_assert(boost::hof::is_unpackable<const tuple_type&>::value, "Not unpackable");
    static_assert(boost::hof::is_unpackable<tuple_type&&>::value, "Not unpackable");
    
}

BOOST_HOF_STATIC_AUTO lambda_unary_unpack = boost::hof::unpack(BOOST_HOF_STATIC_LAMBDA(int x)
{
    return x;
});

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == lambda_unary_unpack(std::make_tuple(3)));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == lambda_unary_unpack(boost::hof::pack(3)));
}

struct unary_move
{
    std::unique_ptr<int> i;
    unary_move()
    : i(new int(2))
    {}

    template<class T>
    T operator()(T x) const
    {
        return x + *i;
    }
};

static constexpr boost::hof::static_<boost::hof::unpack_adaptor<unary_move> > unary_move_unpack = {};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(unary_move())(std::make_tuple(1)));
    BOOST_HOF_TEST_CHECK(3 == unary_move_unpack(std::make_tuple(1)));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(unary_move())(boost::hof::pack(1)));
    BOOST_HOF_TEST_CHECK(3 == unary_move_unpack(boost::hof::pack(1)));
}

struct indirect_sum_f
{
    template<class T, class U>
    auto operator()(T x, U y) const
    BOOST_HOF_RETURNS(*x + *y);
};

#define MAKE_UNIQUE_PTR(x) std::unique_ptr<int>(new int(x)) 

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(indirect_sum_f())(boost::hof::pack_basic(MAKE_UNIQUE_PTR(1), MAKE_UNIQUE_PTR(2))));
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(indirect_sum_f())(boost::hof::pack_forward(MAKE_UNIQUE_PTR(1), MAKE_UNIQUE_PTR(2))));
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(indirect_sum_f())(boost::hof::pack(MAKE_UNIQUE_PTR(1), MAKE_UNIQUE_PTR(2))));
    BOOST_HOF_TEST_CHECK(3 == boost::hof::unpack(indirect_sum_f())(std::make_tuple(MAKE_UNIQUE_PTR(1), MAKE_UNIQUE_PTR(2))));
}

template<class...>
struct deduce_types
{};

struct deducer
{
    template<class... Ts>
    deduce_types<Ts...> operator()(Ts&&...) const;
};

static constexpr boost::hof::unpack_adaptor<deducer> deduce = {};

BOOST_HOF_TEST_CASE()
{
    STATIC_ASSERT_SAME(deduce_types<int, int>, decltype(deduce(std::make_tuple(1, 2))));
    STATIC_ASSERT_SAME(deduce_types<int, int>, decltype(deduce(std::make_tuple(1), std::make_tuple(2))));
    STATIC_ASSERT_SAME(deduce_types<int, int, int>, decltype(deduce(std::make_tuple(1), std::make_tuple(2), std::make_tuple(3))));
    STATIC_ASSERT_SAME(std::tuple<int&&, int&&>, decltype(std::forward_as_tuple(1, 2)));
    // Disable this test, it seems that rvalue references get swalllowed by type deduction
    // STATIC_ASSERT_SAME(deduce_types<int&&, int&&>, decltype(deduce(std::forward_as_tuple(1, 2))));


    STATIC_ASSERT_SAME(deduce_types<int, int>, decltype(deduce(boost::hof::pack_basic(1, 2))));
    STATIC_ASSERT_SAME(deduce_types<int, int>, decltype(deduce(boost::hof::pack_basic(1), boost::hof::pack_basic(2))));
    STATIC_ASSERT_SAME(deduce_types<int, int, int>, decltype(deduce(boost::hof::pack_basic(1), boost::hof::pack_basic(2), boost::hof::pack_basic(3))));
    // STATIC_ASSERT_SAME(deduce_types<int&&, int&&>, decltype(deduce(boost::hof::pack_forward(1, 2))));
}

struct not_unpackable
{};

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::unpack(boost::hof::always(1));

    static_assert(!boost::hof::is_invocable<decltype(f), not_unpackable>::value, "SFINAE for unpack failed");
}

struct simple_unpackable
{};

namespace boost { namespace hof {

template<>
struct unpack_sequence<simple_unpackable>
{
    template<class F, class S>
    constexpr static auto apply(F&& f, S&&) BOOST_HOF_RETURNS
    (
        f(1)
    );
};
}} // namespace boost::hof

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::unpack(boost::hof::identity)(simple_unpackable{}) == 1);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::unpack(boost::hof::identity)(simple_unpackable{}) == 1);
}
