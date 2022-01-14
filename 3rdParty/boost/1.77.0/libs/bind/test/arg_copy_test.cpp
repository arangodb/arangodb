//
//  arg_copy_test.cpp - copying a custom placeholder _1 to arg<1>
//
//  Copyright 2016 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/is_placeholder.hpp>
#include <boost/bind/arg.hpp>

//

template<int I> struct ph
{
};

namespace boost
{

template<int I> struct is_placeholder< ::ph<I> >
{
    enum _vt { value = I };
};

} // namespace boost

int main()
{
    boost::arg<1> a1 = ph<1>();
    (void)a1;
}
