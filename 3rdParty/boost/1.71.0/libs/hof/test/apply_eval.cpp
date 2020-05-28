/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    apply_eval.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/apply_eval.hpp>
#include <boost/hof/always.hpp>
#include <boost/hof/placeholders.hpp>
#include "test.hpp"

#include <memory>

BOOST_HOF_TEST_CASE()
{    
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::apply_eval(binary_class(), boost::hof::always(1), boost::hof::always(2)) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::apply_eval(binary_class(), []{ return 1; }, []{ return 2;}) == 3);
}

BOOST_HOF_TEST_CASE()
{
    boost::hof::apply_eval(boost::hof::always(), boost::hof::always(1), boost::hof::always(2));
}
#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::apply_eval(boost::hof::always(), boost::hof::always(1), boost::hof::always(2))), "noexcept apply_eval");
}
#endif
BOOST_HOF_TEST_CASE()
{
    int i = 3;
    BOOST_HOF_TEST_CHECK(boost::hof::apply_eval(boost::hof::_ - boost::hof::_, [&]{ return i++; }, [&]{ return i++;}) == -1);
    BOOST_HOF_TEST_CHECK(boost::hof::apply_eval(boost::hof::_ - boost::hof::_, [&]{ return ++i; }, [&]{ return ++i;}) == -1);
}

struct indirect_sum_f
{
    template<class T, class U>
    auto operator()(T x, U y) const
    BOOST_HOF_RETURNS(*x + *y);
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(
        boost::hof::apply_eval(
            indirect_sum_f(), 
            []{ return std::unique_ptr<int>(new int(1)); }, 
            []{ return std::unique_ptr<int>(new int(2)); })
        == 3);
}

std::unique_ptr<int> moveable(int i)
{
    return std::unique_ptr<int>{new int(i)};
}

BOOST_HOF_TEST_CASE()
{    
    BOOST_HOF_TEST_CHECK(*boost::hof::apply_eval(&moveable, boost::hof::always(1)) == 1);
    BOOST_HOF_TEST_CHECK(*boost::hof::apply_eval(&moveable, boost::hof::always(3)) == 3);
}
