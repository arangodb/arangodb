
//          Copyright Keld Helsgaun 2000, Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/coroutine/all.hpp>
#include <boost/move/move.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

typedef boost::coroutines::symmetric_coroutine< void >  coro_t;

class player
{
private:
    int die()
    {
        boost::random::uniform_int_distribution<> dist( 1, 6);
        return dist( gen);
    }

    void run_( coro_t::yield_type & yield)
    {
        int sum = 0;
        while ( ( sum += die() ) < 100)
            yield( nxt->coro);
        std::cout << "player " << id << " winns" << std::endl;
    }

    player( player const&);
    player & operator=( player const&);

public:
    int                             id;
    player                      *   nxt;
    coro_t::call_type               coro;
    boost::random::random_device    gen;

    player( int id_) :
        id( id_), nxt( 0),
        coro( boost::bind( & player::run_, this, _1) ),
        gen()
    {}

    void run()
    { coro(); }
};

int main( int argc, char * argv[])
{
    player * first = new player( 1);
    player * p = first;
    for ( int i = 2; i <= 4; ++i)
    {
        p->nxt = new player( i);
        p = p->nxt;
    }
    p->nxt = first;
    first->run();

    std::cout << "Done" << std::endl;

    return EXIT_SUCCESS;
}
