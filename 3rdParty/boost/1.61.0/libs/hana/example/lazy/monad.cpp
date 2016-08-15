// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/chain.hpp>
#include <boost/hana/eval.hpp>
#include <boost/hana/lazy.hpp>

#include <functional>
#include <iostream>
#include <sstream>
namespace hana = boost::hana;


template <typename T>
T read_(std::istream& stream) {
    T x;
    stream >> x;
    std::cout << "read " << x << " from the stream\n";
    return x;
}

int main() {
    std::stringstream ss;
    int in = 123;

    std::cout << "creating the monadic chain...\n";
    auto out = hana::make_lazy(read_<int>)(std::ref(ss))
        | [](auto x) {
            std::cout << "performing x + 1...\n";
            return hana::make_lazy(x + 1);
        }
        | [](auto x) {
            std::cout << "performing x / 2...\n";
            return hana::make_lazy(x / 2);
        };

    std::cout << "putting " << in << " in the stream...\n";
    ss << in; // nothing is evaluated yet

    std::cout << "evaluating the monadic chain...\n";
    auto eout = hana::eval(out);

    std::cout << "the result of the monadic chain is " << eout << "\n";
    BOOST_HANA_RUNTIME_CHECK(eout == (in + 1) / 2);
}
