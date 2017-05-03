// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_STRUCT_MINIMAL_STRUCT_HPP
#define BOOST_HANA_TEST_STRUCT_MINIMAL_STRUCT_HPP

#include <boost/hana/at.hpp>
#include <boost/hana/fwd/accessors.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/unpack.hpp>


template <int N>
struct minimal_struct_tag;

template <typename ...Members>
struct minimal_struct_t {
    boost::hana::tuple<Members...> members;
    using hana_tag = minimal_struct_tag<sizeof...(Members)>;
};

struct obj_t {
    template <typename ...Members>
    constexpr minimal_struct_t<Members...> operator()(Members ...members) const {
        return {{static_cast<Members&&>(members)...}};
    }
};
constexpr obj_t obj{};

namespace boost { namespace hana {
    template <int N>
    struct accessors_impl<minimal_struct_tag<N>> {
        template <typename K>
        struct get_member {
            template <typename Struct>
            constexpr decltype(auto) operator()(Struct&& s) const {
                return hana::at_c<K::value>(static_cast<Struct&&>(s).members);
            }
        };

        static auto apply() {
            return hana::unpack(hana::range_c<int, 0, N>, [](auto ...k) {
                return hana::make_tuple(
                    hana::make_pair(k, get_member<decltype(k)>{})...
                );
            });
        }
    };
}}

#endif // !BOOST_HANA_TEST_STRUCT_MINIMAL_STRUCT_HPP
