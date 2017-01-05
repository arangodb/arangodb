// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/ext/boost/tuple.hpp>

#include <boost/tuple/tuple.hpp>

#include <type_traits>
using namespace boost::hana;


int main() {
    //////////////////////////////////////////////////////////////////////////
    // make sure the tag is correct
    //////////////////////////////////////////////////////////////////////////
    {
        auto make_cons = [](auto x, auto xs) {
            return boost::tuples::cons<decltype(x), decltype(xs)>{x, xs};
        };

        static_assert(std::is_same<
            tag_of_t<decltype(boost::make_tuple())>,
            ext::boost::tuple_tag
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<decltype(boost::make_tuple(1))>,
            ext::boost::tuple_tag
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<decltype(boost::make_tuple(1, '2'))>,
            ext::boost::tuple_tag
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<decltype(boost::make_tuple(1, '2', 3.3))>,
            ext::boost::tuple_tag
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<decltype(make_cons(1, boost::tuples::null_type{}))>,
            ext::boost::tuple_tag
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<decltype(make_cons(1, make_cons('2', boost::tuples::null_type{})))>,
            ext::boost::tuple_tag
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<decltype(make_cons(1, boost::make_tuple('2', 3.3)))>,
            ext::boost::tuple_tag
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<boost::tuples::null_type>,
            ext::boost::tuple_tag
        >::value, "");
    }
}
