// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/eval.hpp>
#include <boost/hana/extend.hpp>
#include <boost/hana/extract.hpp>
#include <boost/hana/lazy.hpp>

#include <functional>
#include <istream>
#include <sstream>
namespace hana = boost::hana;


template <typename T>
T read_one(std::istream& s) {
    T value;
    s >> value;
    return value;
}

int main() {
    std::stringstream s;
    s << "1 2 3";

    auto from_stream = hana::extend(hana::make_lazy(read_one<int>)(std::ref(s)), [](auto i) {
        return hana::eval(i) + 1;
    });

    BOOST_HANA_RUNTIME_CHECK(hana::extract(from_stream) == 2);
    BOOST_HANA_RUNTIME_CHECK(hana::extract(from_stream) == 3);
    BOOST_HANA_RUNTIME_CHECK(hana::extract(from_stream) == 4);
}
