// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/algorithm.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/mult.hpp>
namespace hana = boost::hana;


// The algorithms are taken from the suggested implementations on cppreference.
// Hence, we assume them to be correct and we only make sure they compile, to
// avoid stupid mistakes I could have made when copy/pasting and editing.
//
// Oh, and we also make sure they can be used in a constexpr context.
constexpr bool constexpr_context() {
    int x = 0, y = 1;
    hana::detail::constexpr_swap(x, y);

    int array[6] = {1, 2, 3, 4, 5, 6};
    int* first = array;
    int* last = array + 6;

    hana::detail::reverse(first, last);

    hana::detail::next_permutation(first, last, hana::less);
    hana::detail::next_permutation(first, last);

    hana::detail::lexicographical_compare(first, last, first, last, hana::less);
    hana::detail::lexicographical_compare(first, last, first, last);

    hana::detail::equal(first, last, first, last, hana::equal);
    hana::detail::equal(first, last, first, last);

    hana::detail::sort(first, last, hana::equal);
    hana::detail::sort(first, last);

    hana::detail::find(first, last, 3);
    hana::detail::find_if(first, last, hana::equal.to(3));

    hana::detail::iota(first, last, 0);

    hana::detail::count(first, last, 2);

    hana::detail::accumulate(first, last, 0);
    hana::detail::accumulate(first, last, 1, hana::mult);

    hana::detail::min_element(first, last);

    return true;
}

static_assert(constexpr_context(), "");

int main() { }
