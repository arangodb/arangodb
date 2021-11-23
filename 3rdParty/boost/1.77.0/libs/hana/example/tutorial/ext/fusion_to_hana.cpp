// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/for_each.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/ext/boost/fusion/vector.hpp>

#include <boost/fusion/include/vector.hpp>

#include <iostream>
namespace fusion = boost::fusion;
namespace hana = boost::hana;


//! [main]
// In the old code, this used to receive a Fusion sequence.
// Now, it can be either a Hana sequence or a Fusion sequence.
template <typename Sequence>
void f(Sequence const& seq) {
    hana::for_each(seq, [](auto const& element) {
        std::cout << element << std::endl;
    });
}
//! [main]


int main() {
    f(hana::make_tuple(1, 2, 3));
    f(fusion::make_vector(1, 2, 3));
}
