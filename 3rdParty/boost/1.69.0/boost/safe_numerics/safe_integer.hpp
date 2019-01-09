#ifndef BOOST_NUMERIC_SAFE_INTEGER_HPP
#define BOOST_NUMERIC_SAFE_INTEGER_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// not actually used here - but needed for integer arithmetic
// so this is a good place to include it
#include "checked_integer.hpp"

#include "safe_base.hpp"
#include "safe_base_operations.hpp"

#include "native.hpp"
#include "exception_policies.hpp"

// specialization for meta functions with safe<T> argument
namespace boost {
namespace safe_numerics {

template <
    class T,
    class P = native,
    class E = default_exception_policy
>
using safe = safe_base<
    T,
    ::std::numeric_limits<T>::min(),
    ::std::numeric_limits<T>::max(),
    P,
    E
>;

} // safe_numerics
} // boost


#endif // BOOST_NUMERIC_SAFE_INTEGER_HPP
