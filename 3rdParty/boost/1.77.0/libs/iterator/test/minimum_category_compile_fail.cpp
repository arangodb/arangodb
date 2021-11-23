// Copyright Andrey Semashev 2014.
//
// Use, modification and distribution is subject to
// the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/iterator/minimum_category.hpp>

using boost::iterators::minimum_category;

struct A {};
struct B {};

int main(int, char*[])
{
    minimum_category<A, B>::type cat;

    return 0;
}
