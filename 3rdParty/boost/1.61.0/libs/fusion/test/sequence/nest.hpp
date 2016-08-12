/*=============================================================================
    Copyright (C) 2015 Kohei Takahshi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <utility>
#include <boost/config.hpp>

template <typename C>
void test_copy()
{
    C src;
    C dst = src;
    (void)dst;
}

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
template <typename C>
void test_move()
{
    C src;
    C dst = std::move(src);
    (void)dst;
}
#endif

template <typename C>
void test_all()
{
    test_copy<C>();
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
    test_move<C>();
#endif
}

void
test()
{
    using namespace boost::fusion;

    test_all<FUSION_SEQUENCE<FUSION_SEQUENCE<> > >();
    test_all<FUSION_SEQUENCE<FUSION_SEQUENCE<>, int> >();
    test_all<FUSION_SEQUENCE<int, FUSION_SEQUENCE<> > >();
    test_all<FUSION_SEQUENCE<int, FUSION_SEQUENCE<>, float> >();

    test_all<FUSION_SEQUENCE<FUSION_SEQUENCE<int> > >();
    test_all<FUSION_SEQUENCE<FUSION_SEQUENCE<int>, int> >();
    test_all<FUSION_SEQUENCE<int, FUSION_SEQUENCE<int> > >();
    test_all<FUSION_SEQUENCE<int, FUSION_SEQUENCE<int>, float> >();

    test_all<FUSION_SEQUENCE<FUSION_SEQUENCE<int, float> > >();
    test_all<FUSION_SEQUENCE<FUSION_SEQUENCE<int, float>, int> >();
    test_all<FUSION_SEQUENCE<int, FUSION_SEQUENCE<int, float> > >();
    test_all<FUSION_SEQUENCE<int, FUSION_SEQUENCE<int, float>, float> >();
}

