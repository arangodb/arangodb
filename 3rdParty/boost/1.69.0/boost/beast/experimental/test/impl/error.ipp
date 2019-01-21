//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_IMPL_ERROR_IPP
#define BOOST_BEAST_TEST_IMPL_ERROR_IPP

namespace boost {
namespace beast {
namespace test {

namespace detail {

inline
const char*
error_codes::
name() const noexcept
{
    return "boost.beast.test";
}

inline
std::string
error_codes::
message(int ev) const
{
    switch(static_cast<error>(ev))
    {
    default:
    case error::test_failure:   return "The test stream generated a simulated error";
    }
}

inline
error_condition
error_codes::
default_error_condition(int ev) const noexcept
{
    return error_condition{ev, *this};
}

} // detail

inline
error_code
make_error_code(error e)
{
    static detail::error_codes const cat{};
    return error_code{static_cast<
        std::underlying_type<error>::type>(e), cat};
}

} // test
} // beast
} // boost

#endif
