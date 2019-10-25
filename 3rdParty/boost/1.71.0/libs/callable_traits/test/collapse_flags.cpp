
/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <boost/callable_traits/detail/qualifier_flags.hpp>
#include "test.hpp"


using namespace boost::callable_traits;
using namespace boost::callable_traits::detail;

int main() {

    // boost::callable_traits::detail::collapse_flags emulates the C++11
    // reference collapsing rules. Here, we test that behavior.

    using rref_plus_lref = collapse_flags<rref_, lref_>;
    CT_ASSERT(rref_plus_lref::value == lref_);

    using lref_plus_rref = collapse_flags<lref_, rref_>;
    CT_ASSERT(lref_plus_rref::value == lref_);

    using lref_plus_lref = collapse_flags<lref_, lref_>;
    CT_ASSERT(lref_plus_lref::value == lref_);

    using rref_plus_rref = collapse_flags<rref_, rref_>;
    CT_ASSERT(rref_plus_rref::value == rref_);

    using const_plus_rref = collapse_flags<const_, rref_>;
    CT_ASSERT(const_plus_rref::value == (const_ | rref_));

    using const_plus_lref = collapse_flags<const_, lref_>;
    CT_ASSERT(const_plus_lref::value == (const_ | lref_));
}
