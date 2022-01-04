/*=============================================================================
    Copyright (c) 2016-2018 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/adapted/struct/define_struct_inline.hpp>
#include <utility>

struct wrapper
{
    int value;

    wrapper() : value(42) {}
    wrapper(wrapper&& other) : value(other.value) { other.value = 0; }
    wrapper(wrapper const& other) : value(other.value) {}

    wrapper& operator=(wrapper&& other) { value = other.value; other.value = 0; return *this; }
    wrapper& operator=(wrapper const& other) { value = other.value; return *this; }
};

namespace ns
{
    BOOST_FUSION_DEFINE_TPL_STRUCT_INLINE((W), value, (W, w))
}

int main()
{
    using namespace boost::fusion;

    {
        ns::value<wrapper> x;
        ns::value<wrapper> y(x); // copy

        BOOST_TEST(x.w.value == 42);
        BOOST_TEST(y.w.value == 42);

        ++y.w.value;

        BOOST_TEST(x.w.value == 42);
        BOOST_TEST(y.w.value == 43);

        y = x; // copy assign

        BOOST_TEST(x.w.value == 42);
        BOOST_TEST(y.w.value == 42);
    }

    // Older MSVCs and gcc 4.4 don't generate move ctor by default.
#if !(defined(CI_SKIP_KNOWN_FAILURE) && (BOOST_WORKAROUND(BOOST_MSVC, < 1900) || BOOST_WORKAROUND(BOOST_GCC, / 100 == 404)))
    {
        ns::value<wrapper> x;
        ns::value<wrapper> y(std::move(x)); // move

        BOOST_TEST(x.w.value == 0);
        BOOST_TEST(y.w.value == 42);

        ++y.w.value;

        BOOST_TEST(x.w.value == 0);
        BOOST_TEST(y.w.value == 43);

        y = std::move(x); // move assign

        BOOST_TEST(x.w.value == 0);
        BOOST_TEST(y.w.value == 0);
    }
#endif // !(ci && (msvc < 14.0 || gcc 4.4.x))

    return boost::report_errors();
}

