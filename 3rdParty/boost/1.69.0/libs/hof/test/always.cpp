/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    always.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/always.hpp>
#include <boost/hof/function.hpp>
#include <memory>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{
    static const int ten = 10;
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::always(ten)(1,2,3,4,5) == 10);
    BOOST_HOF_TEST_CHECK( boost::hof::always(ten)(1,2,3,4,5) == 10 );
    
    int i = 10; 
    BOOST_HOF_TEST_CHECK( boost::hof::always(std::ref(i))(1,2,3,4,5) == 10 );
    BOOST_HOF_TEST_CHECK( &boost::hof::always(std::ref(i))(1,2,3,4,5) == &i );

    boost::hof::always()(1, 2);
    static_assert(std::is_same<decltype(boost::hof::always()(1, 2)), BOOST_HOF_ALWAYS_VOID_RETURN>::value, "Failed");
}

BOOST_HOF_TEST_CASE()
{
    int i = 10; 
    BOOST_HOF_TEST_CHECK( boost::hof::always_ref(i)(1,2,3,4,5) == 10 );
    BOOST_HOF_TEST_CHECK( &boost::hof::always_ref(i)(1,2,3,4,5) == &i );
}

BOOST_HOF_STATIC_FUNCTION(gten) = boost::hof::always(std::integral_constant<int, 10>{});

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(gten(1,2,3,4,5) == 10);
    BOOST_HOF_TEST_CHECK(gten(1,2,3,4,5) == 10);
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::always(10);
    STATIC_ASSERT_NOT_DEFAULT_CONSTRUCTIBLE(decltype(f));
    BOOST_HOF_TEST_CHECK(f(1,2,3,4,5) == 10);
}

struct copy_throws 
{
    copy_throws() {}
    copy_throws(copy_throws const&) {}
    copy_throws(copy_throws&&) noexcept {}
};

BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::always()()), "noexcept always");
    static_assert(noexcept(boost::hof::always(1)()), "noexcept always");
    static_assert(!noexcept(boost::hof::always(copy_throws{})()), "noexcept always");
    copy_throws ct{};
    static_assert(!noexcept(boost::hof::always(ct)()), "noexcept always");
    static_assert(noexcept(boost::hof::always(std::ref(ct))()) == BOOST_HOF_IS_NOTHROW_COPY_CONSTRUCTIBLE(std::reference_wrapper<copy_throws>), "noexcept always");
    auto ctf = boost::hof::always(copy_throws{});
    static_assert(!noexcept(ctf()), "noexcept always");
}
