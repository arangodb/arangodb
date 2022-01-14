/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_strong_typedef.cpp

// (C) Copyright 2016 Ashish Sadanandan
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation
#include <stdlib.h>     // EXIT_SUCCESS

#include <boost/config.hpp>
#include <boost/serialization/strong_typedef.hpp>
#include <boost/static_assert.hpp>

#include <boost/type_traits/has_nothrow_assign.hpp>
#include <boost/type_traits/has_nothrow_constructor.hpp>
#include <boost/type_traits/has_nothrow_copy.hpp>

///////////////////////////////////////////////////////////////////////
// Define a strong typedef for int.
// The new type should be nothrow constructible and assignable.

BOOST_STRONG_TYPEDEF(int, strong_int)

#ifndef BOOST_HAS_NOTHROW_CONSTRUCTOR
BOOST_STATIC_ASSERT(boost::has_nothrow_default_constructor<strong_int>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy_constructor<strong_int>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<strong_int>::value);
#endif

///////////////////////////////////////////////////////////////////////
// strong_int can now be placed in another type, which can also be
// nothrow constructible and assignable.

struct type1
{
    long        some_long;
    strong_int  sint;
};

#ifndef BOOST_HAS_NOTHROW_CONSTRUCTOR
BOOST_STATIC_ASSERT(boost::has_nothrow_default_constructor<type1>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_copy_constructor<type1>::value);
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<type1>::value);
#endif

///////////////////////////////////////////////////////////////////////
// Now define a type that throws, and a strong_typedef for it
// The strong_typedef should also not have nothrow construction/assign.

struct not_noexcept
{
    not_noexcept() {}
    not_noexcept(not_noexcept const&) {}
    not_noexcept& operator=(not_noexcept const&) {return *this;}
    bool operator==(not_noexcept const&) const {return false;}
    bool operator<(not_noexcept const&) const {return false;}
};
BOOST_STRONG_TYPEDEF(not_noexcept, strong_not_noexcept)

#ifndef BOOST_HAS_NOTHROW_CONSTRUCTOR
BOOST_STATIC_ASSERT(! boost::has_nothrow_default_constructor<strong_not_noexcept>::value);
BOOST_STATIC_ASSERT(! boost::has_nothrow_copy_constructor<strong_not_noexcept>::value);
BOOST_STATIC_ASSERT(! boost::has_nothrow_assign<strong_not_noexcept>::value);
#endif

int main()
{
    return EXIT_SUCCESS;
}
