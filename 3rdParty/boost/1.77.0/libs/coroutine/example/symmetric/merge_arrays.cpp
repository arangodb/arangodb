
//          Copyright Keld Helsgaun 2000, Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/coroutine/all.hpp>

#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <vector>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

typedef boost::coroutines::symmetric_coroutine< void >  coro_t;

class merger
{
private:
    std::size_t                 max_;
    std::vector< int >      &   to_;

    void run_( coro_t::yield_type & yield)
    {
        while ( idx < from.size() )
        {
            if ( other->from[other->idx] < from[idx])
                yield( other->coro);
            to_.push_back(from[idx++]);
        }
        while ( to_.size() < max_)
            to_.push_back( other->from[other->idx++]); 
    }

    merger( merger const&);
    merger & operator=( merger const&);

public:
    std::vector< int >  const&  from;
    std::size_t                 idx;
    merger                  *   other;
    coro_t::call_type           coro;

    merger( std::vector< int > const& from_, std::vector< int > & to, std::size_t max) :
        max_( max),
        to_( to),
        from( from_),
        idx( 0),
        other( 0),
        coro( boost::bind( & merger::run_, this, _1) )
    {}

    void run()
    { coro(); }
};

std::vector< int > merge( std::vector< int > const& a, std::vector< int > const& b)
{
    std::vector< int > c;
    merger ma( a, c, a.size() + b. size() );
    merger mb( b, c, a.size() + b. size() );

    ma.other = & mb;
    mb.other = & ma;

    ma.run();

    return c;
}

void print( std::string const& name, std::vector< int > const& v)
{
    std::cout << name << " : ";
    BOOST_FOREACH( int itm, v)
    { std::cout << itm << " "; }
    std::cout << "\n";
}

int main( int argc, char * argv[])
{
    std::vector< int > a;
    a.push_back( 1);
    a.push_back( 5);
    a.push_back( 6);
    a.push_back( 10);
    print( "a", a);

    std::vector< int > b;
    b.push_back( 2);
    b.push_back( 4);
    b.push_back( 7);
    b.push_back( 8);
    b.push_back( 9);
    b.push_back( 13);
    print( "b", b);

    std::vector< int > c = merge( a, b);
    print( "c", c);

    std::cout << "Done" << std::endl;

    return EXIT_SUCCESS;
}
