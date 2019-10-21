/*=============================================================================
    Copyright (c) 2018 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/fusion/sequence/intrinsic/at_key.hpp>
#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/container/deque.hpp>
#include <boost/fusion/container/map.hpp>
#include <boost/fusion/container/set.hpp>
#include <boost/fusion/tuple/tuple.hpp>
#include <boost/core/lightweight_test.hpp>

template <typename Sequence>
void test_at()
{
    Sequence seq;

    // zero initialized
    BOOST_TEST(boost::fusion::at_c<0>(seq)[0] == 0);
    BOOST_TEST(boost::fusion::at_c<0>(seq)[1] == 0);
    BOOST_TEST(boost::fusion::at_c<0>(seq)[2] == 0);

    int (&arr)[3] = boost::fusion::deref(boost::fusion::begin(seq));

    arr[0] = 2;
    arr[1] = 4;
    arr[2] = 6;

    BOOST_TEST(boost::fusion::at_c<0>(seq)[0] == 2);
    BOOST_TEST(boost::fusion::at_c<0>(seq)[1] == 4);
    BOOST_TEST(boost::fusion::at_c<0>(seq)[2] == 6);

    boost::fusion::at_c<0>(seq)[1] = 42;

    BOOST_TEST(boost::fusion::at_c<0>(seq)[0] == 2);
    BOOST_TEST(boost::fusion::at_c<0>(seq)[1] == 42);
    BOOST_TEST(boost::fusion::at_c<0>(seq)[2] == 6);
}

template <typename T> inline T& value(T& v) { return v; }
template <typename K, typename T> inline T& value(boost::fusion::pair<K, T>& v) { return v.second; }

template <typename Sequence>
void test_at_key()
{
    Sequence seq;

    // zero initialized
    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[0] == 0);
    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[1] == 0);
    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[2] == 0);

    int (&arr)[3] = value(boost::fusion::deref(boost::fusion::begin(seq)));

    arr[0] = 2;
    arr[1] = 4;
    arr[2] = 6;

    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[0] == 2);
    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[1] == 4);
    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[2] == 6);

    boost::fusion::at_key<int[3]>(seq)[1] = 42;

    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[0] == 2);
    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[1] == 42);
    BOOST_TEST(boost::fusion::at_key<int[3]>(seq)[2] == 6);
}

int main()
{
    using namespace boost::fusion;

    test_at<vector<int[3]> >();
    test_at<deque<int[3]> >();
    test_at<list<int[3]> >();
    test_at<tuple<int[3]> >();

#if !BOOST_WORKAROUND(BOOST_GCC, / 100 == 406) || defined(BOOST_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_ARGS)
    // FIXME: gcc 4.6 w/ c++0x doesn't like set with array...
    test_at_key<set<int[3]> >();
#endif
    test_at_key<map<pair<int[3], int[3]> > >();
    return boost::report_errors();
}
