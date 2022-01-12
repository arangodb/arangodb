// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>

using namespace boost::variant2;

template<class... T> class X: variant<T...>
{
    using base = variant<T...>;
    using base::base;
};

struct Y
{
    Y( Y const& rhs ) = default;

    template<class T> Y( T const& t )
    {
        t.bar();
    }
};

int main()
{
    using W = X<int, double, Y>;

    W a( 1 );
    W b( a );

    (void)b;
}
