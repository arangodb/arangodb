/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    decorate.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/decorate.hpp>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::decorate(boost::hof::always(1))(boost::hof::always(1))(boost::hof::always(1))(5) == 1);
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
    void operator()() const noexcept {}
    void operator()(copy_throws) const noexcept {}
};

BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::decorate(boost::hof::always(1))(boost::hof::always(1))(boost::hof::always(1))(5)), "noexcept decorator");
    static_assert(!noexcept(boost::hof::decorate(boost::hof::always(1))(boost::hof::always(1))(boost::hof::always(1))(copy_throws{})), "noexcept decorator");
}

#endif
