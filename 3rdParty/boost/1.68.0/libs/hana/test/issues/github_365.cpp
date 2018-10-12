// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/accessors.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/second.hpp>

#include <cstddef>
#include <type_traits>
namespace hana = boost::hana;


//
// This test makes sure that `hana::accessors` does not decay C-style array
// members to pointers.
//

struct Foo {
    float array[3];
};

BOOST_HANA_ADAPT_STRUCT(Foo,
    array
);

template <std::size_t N>
using FloatArray = float[N];

struct Bar {
    BOOST_HANA_DEFINE_STRUCT(Bar,
        (FloatArray<3>, array)
    );
};

int main() {
    {
        Foo foo = {{1.0f, 2.0f, 3.0f}};
        auto accessors = hana::accessors<Foo>();
        auto get_array = hana::second(hana::at_c<0>(accessors));
        static_assert(std::is_same<decltype(get_array(foo)), float(&)[3]>{}, "");
        float (&array)[3] = get_array(foo);
        BOOST_HANA_RUNTIME_CHECK(array[0] == 1.0f);
        BOOST_HANA_RUNTIME_CHECK(array[1] == 2.0f);
        BOOST_HANA_RUNTIME_CHECK(array[2] == 3.0f);
    }

    {
        Bar bar = {{1.0f, 2.0f, 3.0f}};
        auto accessors = hana::accessors<Bar>();
        auto get_array = hana::second(hana::at_c<0>(accessors));
        static_assert(std::is_same<decltype(get_array(bar)), float(&)[3]>{}, "");
        float (&array)[3] = get_array(bar);
        BOOST_HANA_RUNTIME_CHECK(array[0] == 1.0f);
        BOOST_HANA_RUNTIME_CHECK(array[1] == 2.0f);
        BOOST_HANA_RUNTIME_CHECK(array[2] == 3.0f);
    }
}
