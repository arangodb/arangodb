//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_ERROR_HPP
#define BOOST_BEAST_ERROR_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

namespace boost {
namespace beast {

/// The type of error code used by the library
using error_code = boost::system::error_code;

/// The type of system error thrown by the library
using system_error = boost::system::system_error;

/// The type of error category used by the library
using error_category = boost::system::error_category;

/// A function to return the generic error category used by the library
#if BOOST_BEAST_DOXYGEN
error_category const&
generic_category();
#else
using boost::system::generic_category;
#endif

/// A function to return the system error category used by the library
#if BOOST_BEAST_DOXYGEN
error_category const&
system_category();
#else
using boost::system::system_category;
#endif

/// The type of error condition used by the library
using error_condition = boost::system::error_condition;

/// The set of constants used for cross-platform error codes
#if BOOST_BEAST_DOXYGEN
enum errc{};
#else
namespace errc = boost::system::errc;
#endif

} // beast
} // boost

#endif
