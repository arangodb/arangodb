// Copyright 2019 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2.hpp>

using namespace boost::variant2;

int main()
{
    variant<float, int> v( 2 );
    return get<1>( v ) == 2? 0: 1;
}
