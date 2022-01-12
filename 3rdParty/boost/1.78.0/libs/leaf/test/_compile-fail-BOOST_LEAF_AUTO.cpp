// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/error.hpp>

namespace leaf = boost::leaf;

template <class T>
struct my_result
{
    my_result( T );
    my_result( std::error_code );
    T value() const;
    std::error_code error();
    explicit operator bool() const;
};

#if 0
namespace boost { namespace leaf {
    template <class T>
    struct is_result_type<my_result<T>>: std::true_type
    {
    };
} }
#endif

my_result<int> f();

my_result<int> g()
{
    BOOST_LEAF_AUTO(a, f());
    return a;
}
