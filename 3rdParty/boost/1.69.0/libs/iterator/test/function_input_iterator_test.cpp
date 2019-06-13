// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>

#include <algorithm>
#include <iterator>
#include <vector>

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_DECLTYPE)
// Force boost::result_of use decltype, even on compilers that don't support N3276.
// This enables this test to also verify if the iterator works with lambdas
// on such compilers with this config macro. Note that without the macro result_of
// (and consequently the iterator) is guaranteed to _not_ work, so this case is not
// worth testing anyway.
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#include <boost/core/lightweight_test.hpp>
#include <boost/iterator/function_input_iterator.hpp>

namespace {

struct ones {
    typedef int result_type;
    result_type operator() () {
        return 1;
    }
};

int ones_function () {
    return 1;
}

struct counter {
    typedef int result_type;
    int n;
    explicit counter(int n_) : n(n_) { }
    result_type operator() () {
        return n++;
    }
};

} // namespace

using namespace std;

int main()
{
    // test the iterator with function objects
    ones ones_generator;
    vector<int> values(10);
    generate(values.begin(), values.end(), ones());
    
    vector<int> generated;
    copy(
        boost::make_function_input_iterator(ones_generator, 0),
        boost::make_function_input_iterator(ones_generator, 10),
        back_inserter(generated)
        );

    BOOST_TEST_ALL_EQ(values.begin(), values.end(), generated.begin(), generated.end());

    // test the iterator with normal functions
    vector<int>().swap(generated);
    copy(
        boost::make_function_input_iterator(&ones_function, 0),
        boost::make_function_input_iterator(&ones_function, 10),
        back_inserter(generated)
        );

    BOOST_TEST_ALL_EQ(values.begin(), values.end(), generated.begin(), generated.end());

    // test the iterator with a reference to a function
    vector<int>().swap(generated);
    copy(
        boost::make_function_input_iterator(ones_function, 0),
        boost::make_function_input_iterator(ones_function, 10),
        back_inserter(generated)
        );

    BOOST_TEST_ALL_EQ(values.begin(), values.end(), generated.begin(), generated.end());

    // test the iterator with a stateful function object
    counter counter_generator(42);
    vector<int>().swap(generated);
    copy(
        boost::make_function_input_iterator(counter_generator, 0),
        boost::make_function_input_iterator(counter_generator, 10),
        back_inserter(generated)
        );

    BOOST_TEST_EQ(generated.size(), 10u);
    BOOST_TEST_EQ(counter_generator.n, 42 + 10);
    for(std::size_t i = 0; i != 10; ++i)
        BOOST_TEST_EQ(generated[i], static_cast<int>(42 + i));

#if !defined(BOOST_NO_CXX11_LAMBDAS) && !defined(BOOST_NO_CXX11_AUTO_DECLARATIONS) \
    && defined(BOOST_RESULT_OF_USE_DECLTYPE)
    // test the iterator with lambda expressions
    int num = 42;
    auto lambda_generator = [&num] { return num++; };
    vector<int>().swap(generated);
    copy(
        boost::make_function_input_iterator(lambda_generator, 0),
        boost::make_function_input_iterator(lambda_generator, 10),
        back_inserter(generated)
        );

    BOOST_TEST_EQ(generated.size(), 10u);
    for(std::size_t i = 0; i != 10; ++i)
        BOOST_TEST_EQ(generated[i], static_cast<int>(42 + i));
#endif // BOOST_NO_CXX11_LAMBDAS

    return boost::report_errors();
}
