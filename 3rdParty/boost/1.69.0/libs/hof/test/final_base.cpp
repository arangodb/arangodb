/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    final_base.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/flip.hpp>
#include "test.hpp"

#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ < 7
#define FINAL
#else
#define FINAL final
#endif


struct f FINAL {
    int operator()(int i, void *) const {
        return i;
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::flip(f())(nullptr, 2) == 2);
}
