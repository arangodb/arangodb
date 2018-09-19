// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>

#include <cstddef>
#include <functional>
namespace hana = boost::hana;


// A tuple that holds reference_wrappers to its elements, instead of the
// elements themselves.

struct RefTuple { };

template <typename ...T>
struct ref_tuple;

template <typename ...T>
struct ref_tuple<T&...> {
    hana::tuple<std::reference_wrapper<T>...> storage_;
};


namespace boost { namespace hana {
    template <typename ...T>
    struct tag_of<ref_tuple<T...>> {
        using type = RefTuple;
    };

    template <>
    struct make_impl<RefTuple> {
        template <typename ...T>
        static constexpr auto apply(T& ...t) {
            return ref_tuple<T&...>{{std::ref(t)...}};
        }
    };

    template <>
    struct at_impl<RefTuple> {
        template <typename Xs, typename N>
        static constexpr decltype(auto) apply(Xs&& xs, N const& n) {
            return hana::at(static_cast<Xs&&>(xs).storage_, n).get();
        }
    };

    template <>
    struct is_empty_impl<RefTuple> {
        template <typename Xs>
        static constexpr auto apply(Xs const& xs) {
            return hana::is_empty(xs.storage_);
        }
    };

    template <>
    struct drop_front_impl<RefTuple> {
        template <std::size_t n, typename T, typename ...U, std::size_t ...i>
        static constexpr auto helper(ref_tuple<T, U...> xs, std::index_sequence<i...>) {
            return hana::make<RefTuple>(hana::at_c<n + i>(xs.storage_).get()...);
        }

        template <typename ...T, typename N>
        static constexpr auto apply(ref_tuple<T...> xs, N const&) {
            return helper<N::value>(xs, std::make_index_sequence<(
                N::value < sizeof...(T) ? sizeof...(T) - N::value : 0
            )>{});
        }
    };
}} // end namespace boost::hana


int main() {
    int i = 0, j = 1, k = 2;

    ref_tuple<int&, int&, int&> refs = hana::make<RefTuple>(i, j, k);
    hana::at_c<0>(refs) = 3;
    BOOST_HANA_RUNTIME_CHECK(i == 3);

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(refs)));

    ref_tuple<int&, int&> tail = hana::drop_front(refs);
    hana::at_c<0>(tail) = 4;
    BOOST_HANA_RUNTIME_CHECK(j == 4);
}
