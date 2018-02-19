// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


struct A { };
struct B { };

int main() {
    {
        using T = hana::tuple<int, A>;
        static_assert((sizeof(T) == sizeof(int)), "");
    }
    {
        using T = hana::tuple<A, int>;
        static_assert((sizeof(T) == sizeof(int)), "");
    }
    {
        using T = hana::tuple<A, int, B>;
        static_assert((sizeof(T) == sizeof(int)), "");
    }
    {
        using T = hana::tuple<A, B, int>;
        static_assert((sizeof(T) == sizeof(int)), "");
    }
    {
        using T = hana::tuple<int, A, B>;
        static_assert((sizeof(T) == sizeof(int)), "");
    }
}
