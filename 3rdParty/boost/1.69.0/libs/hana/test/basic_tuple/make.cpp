// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/basic_tuple.hpp>
#include <boost/hana/core/make.hpp>
namespace hana = boost::hana;


template <int i>
struct x { };

int main() {
    auto xs1 = hana::make<hana::basic_tuple_tag>(); (void)xs1;
    auto xs2 = hana::make<hana::basic_tuple_tag>(x<0>{}); (void)xs2;
    auto xs3 = hana::make<hana::basic_tuple_tag>(x<0>{}, x<1>{}); (void)xs3;
    auto xs4 = hana::make<hana::basic_tuple_tag>(x<0>{}, x<1>{}, x<2>{}); (void)xs4;
}
