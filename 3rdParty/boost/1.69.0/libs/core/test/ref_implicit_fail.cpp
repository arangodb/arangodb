//
// Rvalues should not implicitly convert to a reference_wrapper
//
// Copyright 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/ref.hpp>

void f( boost::reference_wrapper< int > )
{
}

int main()
{
    f( 1 ); // should fail
}
