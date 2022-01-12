// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/detail/function_traits.hpp>
#endif

#include <functional>

namespace leaf = boost::leaf;

template <class F>
void check_traits( F )
{
    using namespace leaf::leaf_detail;
    using leaf::leaf_detail_mp11::mp_list;
    static_assert(function_traits<F>::arity==4,"arity");
    static_assert(std::is_same<fn_return_type<F>,double>::value,"return_type");
    static_assert(std::is_same<fn_arg_type<F,0>,int>::value,"arg<0>");
    static_assert(std::is_same<fn_arg_type<F,1>,float>::value,"arg<1>");
    static_assert(std::is_same<fn_arg_type<F,2>,int const &>::value,"arg<2>");
    static_assert(std::is_same<fn_arg_type<F,3>,float &&>::value,"arg<3>");
    static_assert(std::is_same<fn_mp_args<F>,mp_list<int,float,int const &,float &&>>::value,"mp_args");
}

double f1( int, float, int const &, float && )
{
    return 42;
}

int main()
{
    check_traits(&f1);
    check_traits(std::function<double(int const volatile, float const, int const &, float &&)>(f1));
    check_traits( []( int const volatile, float const, int const &, float && ) -> double
        {
            return 42;
        } );
    check_traits( []( int const volatile, float const, int const &, float && ) noexcept -> double
        {
            return 42;
        } );
    static_assert(leaf::leaf_detail::function_traits<int>::arity==-1, "int arity");
    return 0;
}
