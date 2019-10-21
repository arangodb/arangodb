/*=============================================================================
    Copyright (c) 1999-2003 Jaakko Jarvi
    Copyright (c) 2001-2011 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/tuple/tuple.hpp>
#include <boost/fusion/adapted/mpl.hpp>

#define NO_CONSTRUCT_FROM_NIL
#define FUSION_SEQUENCE tuple
#define FUSION_AT get
#include "construction.hpp"

// Bug in C++03 tuple? Cannot construct from a std::pair without including
// std::pair fusion adaption
#if !defined(BOOST_FUSION_HAS_VARIADIC_TUPLE)
# include <boost/fusion/adapted/std_pair.hpp>
#endif

struct test_conversion
{
    test_conversion(int value) : value_(value) {}

    int value_;
};

int
main()
{
    test();

    {
        using namespace boost::fusion;
        const tuple<int, test_conversion> instance(std::pair<int, int>(1, 9));
        BOOST_TEST(boost::fusion::get<0>(instance) == 1);
        BOOST_TEST(boost::fusion::get<1>(instance).value_ == 9);
    }
    {
        using namespace boost::fusion;
        const std::pair<int, int> init(16, 4);
        const tuple<int, test_conversion> instance(init);
        BOOST_TEST(boost::fusion::get<0>(instance) == 16);
        BOOST_TEST(boost::fusion::get<1>(instance).value_ == 4);
    }

    return boost::report_errors();
}
