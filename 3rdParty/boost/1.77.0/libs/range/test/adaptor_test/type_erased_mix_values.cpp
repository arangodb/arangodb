// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/range/adaptor/type_erased.hpp>
#include "type_erased_test.hpp"

#include <boost/test/unit_test.hpp>

#include <vector>

namespace boost_range_adaptor_type_erased_test
{
    namespace
    {

template<
    class Traversal
  , class ValueType
  , class SourceValueType
  , class SourceReference
  , class TargetValueType
  , class TargetReference
>
void mix_values_impl()
{
    typedef std::vector<ValueType> Container;

    typedef typename boost::any_range_type_generator<
        Container
      , SourceValueType
      , Traversal
      , SourceReference
    >::type source_type;

    typedef typename boost::any_range_type_generator<
        Container
      , TargetValueType
      , Traversal
      , TargetReference
    >::type target_type;

    Container test_data;
    for (int i = 0; i < 10; ++i)
        test_data.push_back(i);

    const source_type source_data(test_data);
    target_type t1(source_data);
    BOOST_CHECK_EQUAL_COLLECTIONS(source_data.begin(), source_data.end(),
                                  t1.begin(), t1.end());

    target_type t2;
    t2 = source_data;
    BOOST_CHECK_EQUAL_COLLECTIONS(source_data.begin(), source_data.end(),
                                  t2.begin(), t2.end());
}

template<class Traversal>
void mix_values_driver()
{
    mix_values_impl<
        Traversal,
        MockType,
        MockType2, const MockType&,
        MockType, const MockType&
    >();
}

void mix_values()
{
    mix_values_driver<boost::single_pass_traversal_tag >();
    mix_values_driver<boost::forward_traversal_tag >();
    mix_values_driver<boost::bidirectional_traversal_tag >();
    mix_values_driver<boost::random_access_traversal_tag >();
}

    } // anonymous namespace
} // namespace boost_range_adaptor_type_erased_test

boost::unit_test::test_suite*
init_unit_test_suite(int, char*[])
{
    boost::unit_test::test_suite* test =
        BOOST_TEST_SUITE("RangeTestSuite.adaptor.type_erased_mix_values");

    test->add(BOOST_TEST_CASE(
            &boost_range_adaptor_type_erased_test::mix_values));

    return test;
}

