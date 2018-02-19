/*=============================================================================
    Copyright (c) 2016 Lee Clagett

    Use modification and distribution are subject to the Boost Software
    License, Version 1.0. (See accompanyintg file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
==============================================================================*/

#include <boost/config.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/adapted/boost_tuple.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#if !defined(BOOST_NO_CXX11_HDR_TUPLE) \
 && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
# include <boost/fusion/adapted/std_tuple.hpp>
#endif
#include <boost/fusion/container/deque.hpp>
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/tuple.hpp>
#include <boost/fusion/container/vector.hpp>

#include "fixture.hpp"

template <template <typename> class Scenario>
void test()
{
    using namespace test_detail;

    // Note the trunction conversion tests from each containter
    // ... bug or feature?

    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(FUSION_SEQUENCE<int>(300), 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::push_back(FUSION_SEQUENCE<int>(200), 400)
          , FUSION_SEQUENCE<convertible>(200)
        )
    ));

    BOOST_TEST((run<Scenario<FUSION_SEQUENCE<> > >(boost::fusion::vector<>())));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<> > >(
            boost::fusion::vector<int>(100), boost::fusion::vector<>()
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::vector<int>(110)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::vector<int, int>(200, 100)
          , boost::fusion::vector<convertible>(200)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::vector<int, int>(200, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::vector<int, int, int>(500, 400, 100)
          , boost::fusion::vector<convertible, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::fusion::vector<int>(500), 400
            )
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::fusion::vector<int, int>(500, 400), 100
            )
          , boost::fusion::vector<convertible, int>(500, 400)
        )
    ));

    BOOST_TEST((run<Scenario< FUSION_SEQUENCE<> > >(boost::fusion::deque<>())));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<> > >(
            boost::fusion::deque<int>(100), boost::fusion::deque<>()
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::deque<int>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::deque<int, int>(500, 100)
          , boost::fusion::deque<convertible>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::deque<int, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::deque<int, int, int>(500, 400, 100)
          , boost::fusion::deque<convertible, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::fusion::deque<int>(500), 400
            )
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::fusion::deque<int, int>(500, 400), 100
            )
          , boost::fusion::deque<convertible, int>(500, 400)
        )
    ));

    BOOST_TEST((run< Scenario< FUSION_SEQUENCE<> > >(boost::fusion::list<>())));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<> > >(
            boost::fusion::list<int>(100), boost::fusion::list<>()
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::list<int>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::list<int, int>(500, 100)
          , boost::fusion::list<convertible>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::list<int, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::list<int, int, int>(500, 400, 100)
          , boost::fusion::list<convertible, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::fusion::list<int>(500), 400
            )
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::fusion::list<int, int>(500, 400), 100
            )
          , boost::fusion::list<convertible, int>(500, 400)
        )
    ));

    BOOST_TEST((run<Scenario< FUSION_SEQUENCE<> > >(boost::fusion::tuple<>())));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<> > >(
            boost::fusion::tuple<int>(100), boost::fusion::tuple<>()
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::tuple<int>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::fusion::tuple<int, int>(500, 100)
          , boost::fusion::tuple<convertible>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::tuple<int, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::tuple<int, int, int>(500, 400, 100)
          , boost::fusion::tuple<convertible, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::fusion::tuple<int>(500), 400
            )
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::fusion::tuple<int, int>(500, 400), 100
            )
          , boost::fusion::tuple<convertible, int>(500, 400)
        )
    ));

    BOOST_TEST((run< Scenario< FUSION_SEQUENCE<> > >(boost::tuple<>())));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<> > >(
            boost::tuple<int>(100), boost::tuple<>()
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::tuple<int>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            boost::tuple<int, int>(500, 100)
          , boost::tuple<convertible>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::tuple<int, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::tuple<int, int, int>(500, 400, 100)
          , boost::tuple<convertible, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(boost::tuple<int>(500), 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                boost::tuple<int, int>(500, 400), 100
            )
          , boost::tuple<convertible, int>(500, 400)
        )
    ));

#if !defined(BOOST_NO_CXX11_HDR_TUPLE) \
 && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    BOOST_TEST((run< Scenario< FUSION_SEQUENCE<> > >(std::tuple<>())));
    BOOST_TEST((
        run<Scenario<FUSION_SEQUENCE<> > >(std::tuple<int>(100), std::tuple<>())
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            std::tuple<int>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            std::tuple<int, int>(500, 100)
          , std::tuple<convertible>(500)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            std::tuple<int, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            std::tuple<int, int, int>(500, 400, 100)
          , std::tuple<convertible, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(std::tuple<int>(500), 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            boost::fusion::push_back(
                std::tuple<int, int>(500, 400), 100
            )
          , std::tuple<convertible, int>(500, 400)
        )
    ));
#endif

    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible, int> > >(
            std::pair<int, int>(500, 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<> > >(
            std::pair<int, int>(500, 400)
          , boost::fusion::vector<>()
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<convertible> > >(
            std::pair<int, int>(500, 400)
          , boost::fusion::vector<convertible>(500)
        )
    ));
}
