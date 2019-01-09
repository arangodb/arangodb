// Copyright Jason Rice 2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
// modified from https://github.com/boostorg/hana/issues/304

#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/tuple.hpp>

namespace hana = boost::hana;

struct Foo
{
    Foo() = default;
    Foo(Foo const&) = delete;
    Foo(Foo &&) = default;
};

using bar = decltype(hana::to_tuple(std::array<Foo, 2>()));

int main()
{ }
