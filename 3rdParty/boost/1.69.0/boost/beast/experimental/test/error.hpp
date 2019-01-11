//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_ERROR_HPP
#define BOOST_BEAST_TEST_ERROR_HPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/experimental/test/detail/error.hpp>

namespace boost {
namespace beast {
namespace test {

/// Error codes returned from unit testing algorithms
enum class error
{
    /** The test stream generated a simulated testing error

        This error is returned by the test @ref stream when it
        generates a simulated error.
    */
    test_failure = 1
};

} // test
} // beast
} // boost

#include <boost/beast/experimental/test/impl/error.ipp>

#endif
