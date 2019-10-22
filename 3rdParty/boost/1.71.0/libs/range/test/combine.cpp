// Boost.Range library
//
// Copyright Neil Groves 2014
//
//  Copyright Thorsten Ottosen 2006. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//
#include <boost/range/combine.hpp>

#include <boost/range/algorithm_ext/push_back.hpp>

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>
#include <vector>

namespace boost_range_test
{
    namespace
    {

template<typename ContRef1, typename ContRef2>
void test_combine2()
{
    std::vector<int> v;
    std::list<int> l;

    for (int i = 0; i < 10; ++i)
    {
        v.push_back(i);
        l.push_back(i * 2);
    }

    ContRef1& in1 = v;
    ContRef2& in2 = l;

    std::vector<boost::tuple<int,int> > output;
    boost::push_back(output, boost::combine(in1, in2));

    int index = 0;
    int i1, i2;
    BOOST_FOREACH(boost::tie(i1,i2), output)
    {
        BOOST_CHECK_EQUAL(i1, index);
        BOOST_CHECK_EQUAL(i2, index * 2);
        ++index;
    }
}

template<typename ContRef1, typename ContRef2, typename ContRef3>
void test_combine3()
{
    std::vector<int> v1;
    std::vector<int> v2;
    std::vector<int> v3;

    for (int i = 0; i < 10; ++i)
    {
        v1.push_back(i);
        v2.push_back(i * 2);
        v3.push_back(i * 3);
    }

    ContRef1& in1 = v1;
    ContRef2& in2 = v2;
    ContRef3& in3 = v3;

    std::vector<boost::tuple<int,int,int> > output;
    boost::push_back(output, boost::combine(in1, in2, in3));

    int index = 0;
    int i1, i2, i3;

    BOOST_FOREACH(boost::tie(i1,i2,i3), output)
    {
        BOOST_CHECK_EQUAL(i1, index);
        BOOST_CHECK_EQUAL(i2, index * 2);
        BOOST_CHECK_EQUAL(i3, index * 3);
        ++index;
    }
}

    } // anonymous namespace
} // namespace boost_range_test

boost::unit_test::test_suite* init_unit_test_suite(int, char*[] )
{
    boost::unit_test::test_suite* test =
            BOOST_TEST_SUITE( "Boost.Range combine() test suite" );

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine2<
                        const std::vector<int>, const std::list<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine2<
                        const std::vector<int>, std::list<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine2<
                        std::vector<int>, const std::list<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine2<
                        std::vector<int>, std::list<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine3<
                        std::vector<int>,
                        std::vector<int>,
                        std::vector<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine3<
                        std::vector<int>,
                        std::vector<int>,
                        const std::vector<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine3<
                        std::vector<int>,
                        const std::vector<int>,
                        std::vector<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine3<
                        std::vector<int>,
                        const std::vector<int>,
                        const std::vector<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine3<
                        const std::vector<int>,
                        std::vector<int>,
                        std::vector<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine3<
                        const std::vector<int>,
                        std::vector<int>,
                        const std::vector<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine3<
                        const std::vector<int>,
                        const std::vector<int>,
                        std::vector<int> >)));

    test->add(BOOST_TEST_CASE((
            &boost_range_test::test_combine3<
                        const std::vector<int>,
                        const std::vector<int>,
                        const std::vector<int> >)));

    return test;
}
