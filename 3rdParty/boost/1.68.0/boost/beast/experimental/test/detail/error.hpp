//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_DETAIL_ERROR_HPP
#define BOOST_BEAST_TEST_DETAIL_ERROR_HPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/core/string.hpp>

namespace boost {

namespace beast {
namespace test {
enum class error;
} // test
} // beast

namespace system {
template<>
struct is_error_code_enum<beast::test::error>
{
    static bool const value = true;
};

} // system

namespace beast {
namespace test {
namespace detail {

class error_codes : public error_category
{
public:
    const char*
    name() const noexcept override;

    std::string
    message(int ev) const override;

    error_condition
    default_error_condition(int ev) const noexcept override;
};

} // detail

error_code
make_error_code(error e);

} // test
} // beast

} // boost

#endif
