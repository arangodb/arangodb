// Copyright (c) 2018 Andrey Semashev
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// The test verifies that Boost.Optional copy constructors do not attempt to invoke
// the element type initializing constructors from templated arguments

#include <boost/optional/optional.hpp>
#include <boost/core/enable_if.hpp>

struct no_type
{
    char data;
};

struct yes_type
{
    char data[2];
};

template< unsigned int Size >
struct size_tag {};

template< typename T, typename U >
struct is_constructible
{
    static U& get();

    template< typename T1 >
    static yes_type check_helper(size_tag< sizeof(static_cast< T1 >(get())) >*);
    template< typename T1 >
    static no_type check_helper(...);

    static const bool value = sizeof(check_helper< T >(0)) == sizeof(yes_type);
};

template< typename T >
class wrapper
{
public:
    wrapper() {}
    wrapper(wrapper const&) {}
    template< typename U >
    wrapper(U const&, typename boost::enable_if_c< is_constructible< T, U >::value, int >::type = 0) {}
};

inline boost::optional< wrapper< int > > foo()
{
    return boost::optional< wrapper< int > >();
}

int main()
{
    // Invokes boost::optional copy constructor. Should not invoke wrapper constructor from U.
    boost::optional< wrapper< int > > res = foo();
    return 0;
}
