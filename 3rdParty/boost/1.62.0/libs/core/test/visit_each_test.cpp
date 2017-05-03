//
// visit_each_test.cpp
//
// Copyright 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/visit_each.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

struct X
{
    int v;
    std::string w;
};

template<class Visitor> inline void visit_each( Visitor & visitor, X const & x )
{
    using boost::visit_each;

    visit_each( visitor, x.v );
    visit_each( visitor, x.w );
}

struct V
{
    int s_;

    V(): s_( 0 )
    {
    }

    template< class T > void operator()( T const & t )
    {
    }

    void operator()( int const & v )
    {
        s_ = s_ * 10 + v;
    }

    void operator()( std::string const & w )
    {
        s_ = s_ * 10 + w.size();
    }
};

int main()
{
    X x = { 5, "test" };
    V v;

    {
        using boost::visit_each;
        visit_each( v, x );
    }

    BOOST_TEST( v.s_ == 54 );

    return boost::report_errors();
}
