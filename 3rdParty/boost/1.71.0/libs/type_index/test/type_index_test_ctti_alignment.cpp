//
// Copyright 2012-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_index/ctti_type_index.hpp>
#include <boost/type_traits/alignment_of.hpp>

int main() {
    BOOST_STATIC_ASSERT_MSG(
        boost::alignment_of<boost::typeindex::detail::ctti_data>::value == boost::alignment_of<char>::value,
        "Alignments of boost::typeindex::detail::ctti_data and char differ. "
        "It is unsafe to reinterpret_cast between them."
    );
}

