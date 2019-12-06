//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/is_invocable.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace detail {

namespace {

//
// is_invocable
//

struct is_invocable_udt1
{
    void operator()(int) const;
};

struct is_invocable_udt2
{
    int operator()(int) const;
};

struct is_invocable_udt3
{
    int operator()(int);
};

struct is_invocable_udt4
{
    void operator()(std::unique_ptr<int>);
};

#ifndef __INTELLISENSE__
// VFALCO Fails to compile with Intellisense
BOOST_STATIC_ASSERT(is_invocable<is_invocable_udt1, void(int)>::value);
BOOST_STATIC_ASSERT(is_invocable<is_invocable_udt2, int(int)>::value);
BOOST_STATIC_ASSERT(is_invocable<is_invocable_udt3, int(int)>::value);
BOOST_STATIC_ASSERT(! is_invocable<is_invocable_udt1, void(void)>::value);
BOOST_STATIC_ASSERT(! is_invocable<is_invocable_udt2, int(void)>::value);
BOOST_STATIC_ASSERT(! is_invocable<is_invocable_udt2, void(void)>::value);
BOOST_STATIC_ASSERT(! is_invocable<is_invocable_udt3 const, int(int)>::value);
BOOST_STATIC_ASSERT(is_invocable<is_invocable_udt4, void(std::unique_ptr<int>)>::value);
#endif

} // (anonymous)

} // detail
} // beast
} // boost
