// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/basic_tuple.hpp>
namespace hana = boost::hana;


template <int i>
struct x { };


int main() {
    constexpr hana::basic_tuple<> empty{}; (void)empty;

    constexpr hana::basic_tuple<int, float> xs{1, 2.3f}; (void)xs;
    constexpr auto ys = hana::basic_tuple<int, float>{1, 2.3f};
    constexpr auto copy = ys; (void)copy;
}
