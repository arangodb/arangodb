// Boost.Range library
//
//  Copyright Robin Eckert 2015. Use, modification and distribution is
//  subject to the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//
// For more information, see http://www.boost.org/libs/range/
//
#include <boost/range/adaptor/ref_unwrapped.hpp>

#define BOOST_TEST_MAIN

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

#include <vector>

#if !defined(BOOST_NO_CXX11_AUTO_DECLARATIONS) && !defined(BOOST_NO_CXX11_RANGE_BASED_FOR) && !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX)

namespace boost
{

    BOOST_AUTO_TEST_CASE(test_mutable)
    {
        int one = 1;
        int two = 2;
        int three = 3;

        std::vector<std::reference_wrapper<int>> input_values{one, two, three};

        const std::vector<int*> expected{&one, &two, &three};
        std::vector<int*> actual;

        for (auto&& value : input_values | adaptors::ref_unwrapped)
        {
          actual.push_back(&value);
        }

        BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(),
                                      expected.end(),
                                      actual.begin(),
                                      actual.end());
    }

    BOOST_AUTO_TEST_CASE(test_const_range)
    {
        int one = 1;
        int two = 2;
        int three = 3;

        const std::vector<std::reference_wrapper<int>> input_values{one, two, three};

        const std::vector<int*> expected{&one, &two, &three};
        std::vector<int*> actual;

        for (auto&& value : input_values | adaptors::ref_unwrapped)
        {
          actual.push_back(&value);
        }

        BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(),
                                      expected.end(),
                                      actual.begin(),
                                      actual.end());
    }

    BOOST_AUTO_TEST_CASE(test_const_reference)
    {
        const int one = 1;
        const int two = 2;
        const int three = 3;

        const std::vector<std::reference_wrapper<const int>> input_values{one, two, three};

        const std::vector<const int*> expected{&one, &two, &three};
        std::vector<const int*> actual;

        for (auto&& value : input_values | adaptors::ref_unwrapped)
        {
          actual.push_back(&value);
        }

        BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(),
                                      expected.end(),
                                      actual.begin(),
                                      actual.end());
    }


}

#else

BOOST_AUTO_TEST_CASE(empty)
{
  // C++11 only
}

#endif
