// Copyright 2009 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dynarray.h"
#include <unordered_set>

#include <atomic>
#include <mutex>

namespace gcl {

/*


INTRODUCTION

This file implements highly concurrent distributed counters.
The intent is to minimize the cost of incrementing the counter,
accepting increased costs to obtain the count.
That is, these counters are appropriate to code with
very frequent counter increments but relatively rare counter reads.

These counters are parameterized by the base integer type
that maintains the count.
Avoid situations that overflow the integer,
as that may have undefined behavior.
This constraint implies that counters must be sized to their use.


GENERAL METHODS

The general counter methods are as follows.

<constructor>( integer ):
The parameter is the initial counter value.

<constructor>():
Equivalent to an initial value of zero.

void operator +=( integer ):
void operator -=( integer ):
Add/subtract a value to/front the counter.
There is no default value.

void operator ++():    // prefix
void operator ++(int): // postfix
void operator --():    // prefix
void operator --(int): // postfix
Increment or decrement the counter.

integer load():
Returns the value of the counter.

integer exchange( integer ):
Replaces the existing count by the count in the parameter
and returns the previous count,
which enables safe concurrent count extraction.

There are no copy or assignment operations.


SIMPLEX COUNTERS

The simplex counters provide low-latency counting.
They implement all the general operations.

    counter::simplex<int> red_count;

    void count_red( Bag bag ) {
        for ( Bag::iterator i = bag.begin(); i != bag.end(); i++ )
            if ( is_red( *i ) )
                ++red_count;
    }

Note that this code may have significant cost
because of the repeated global atomic increments.
Using a temporary int to maintain the count within the loop
runs the risk of loosing some counts in the event of an exception.


COUNTER BUFFERS

The cost of incrementing the counter is reduced
by using a buffer as a proxy for the counter.
The counter is a reference parameter to the buffer.
This buffer is typically accessed by a single thread,
and so its cost can be substantially lower.

    counter::simplex<int> red_count;

    void count_red( Bag bag ) {
        counter::buffer<int> local_red( red_count );
        for ( Bag::iterator i = bag.begin(); i != bag.end(); i++ )
            if ( is_red( *i ) )
                ++local_red;
    }

The buffer will automatically transfer its count
to the main counter on destruction.
If this latency is too great,
use the push method to transfer the count early.

    void count_red( Bag bag1, Bag bag2 ) {
        counter::buffer<int> local_red( red_count );
        for ( Bag::iterator i = bag1.begin(); i != bag1.end(); i++ )
            if ( is_red( *i ) )
                ++local_red;
        local_red.push();
        for ( Bag::iterator i = bag2.begin(); i != bag2.end(); i++ )
            if ( is_red( *i ) )
                ++local_red;
    }

Any increments on buffers since the last push
will not be reflected in the value reported by a load of the counter.
The destructor does an implicit push.
The lifetime of the counter must be strictly larger than
the lifetimes of any buffers attached to it.


DUPLEX COUNTERS

The push model of buffers sometimes yields an unacceptable lag
in the observed value of the count.
To avoid this lag,
there are duplex counters.
A duplex counter is paired with a broker,
the counter can query the broker for counts,
thus maintaining a somewhat current view of the count.

    counter::strong_duplex<int> red_count;

    void count_red( Bag bag )
        counter::strong_broker broker( red_count );
        for ( Bag::iterator i = bag.begin(); i != bag.end(); i++ )
            if ( is_red( *i ) )
                ++broker;
    }

Another thread may call red_count.load() and get the current count.
That operation will poll each broker for its count and return the sum.
Naturally, any increments done to a broker after it is polled will be missed,
but no counts will be lost.

The primary use case for duplex counters
is to enable fast thread-local increments
while still maintaining a decent global count.

    weak_duplex<int> red_count;
    thread_local weak_broker<int> thread_red( red_count );

    void count_red( Bag bag )
        for ( Bag::iterator i = bag.begin(); i != bag.end(); i++ )
            if ( is_red( *i ) )
                ++thread_red;
    }


WEAK DUPLEX COUNTERS

The exchange operation works
by atomically transfering broker counts to the main counter,
and then exchanging out of the main counter.
Consequently,
every count will be extracted by one and only one exchange operation.

However, that exchange operation can be expensive because 
it and the broker increment operations
require write atomicity to the same broker object.
To reduce that concurrency cost,
the weak_duplex counter and its weak_broker
do not provide the exchange operation.
This difference
means that the polling is a read-only operation
and requires less synchronization.
Use this counter when you do not intend to exchange values.


BUFFERING BROKERS

Thread-local increments, while cheaper than shared global increments,
are still more expensive than function-local increments.
To mitigate that cost, buffers work with brokers as well.

    weak_duplex<int> red_count;
    thread_local weak_broker<int> thread_red( red_count );

    void count_red( Bag bag )
        buffer<int> local_red( thread_red );
        for ( Bag::iterator i = bag.begin(); i != bag.end(); i++ )
            if ( is_red( *i ) )
                ++local_red;
    }

As with buffers in general,
the count transfers only on destruction a push operation.
Consequently, red_count.load() will not reflect any counts in buffers.
Those counts will not be lost, though.


COUNTER ARRAYS

Counter arrays provide a means to handle many counters with one name.
The size of the counter arrays is fixed at construction time.
Counter arrays have the same structure as single-value counters,
with the following exceptions.

Increment a counter by indexing its array
and then applying one of the operations.
E.g. ++ctr_ary[i].

The load and exchange operations take an additional index parameter.

Do we want to initialize a counter array with an initializer list?
Do we want to return a dynarray for the load operation?
Do we want to pass and return a dynarray for the exchange operation?


ATOMICITY

In the course of program evolution, debugging and tuning,
a counter may desire implementation with weaker concurrency requirements.
That is accomplished by explicitly specifing the atomicity.
For example, suppose it is discovered that a counter

    counter::simplex<int> red_count;

is only ever read and written from a single thread.
We can avoid the cost of atomic operations by making the counter serial.

    counter::simplex<int, counter::atomicity::none> red_count;

This approach preserves the programming interface.

The counters support three levels of atomicity.
These are specified as a (usually defaulted) template parameter.

counter::atomicity::none // only one thread may access the counter
counter::atomicity::semi // multiple readers, but only one writer
counter::atomicity::full // multiple readers and writers

Buffers have two template parameters for atomicity,
one for the atomicity of the counter it is modifying,
and one for atomicity of the buffer itself.
By exlicitly specifying this atomicity,
one can build unusual configurations of buffers for unusual situations.
For example, suppose increments of red_count
tend to cluster in tasks with high processor affinity.
By separating those counts with a global intermediate buffer,
counting can exploit processor locality
and avoid substantial inter-processor communication.
For example,

    counter::simplex<int> red_count;
    counter::buffer<int, counter::atomicity::full, counter::atomicity::full>
        red_squares( red_count );
    counter::buffer<int, counter::atomicity::full, counter::atomicity::full>
        red_circles( red_count );

    void count_red_squares( Bag bag ) {
        counter::buffer<int> local_red( red_squares );
        for ( Bag::iterator i = bag.begin(); i != bag.end(); i++ )
            if ( is_red( *i ) )
                ++local_red;
    }

The red_squares variable is global,
and will automatically transfer its count to red_count
only on global variable destruction
This transfer is likely to be too late for many purposes.
One solution is explicit programming to call red_squares.push()
at appropriate times.
Another solution is to use duplex counters.


GUIDELINES FOR USE

Use a simplex counter
when you have a low rate of updates or a high read rate
or little tolerance for latency.
Use a strong duplex counter and broker
when your update rate is significantly higher than the load rate,
you can tolerate latency in counting,
and you need the exchange operation.
Use a weak duplex counter and broker
when your update rate is significantly higher than the load rate,
you can tolerate latency in counting,
but you do not need the exchange operation.
Use buffers to collect short-term bursts of counts.

The operations of the counters, brokers, and buffers
have the following rough costs.


                                strong          weak
                simplex         duplex          duplex

update          atomic rmw      atomic rmw      atomic rmw

load            atomic read     mutex + n *     mutex + n *
                                atomic read     atomic read

exchange        atomic rmw      mutex + n *     n/a
                                atomic rmw

construction    trivial         std::set        std::set

destruction     trivial         std::set        std::set


                ==              ==              ==

                                strong          weak
                buffer          broker          broker

update          serial          atomic rmw      atomic
                read & write                    read & write

construction    pointer         mutex +         mutex +
                assign          set.insert      set.insert

destruction     pointer         mutex +         mutex +
                assign          set.remove      set.remove


IMPLEMENTATION

The lowest implementation layer level of the organization is a bumper.
Bumpers provide the interface of simplex counter,
but only the increment and decrement interface is public.
The rest are protected.
Buffer constructors require a reference to a bumper.
Simplex counters, buffers, duplex counters, and buffers
are all derived from a bumper,
which enables buffers to connect to all of them.


*/

/* Implementation .... */

/*
   The operation defined out-of-line
   break cyclic dependences in the class definitions.
   They are expected to be rarely called, and efficiency is less of a concern.
*/

namespace counter {

enum class atomicity
{
    none, // permits access to only one thread
    semi, // allows multiple readers, but only one writer
    full  // allows multiple readers and writers
};

/*
   The bumper classes provide the minimal increment and decrement interface.
   They serve as base classes for the public types.
*/

template< typename Integral, atomicity Atomicity >
class bumper;

template< typename Integral >
class bumper< Integral, atomicity::none >
{
    bumper( const bumper& ) = delete;
    bumper& operator=( const bumper& ) = delete;
public:
    void operator +=( Integral by ) { value_ += by; }
    void operator -=( Integral by ) { value_ -= by; }
    void operator ++() { *this += 1; }
    void operator ++(int) { *this += 1; }
    void operator --() { *this -= 1; }
    void operator --(int) { *this -= 1; }
protected:
    constexpr bumper( Integral in ) : value_( in ) {}
    constexpr bumper() : value_( 0 ) {}
    Integral load() const { return value_; }
    Integral exchange( Integral to )
        { Integral tmp = value_; value_ = to; return tmp; }
    Integral value_;
    template< typename, atomicity >
    friend class bumper_array;
    template< typename, atomicity, atomicity >
    friend class buffer_array;
    friend struct std::dynarray< bumper >;
};

template< typename Integral >
class bumper< Integral, atomicity::semi >
{
    bumper( const bumper& ) = delete;
    bumper& operator=( const bumper& ) = delete;
public:
    void operator +=( Integral by )
        { value_.store( value_.load( std::memory_order_relaxed ) + by,
                        std::memory_order_relaxed ); }
    void operator -=( Integral by )
        { value_.store( value_.load( std::memory_order_relaxed ) - by,
                        std::memory_order_relaxed ); }
    void operator ++() { *this += 1; }
    void operator ++(int) { *this += 1; }
    void operator --() { *this -= 1; }
    void operator --(int) { *this -= 1; }
protected:
    constexpr bumper( Integral in ) : value_( in ) {}
    constexpr bumper() : value_( 0 ) {}
    Integral load() const { return value_.load( std::memory_order_relaxed ); }
    Integral exchange( Integral to )
        { Integral tmp = value_.load( std::memory_order_relaxed );
          value_.store( to, std::memory_order_relaxed ); return tmp; }
    std::atomic< Integral > value_;
    template< typename, atomicity >
    friend class bumper_array;
    template< typename, atomicity, atomicity >
    friend class buffer_array;
    friend struct std::dynarray< bumper >;
};

template< typename Integral >
class bumper< Integral, atomicity::full >
{
    bumper( const bumper& ) = delete;
    bumper& operator=( const bumper& ) = delete;
public:
    void operator +=( Integral by )
        { value_.fetch_add( by, std::memory_order_relaxed ); }
    void operator -=( Integral by )
        { value_.fetch_sub( by, std::memory_order_relaxed ); }
    void operator ++() { *this += 1; }
    void operator ++(int) { *this += 1; }
    void operator --() { *this -= 1; }
    void operator --(int) { *this -= 1; }
protected:
    constexpr bumper( Integral in ) : value_( in ) {}
    constexpr bumper() : value_( 0 ) {}
    Integral load() const { return value_.load( std::memory_order_relaxed ); }
    Integral exchange( Integral to )
        { return value_.exchange( to, std::memory_order_relaxed ); }
    std::atomic< Integral > value_;
    template< typename, atomicity >
    friend class bumper_array;
    template< typename, atomicity, atomicity >
    friend class buffer_array;
    friend struct std::dynarray< bumper >;
};

/*
   The simplex counters.
*/

template< typename Integral,
          atomicity Atomicity = atomicity::full >
class simplex
: public bumper< Integral, Atomicity >
{
    typedef bumper< Integral, Atomicity > base_type;
public:
    constexpr simplex() : base_type( 0 ) {}
    constexpr simplex( Integral in ) : base_type( in ) {}
    simplex( const simplex& ) = delete;
    simplex& operator=( const simplex& ) = delete;
    Integral load() const { return base_type::load(); }
    Integral exchange( Integral to ) { return base_type::exchange( to ); }
};

/*
   Buffers reduce contention on counters.
   They require template parameters to specify the atomicity
   of the prime counter and of the buffer itself.
   The lifetime of the prime must cover the lifetime of the buffer.
   Any increments in the buffer since the last buffer::push call
   are not reflected in the queries on the prime.
*/

template< typename Integral,
          atomicity PrimeAtomicity = atomicity::full,
          atomicity BufferAtomicity = atomicity::none >
class buffer
: public bumper< Integral, BufferAtomicity >
{
    typedef bumper< Integral, PrimeAtomicity > prime_type;
    typedef bumper< Integral, BufferAtomicity > base_type;
public:
    buffer() = delete;
    buffer( prime_type& p ) : base_type( 0 ), prime_( p ) {}
    buffer( const buffer& ) = delete;
    buffer& operator=( const buffer& ) = delete;
    void push()
        { Integral value = base_type::exchange( 0 );
          if ( value != 0 ) prime_ += value; }
    ~buffer() { push(); }
private:
    prime_type& prime_;
};

/*
   Duplex counters enable a "pull" model of counting.
   Each counter, the prime, may have one or more brokers.
   The lifetime of the prime must cover the lifetime of the brokers.
   The duplex counter queries may fail to detect any counting done concurrently.
   The query operations are expected to be rare relative to the counting.
   The weak duplex counter does not support the exchange operation,
   which means that you cannot extract counts early.
*/

template< typename Integral > class strong_broker;

template< typename Integral > class strong_duplex
: public bumper< Integral, atomicity::full >
{
    typedef bumper< Integral, atomicity::full > base_type;
    typedef strong_broker< Integral > broker_type;
    friend class strong_broker< Integral >;
public:
    strong_duplex() : base_type( 0 ) {}
    strong_duplex( Integral in ) : base_type( in ) {}
    Integral load() const;
    Integral exchange( Integral to );
    ~strong_duplex();
private:
    void insert( broker_type* child );
    void erase( broker_type* child, Integral by );
    mutable std::mutex serializer_;
    typedef std::unordered_set< broker_type* > set_type;
    set_type children_;
};

template< typename Integral > class strong_broker
: public bumper< Integral, atomicity::full >
{
    typedef bumper< Integral, atomicity::full > base_type;
    typedef strong_duplex< Integral > duplex_type;
    friend class strong_duplex< Integral >;
public:
    strong_broker( duplex_type& p );
    strong_broker() = delete;
    strong_broker( const strong_broker& ) = delete;
    strong_broker& operator=( const strong_broker& ) = delete;
    ~strong_broker();
private:
    Integral poll() const { return base_type::load(); }
    Integral drain() { return base_type::exchange( 0 ); }
    duplex_type& prime_;
};

template< typename Integral >
void strong_duplex< Integral >::insert( broker_type* child )
{
    std::lock_guard< std::mutex > _( serializer_ );
    assert( children_.insert( child ).second );
}

template< typename Integral >
void strong_duplex< Integral >::erase( broker_type* child, Integral by )
{
    this->operator +=( by );
    std::lock_guard< std::mutex > _( serializer_ );
    assert( children_.erase( child ) == 1 );
}

template< typename Integral >
Integral strong_duplex< Integral >::load() const
{
    typedef typename set_type::iterator iterator;
    Integral tmp = 0;
    {
        std::lock_guard< std::mutex > _( serializer_ );
        iterator rollcall = children_.begin();
        for ( ; rollcall != children_.end(); ++rollcall )
            tmp += (*rollcall)->poll();
    }
    return tmp + base_type::load();
}

template< typename Integral >
Integral strong_duplex< Integral >::exchange( Integral to )
{
    typedef typename set_type::iterator iterator;
    Integral tmp = 0;
    {
        std::lock_guard< std::mutex > _( serializer_ );
        iterator rollcall = children_.begin();
        for ( ; rollcall != children_.end(); ++rollcall )
            tmp += (*rollcall)->drain();
    }
    return tmp + base_type::exchange( to );
}

template< typename Integral >
strong_duplex< Integral >::~strong_duplex()
{
    std::lock_guard< std::mutex > _( serializer_ );
    assert( children_.size() == 0 );
}

template< typename Integral >
strong_broker< Integral >::strong_broker( duplex_type& p )
:
    base_type( 0 ),
    prime_( p )
{
    prime_.insert( this );
}

template< typename Integral >
strong_broker< Integral >::~strong_broker()
{
    prime_.erase( this, base_type::load() );
}

template< typename Integral > class weak_broker;

template< typename Integral > class weak_duplex
: public bumper< Integral, atomicity::full >
{
    typedef bumper< Integral, atomicity::full > base_type;
    typedef weak_broker< Integral > broker_type;
    friend class weak_broker< Integral >;
public:
    weak_duplex() : base_type( 0 ) {}
    weak_duplex( Integral in ) : base_type( in ) {}
    weak_duplex( const weak_duplex& ) = delete;
    weak_duplex& operator=( const weak_duplex& ) = delete;
    Integral load() const;
    ~weak_duplex();
private:
    void insert( broker_type* child );
    void erase( broker_type* child, Integral by );
    mutable std::mutex serializer_;
    typedef std::unordered_set< broker_type* > set_type;
    set_type children_;
};

template< typename Integral > class weak_broker
: public bumper< Integral, atomicity::semi >
{
    typedef bumper< Integral, atomicity::semi > base_type;
    typedef weak_duplex< Integral > duplex_type;
    friend class weak_duplex< Integral >;
public:
    weak_broker( duplex_type& p );
    weak_broker() = delete;
    weak_broker( const weak_broker& ) = delete;
    weak_broker& operator=( const weak_broker& ) = delete;
    ~weak_broker();
private:
    Integral poll() const { return base_type::load(); }
    duplex_type& prime_;
};

template< typename Integral >
void weak_duplex< Integral >::insert( broker_type* child )
{
    std::lock_guard< std::mutex > _( serializer_ );
    assert( children_.insert( child ).second );
}

template< typename Integral >
void weak_duplex< Integral >::erase( broker_type* child, Integral by )
{
    std::lock_guard< std::mutex > _( serializer_ );
    this->operator +=( by );
    assert( children_.erase( child ) == 1 );
}

template< typename Integral >
Integral weak_duplex< Integral >::load() const
{
    typedef typename set_type::iterator iterator;
    Integral tmp = 0;
    {
        std::lock_guard< std::mutex > _( serializer_ );
        iterator rollcall = children_.begin();
        for ( ; rollcall != children_.end(); ++rollcall )
            tmp += (*rollcall)->poll();
        tmp += base_type::load();
    }
    return tmp;
}

template< typename Integral >
weak_duplex< Integral >::~weak_duplex()
{
    std::lock_guard< std::mutex > _( serializer_ );
    assert( children_.size() == 0 );
}

template< typename Integral >
weak_broker< Integral >::weak_broker( duplex_type& p )
:
    base_type( 0 ),
    prime_( p )
{
    prime_.insert( this );
}

template< typename Integral >
weak_broker< Integral >::~weak_broker()
{
    prime_.erase( this, base_type::load() );
}


// Counter arrays.


template< typename Integral,
          atomicity Atomicity = atomicity::full >
class bumper_array
{
public:
    typedef bumper< Integral, Atomicity > value_type;
private:
    typedef std::dynarray< value_type > storage_type;
public:
    typedef typename storage_type::size_type size_type;
    bumper_array() = delete;
    bumper_array( size_type size ) : storage( size ) {}
    bumper_array( const bumper_array& ) = delete;
    bumper_array& operator=( const bumper_array& ) = delete;
    value_type& operator[]( size_type idx ) { return storage[ idx ]; }
    size_type size() const { return storage.size(); }
protected:
    Integral load( size_type idx ) const { return storage[ idx ].load(); }
    Integral exchange( size_type idx, Integral value )
        { return storage[ idx ].exchange( value ); }
private:
    storage_type storage;
};

template< typename Integral,
          atomicity Atomicity = atomicity::full >
class simplex_array
: public bumper_array< Integral, Atomicity >
{
    typedef bumper_array< Integral, Atomicity > base_type;
public:
    typedef typename base_type::value_type value_type;
    typedef typename base_type::size_type size_type;
    simplex_array() = delete;
    constexpr simplex_array( size_type size ) : base_type( size ) {}
    simplex_array( const simplex_array& ) = delete;
    simplex_array& operator=( const simplex_array& ) = delete;
    Integral load( size_type idx ) const{ return base_type::load( idx ); }
    Integral exchange( size_type idx, Integral value )
        { return base_type::exchange( idx, value ); }
    value_type& operator[]( size_type idx ) { return base_type::operator[]( idx ); }
    size_type size() const { return base_type::size(); }
};

template< typename Integral,
          atomicity PrimeAtomicity = atomicity::full,
          atomicity BufferAtomicity = atomicity::full >
class buffer_array
: public bumper_array< Integral, BufferAtomicity >
{
    typedef bumper_array< Integral, BufferAtomicity > base_type;
    typedef bumper_array< Integral, PrimeAtomicity > prime_type;
public:
    typedef typename base_type::value_type value_type;
    typedef typename base_type::size_type size_type;
    buffer_array() = delete;
    buffer_array( prime_type& p ) : base_type( p.size() ), prime_( p ) {}
    buffer_array( const buffer_array& ) = delete;
    buffer_array& operator=( const buffer_array& ) = delete;
    void push( size_type idx )
        { Integral value = base_type::operator[]( idx ).exchange( 0 );
          if ( value != 0 ) prime_[ idx ] += value; }
    void push();
    size_type size() const { return base_type::size(); }
    ~buffer_array() { push(); }
private:
    prime_type& prime_;
};

template< typename Integral,
          atomicity BufferAtomicity, atomicity PrimeAtomicity >
void
buffer_array< Integral, BufferAtomicity, PrimeAtomicity >::push()
{
    int size = base_type::size();
    for ( int i = 0; i < size; ++i )
        push( i );
}

// Duplex arrays

template< typename Integral > class strong_broker_array;

template< typename Integral > class strong_duplex_array
: public bumper_array< Integral, atomicity::full >
{
    typedef bumper_array< Integral, atomicity::full > base_type;
    typedef strong_broker_array< Integral > broker_type;
    friend class strong_broker_array< Integral >;
public:
    typedef typename base_type::value_type value_type;
    typedef typename base_type::size_type size_type;
    strong_duplex_array() = delete;
    constexpr strong_duplex_array( size_type size )
      : base_type( size ) {}
    strong_duplex_array( const strong_duplex_array& ) = delete;
    strong_duplex_array& operator=( const strong_duplex_array& ) = delete;
    value_type& operator[]( size_type idx ) { return base_type::operator[]( idx ); }
    size_type size() const { return base_type::size(); }
    ~strong_duplex_array();
private:
    void insert( broker_type* child );
    void erase( broker_type* child, Integral by );
    mutable std::mutex serializer_;
    typedef std::unordered_set< broker_type* > set_type;
    set_type children_;
};

template< typename Integral > class strong_broker_array
: public bumper_array< Integral, atomicity::semi >
{
    typedef bumper_array< Integral, atomicity::semi > base_type;
    typedef strong_duplex_array< Integral > duplex_type;
    friend class strong_duplex_array< Integral >;
public:
    typedef typename base_type::value_type value_type;
    typedef typename base_type::size_type size_type;
    strong_broker_array() = delete;
    strong_broker_array( duplex_type& p );
    strong_broker_array( const strong_broker_array& ) = delete;
    strong_broker_array& operator=( const strong_broker_array& ) = delete;
    size_type size() const { return base_type::size(); }
    ~strong_broker_array();
private:
    Integral poll( size_type idx )
      { return base_type::operator[]( idx ).load(); }
    Integral drain( size_type idx )
      { return base_type::operator[]( idx ).exchange( 0 ); }
    duplex_type& prime_;
};

template< typename Integral >
strong_duplex_array< Integral >::~strong_duplex_array()
{
    std::lock_guard< std::mutex > _( serializer_ );
    assert( children_.size() == 0 );
}

template< typename Integral >
strong_broker_array< Integral >::strong_broker_array( duplex_type& p )
:
    base_type( p.size() ),
    prime_( p )
{
    prime_.insert( this );
}

template< typename Integral >
strong_broker_array< Integral >::~strong_broker_array()
{
    prime_.erase( this, base_type::load() );
}


template< typename Integral > class weak_broker_array;

template< typename Integral > class weak_duplex_array
: public bumper_array< Integral, atomicity::full >
{
    typedef bumper_array< Integral, atomicity::full > base_type;
    typedef weak_broker_array< Integral > broker_type;
    friend class weak_broker_array< Integral >;
public:
    typedef typename base_type::value_type value_type;
    typedef typename base_type::size_type size_type;
    weak_duplex_array() = delete;
    constexpr weak_duplex_array( size_type size )
      : base_type( size ) {}
    weak_duplex_array( const weak_duplex_array& ) = delete;
    weak_duplex_array& operator=( const weak_duplex_array& ) = delete;
    value_type& operator[]( size_type idx ) { return base_type::operator[]( idx ); }
    size_type size() const { return base_type::size(); }
    ~weak_duplex_array();
private:
    void insert( broker_type* child );
    void erase( broker_type* child, Integral by );
    mutable std::mutex serializer_;
    typedef std::unordered_set< broker_type* > set_type;
    set_type children_;
};

template< typename Integral > class weak_broker_array
: public bumper_array< Integral, atomicity::semi >
{
    typedef bumper_array< Integral, atomicity::semi > base_type;
    typedef weak_duplex_array< Integral > duplex_type;
    friend class weak_duplex_array< Integral >;
public:
    typedef typename base_type::value_type value_type;
    typedef typename base_type::size_type size_type;
    weak_broker_array() = delete;
    weak_broker_array( duplex_type& p );
    weak_broker_array( const weak_broker_array& ) = delete;
    weak_broker_array& operator=( const weak_broker_array& ) = delete;
    size_type size() const { return base_type::size(); }
    ~weak_broker_array();
private:
    Integral poll( size_type idx )
      { return base_type::operator[]( idx ).load(); }
    duplex_type& prime_;
};

template< typename Integral >
weak_duplex_array< Integral >::~weak_duplex_array()
{
    std::lock_guard< std::mutex > _( serializer_ );
    assert( children_.size() == 0 );
}

template< typename Integral >
weak_broker_array< Integral >::weak_broker_array( duplex_type& p )
:
    base_type( p.size() ),
    prime_( p )
{
    prime_.insert( this );
}

template< typename Integral >
weak_broker_array< Integral >::~weak_broker_array()
{
    prime_.erase( this, base_type::load() );
}



} // namespace counter

} // namespace gcl
