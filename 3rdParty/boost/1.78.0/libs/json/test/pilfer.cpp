//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/pilfer.hpp>

// VFALCO This fails to compile in msvc:
// https://godbolt.org/z/8q6K8e
#if 0
namespace boost {
namespace json {

inline namespace standalone {
namespace detail {
struct V{};
} // detail
} // standalone

namespace detail {
struct T{};
} // detail
struct U : detail::T{};

} // json
} // boost
#endif
