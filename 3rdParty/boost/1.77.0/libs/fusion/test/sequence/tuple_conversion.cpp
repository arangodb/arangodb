/*=============================================================================
    Copyright (c) 2016 Lee Clagett

    Use modification and distribution are subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/tuple.hpp>

#define FUSION_SEQUENCE boost::fusion::tuple
#include "conversion.hpp"

// Bug in C++03 tuple? Cannot construct from a std::pair without including
// std::pair fusion adaption
#if !defined(BOOST_FUSION_HAS_VARIADIC_TUPLE)
# include <boost/fusion/adapted/std_pair.hpp>
#endif

using namespace test_detail;

void test_tuple()
{
    BOOST_TEST((
        run< can_copy< boost::fusion::tuple<int, int> > >(
            std::pair<int, int>(1, 9)
        )
    ));
    BOOST_TEST((
        run< can_copy< boost::fusion::tuple<int, convertible> > >(
            std::pair<int, int>(1, 9)
        )
    ));
    BOOST_TEST((
        run< can_copy< boost::fusion::tuple<convertible, int> > >(
            std::pair<int, int>(1, 9)
        )
    ));
    BOOST_TEST((
        run< can_copy< boost::fusion::tuple<convertible, convertible> > >(
            std::pair<int, int>(1, 9)
        )
    ));
}

int main()
{
    test<can_assign>(); // conversion construction not supported
    test_tuple();
    return boost::report_errors();
}
