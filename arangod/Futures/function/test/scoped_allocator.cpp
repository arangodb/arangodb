#include "cxx_function.hpp"

#include <cassert>
#include <forward_list>
#include <map>
#include <scoped_allocator>
#include <string>

std::map< int, std::size_t > pool;

template< typename t >
struct pool_alloc : std::allocator< t > {
    int id;

    template< typename u >
    pool_alloc( pool_alloc< u > const & o )
        : id( o.id ) {}

    pool_alloc( int in_id
#if _MSC_VER && _MSC_VER < 1910
                            = 0
#endif
                                )
        : id( in_id ) {}

    t * allocate( std::size_t n ) {
        n *= sizeof (t);
        pool[ id ] += n;
        return static_cast< t * >( ::operator new( n ) );
    }

    void deallocate( t * p, std::size_t n ) {
        n *= sizeof (t);
        pool[ id ] -= n;
        return ::operator delete( p );
    }

    template< typename o > struct rebind { typedef pool_alloc< o > other; };
    
    typedef std::false_type is_always_equal;
    typedef std::false_type propagate_on_container_move_assignment;
};

template< typename t, typename u >
bool operator == ( pool_alloc< t > lhs, pool_alloc< u > rhs )
    { return lhs.id == rhs.id; }
template< typename t, typename u >
bool operator != ( pool_alloc< t > lhs, pool_alloc< u > rhs )
    { return lhs.id != rhs.id; }

typedef std::basic_string< char, std::char_traits< char >, pool_alloc< char > > pool_string;

struct stateful_op {
    pool_string state;

    stateful_op( stateful_op const & o, pool_alloc< stateful_op > a )
        : state( o.state, a ) {}

    explicit stateful_op( pool_string s )
        : state( std::move( s ) ) {}

    void operator () () const
        { /* std::cout << "op says " << state << " from pool id " << state.get_allocator().id << '\n'; */ }
};

struct listful_op {
    typedef std::scoped_allocator_adaptor< pool_alloc< pool_string > > state_alloc;
    std::forward_list< pool_string, state_alloc > state;

    listful_op( listful_op const & o, state_alloc a )
        : state( o.state, a ) {}

    explicit listful_op( pool_string s, state_alloc a )
        : state( { std::move( s ) }, a ) {}

    void operator () () const
        { /* std::cout << "op says " << state << " from pool id " << state.get_allocator().id << '\n'; */ }
};
static_assert ( sizeof (std::forward_list< int >) < sizeof(void *[3]), "list is bigger than anticipated." );
static_assert ( sizeof (listful_op) <= sizeof (void *[3]), "Small-size container test defeated." );

namespace std {
    template< typename a > struct uses_allocator< stateful_op, a > : std::true_type {};
    template< typename a > struct uses_allocator< listful_op, a > : std::true_type {};
}

using namespace cxx_function;

int main() {
    stateful_op op( { "hello from a very long string", pool_alloc< char >{ 0 } } );
    
    typedef function_container< std::scoped_allocator_adaptor< pool_alloc<char> >, void() > fct;
    fct fc1( pool_alloc< char >{ 1 } );
    fc1 = op;
    function< void() > fv = fc1;
    fct fc2( pool_alloc< char >{ 2 } );
    fc2 = fv;
    
    op();
    fc1();
    fc2();
    fv();
    
    assert ( pool[ 0 ] == op.state.capacity() + 1 );
    assert ( pool[ 1 ] == op.state.capacity() * 2 + 2 + sizeof (stateful_op) * 2 );
    assert ( pool[ 2 ] == op.state.capacity() + 1 + sizeof (stateful_op) );
    {
        std::map< int, function< void() >, std::less< int >,
            typename fct::allocator_type::template rebind< std::pair< int const, function< void() > > >::other > m( pool_alloc< char >{ 3 } );
        
        #if __clang__ || __GNUC__ >= 5
        m[ 42 ].assign( fv, m.get_allocator() );
        #else
        m.insert( std::make_pair( 42, fv ) );
        #endif
        std::size_t pre = pool[ 3 ];
        pool_string{ pool_alloc< char >{ 3 } }.swap( m[ 42 ].target< stateful_op >()->state );
        assert ( pre - pool[ 3 ] == op.state.capacity() + 1 );
    }
    assert ( fc2.target< stateful_op >() );
    fc2 = listful_op( fc2.target< stateful_op >()->state, pool_alloc< char >{ 2 } );
    fc1 = fc2;
#if ! _MSC_VER || _MSC_VER >= 1910
    fv = nullptr;
#else
    fv.operator = < std::nullptr_t >( nullptr );
#endif
    
    assert ( pool[ 1 ] == pool[ 2 ] );
}
