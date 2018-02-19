/*=============================================================================
    Copyright (c) 2016 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/config.hpp>
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/adapted/struct/define_assoc_struct.hpp>
#include <utility>

struct key_type;
struct wrapper
{
    int value;

    wrapper() : value(42) {}
    wrapper(wrapper&& other) : value(other.value) { other.value = 0; }
    wrapper(wrapper const& other) : value(other.value) {}

    wrapper& operator=(wrapper&& other) { value = other.value; other.value = 0; return *this; }
    wrapper& operator=(wrapper const& other) { value = other.value; return *this; }
};
BOOST_FUSION_DEFINE_ASSOC_STRUCT((ns), value, (wrapper, w, key_type))

int main()
{
    using namespace boost::fusion;

    {
        ns::value x;
        ns::value y(x); // copy

        BOOST_TEST(x.w.value == 42);
        BOOST_TEST(y.w.value == 42);

        ++y.w.value;

        BOOST_TEST(x.w.value == 42);
        BOOST_TEST(y.w.value == 43);

        y = x; // copy assign

        BOOST_TEST(x.w.value == 42);
        BOOST_TEST(y.w.value == 42);
    }

    {
        ns::value x;
        ns::value y(std::move(x)); // move

        BOOST_TEST(x.w.value == 0);
        BOOST_TEST(y.w.value == 42);

        ++y.w.value;

        BOOST_TEST(x.w.value == 0);
        BOOST_TEST(y.w.value == 43);

        y = std::move(x); // move assign

        BOOST_TEST(x.w.value == 0);
        BOOST_TEST(y.w.value == 0);
    }

    return boost::report_errors();
}

#else

int main()
{
}

#endif
