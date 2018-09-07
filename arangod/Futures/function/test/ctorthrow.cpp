#include "cxx_function.hpp"

struct ctor_error : std::exception {};

struct throwy {
    mutable int when;
    
    explicit throwy( int in_when )
        : when( in_when )
        { if ( when == 1 ) throw ctor_error{}; }
    
    throwy( throwy && o )
        : when( o.when ) {
        o.when = 0;
        if ( when == 2 ) throw ctor_error{};
    }
    throwy( throwy const & o )
        : when( o.when ) {
        o.when = 0;
        if ( when == 3 ) throw ctor_error{};
    }
    ~ throwy() {
        if ( when != 0 ) abort();
    }
    
    void operator () () {}
};

template< typename t >
struct alloc {
    typedef t value_type;
    int * count;
    
    explicit alloc( int * in_count )
        : count( in_count ) {}
    template< typename u >
    alloc( alloc< u > const & o )
        : count( o.count ) {}
    
    t * allocate( std::size_t n ) {
        ++ * count;
        return static_cast< t * >( ::operator new( n * sizeof (t) ) );
    }
    void deallocate( t * p, std::size_t ) {
        -- * count;
        ::operator delete( p );
    }
};

template< typename t, typename u >
bool operator == ( alloc< t > const & lhs, alloc< u > const & rhs )
    { return lhs.count == rhs.count; }
template< typename t, typename u >
bool operator != ( alloc< t > const & lhs, alloc< u > const & rhs )
    { return lhs.count != rhs.count; }

namespace std {
    template<>
    struct uses_allocator< throwy, alloc< throwy > > : true_type {};
}

struct fine {
    void operator () () {}
};

using namespace cxx_function;

int main() {
    function< void() > f = fine();
    
    try {
        f.emplace_assign< throwy >( 1 );
        abort();
    } catch ( ctor_error & ) {}
    
    assert ( f.target< fine >() );
    
    int count_f = 0, count_g = 0;
    
    f.allocate_assign< throwy >( alloc< throwy >{ & count_f }, 2 );
    
    try {
        function< void() > g( std::allocator_arg, alloc< throwy >{ & count_g }, std::move( f ) );
        abort();
    } catch ( ctor_error & ) {}
    assert ( count_f == 1 );
    assert ( count_g == 0 );
    
    f();
    f.allocate_assign< throwy >( alloc< throwy >{ & count_f }, 3 );
    
    try {
        function< void() > g = f;
        abort();
    } catch ( ctor_error & ) {}
    assert ( count_f == 1 );
    
    f.target< throwy >()->when = 3;
    try {
        function< void() > g;
        g = f;
        abort();
    } catch ( ctor_error & ) {}
    assert ( count_f == 1 );
    
    f();
    auto g = f;
    assert ( count_f == 2 );
}
