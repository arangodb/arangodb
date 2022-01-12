//
// Incompatible reference_wrappers must not implicitly convert to each other
//
// Copyright 2020 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/ref.hpp>

void f( boost::reference_wrapper< int const > )
{
}

int main()
{
    long y = 0;
    f( boost::ref(y) ); // should fail
}
