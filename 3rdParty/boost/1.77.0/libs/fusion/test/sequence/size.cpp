/*=============================================================================
    Copyright (c) 2014 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/config.hpp>
#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/container/deque.hpp>
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/container/set.hpp>
#include <boost/fusion/container/map.hpp>
#include <boost/fusion/support/pair.hpp>
#include <boost/fusion/sequence/intrinsic/size.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/mpl/assert.hpp>

#include <boost/array.hpp>
#include <boost/fusion/adapted/boost_array.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/fusion/adapted/boost_tuple.hpp>
#if !defined(BOOST_NO_CXX11_HDR_TUPLE) && \
    !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <tuple>
#include <boost/fusion/adapted/std_tuple.hpp>
#endif

template <typename LHS, typename RHS>
void check_(LHS const&, RHS const&)
{
    BOOST_MPL_ASSERT((boost::is_same<LHS, RHS>));
}

template <typename S>
void check()
{
    check_(
        boost::fusion::result_of::size<S>::type::value
      , boost::fusion::result_of::size<S>::value
    );
}

void test()
{
    {
        check<boost::fusion::vector<> >();
        check<boost::fusion::vector<int> >();
        check<boost::fusion::vector<int, int> >();
        check<boost::fusion::vector<int, int, int> >();
    }

    {
        check<boost::fusion::deque<> >();
        check<boost::fusion::deque<int> >();
        check<boost::fusion::deque<int, int> >();
        check<boost::fusion::deque<int, int, int> >();
    }

    {
        check<boost::fusion::list<> >();
        check<boost::fusion::list<int> >();
        check<boost::fusion::list<int, int> >();
        check<boost::fusion::list<int, int, int> >();
    }

    {
        check<boost::fusion::set<> >();
        check<boost::fusion::set<int> >();
        check<boost::fusion::set<int, float> >();
        check<boost::fusion::set<int, float, double> >();
    }

    {
        check<boost::fusion::map<> >();
        check<boost::fusion::map<boost::fusion::pair<int, int> > >();
        check<boost::fusion::map<boost::fusion::pair<int, int> , boost::fusion::pair<float, int> > >();
        check<boost::fusion::map<boost::fusion::pair<int, int> , boost::fusion::pair<float, int> , boost::fusion::pair<double, int> > >();
    }

    {
        check<boost::array<int, 1> >();
        check<boost::array<int, 2> >();
        check<boost::array<int, 3> >();
    }

    {
        check<boost::tuples::tuple<> >();
        check<boost::tuples::tuple<int> >();
        check<boost::tuples::tuple<int, int> >();
        check<boost::tuples::tuple<int, int, int> >();
    }

#if !defined(BOOST_NO_CXX11_HDR_TUPLE) && \
    !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    {
        check<std::tuple<> >();
        check<std::tuple<int> >();
        check<std::tuple<int, int> >();
        check<std::tuple<int, int, int> >();
    }
#endif
}

