#ifndef BOOST_NUMERIC_CONCEPT_INTEGER_HPP
#define BOOST_NUMERIC_CONCEPT_INTEGER_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "numeric.hpp"

namespace boost {
namespace safe_numerics {

template <class T>
class Integer : public Numeric<T> {
    // integer types must have the corresponding numeric trait.
    static_assert(
        std::numeric_limits<T>::is_integer,
        "Fails to fulfill requirements for an integer type"
    );
};

} // safe_numerics
} // boost

#endif // BOOST_NUMERIC_CONCEPT_INTEGER_HPP
