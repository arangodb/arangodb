//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_FAIL_COUNTER_HPP
#define BOOST_BEAST_TEST_FAIL_COUNTER_HPP

#include <boost/beast/core/error.hpp>
#include <boost/throw_exception.hpp>

namespace boost {
namespace beast {
namespace test {

enum class error
{
    fail_error = 1
};

namespace detail {

class fail_error_category : public boost::system::error_category
{
public:
    const char*
    name() const noexcept override
    {
        return "test";
    }

    std::string
    message(int ev) const override
    {
        switch(static_cast<error>(ev))
        {
        default:
        case error::fail_error:
            return "test error";
        }
    }

    boost::system::error_condition
    default_error_condition(int ev) const noexcept override
    {
        return boost::system::error_condition{ev, *this};
    }

    bool
    equivalent(int ev,
        boost::system::error_condition const& condition
            ) const noexcept override
    {
        return condition.value() == ev &&
            &condition.category() == this;
    }

    bool
    equivalent(error_code const& error, int ev) const noexcept override
    {
        return error.value() == ev &&
            &error.category() == this;
    }
};

inline
boost::system::error_category const&
get_error_category()
{
    static fail_error_category const cat{};
    return cat;
}

} // detail

inline
error_code
make_error_code(error ev)
{
    return error_code{
        static_cast<std::underlying_type<error>::type>(ev),
            detail::get_error_category()};
}

/** An error code with an error set on default construction

    Default constructed versions of this object will have
    an error code set right away. This helps tests find code
    which forgets to clear the error code on success.
*/
struct fail_error_code : error_code
{
    fail_error_code()
        : error_code(make_error_code(error::fail_error))
    {
    }

    template<class Arg0, class... ArgN>
    fail_error_code(Arg0&& arg0, ArgN&&... argn)
        : error_code(arg0, std::forward<ArgN>(argn)...)
    {
    }
};

/** A countdown to simulated failure.

    On the Nth operation, the class will fail with the specified
    error code, or the default error code of @ref error::fail_error.
*/
class fail_counter
{
    std::size_t n_;
    std::size_t i_ = 0;
    error_code ec_;

public:
    fail_counter(fail_counter&&) = default;

    /** Construct a counter.

        @param The 0-based index of the operation to fail on or after.
    */
    explicit
    fail_counter(std::size_t n,
            error_code ev = make_error_code(error::fail_error))
        : n_(n)
        , ec_(ev)
    {
    }

    /// Returns the fail index
    std::size_t
    count() const
    {
        return n_;
    }

    /// Throw an exception on the Nth failure
    void
    fail()
    {
        if(i_ < n_)
            ++i_;
        if(i_ == n_)
            BOOST_THROW_EXCEPTION(system_error{ec_});
    }

    /// Set an error code on the Nth failure
    bool
    fail(error_code& ec)
    {
        if(i_ < n_)
            ++i_;
        if(i_ == n_)
        {
            ec = ec_;
            return true;
        }
        ec.assign(0, ec.category());
        return false;
    }
};

} // test
} // beast
} // boost

namespace boost {
namespace system {
template<>
struct is_error_code_enum<beast::test::error>
{
    static bool const value = true;
};
} // system
} // boost

#endif
