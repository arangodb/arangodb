#include <map>

#include "cxx_function.hpp"
#include <cassert>

std::map< int, std::ptrdiff_t > pool_total;
std::ptrdiff_t global_total = 0;


struct accounting {
    int id;
    static int next_id;
    
    accounting()
        : id( next_id ++ ) {}
    
    std::ptrdiff_t total() const { return pool_total[ id ]; }
};
int accounting::next_id = 0;

bool operator == ( accounting const & lhs, accounting const & rhs )
    { return lhs.id == rhs.id; }
bool operator != ( accounting const & lhs, accounting const & rhs )
    { return lhs.id != rhs.id; }


template< typename t >
struct accountant
    : accounting {
    typedef t value_type;
    
    t * allocate( std::size_t n ) {
        n *= sizeof (t);
        pool_total[ id ] += n;
        global_total += n;
        return static_cast< t * >( ::operator new( n ) );
    }
    void deallocate( t * p, std::size_t n ) {
        n *= sizeof (t);
        pool_total[ id ] -= n;
        global_total -= n;
        assert ( pool_total[ id ] >= 0 );
        assert ( global_total >= 0 );
        ::operator delete( p );
    }
    
    accountant select_on_container_copy_construction() const
        { return {}; }
    
    accountant() = default;
    
    template< typename u >
    accountant( accountant< u > const & o )
        : accounting( o ) {}
};

template< std::size_t n >
struct immovable {
    char weight[ n ];
    immovable() = default;
    immovable( immovable const & ) = default;
    immovable( immovable && ) {}
};

int main() {
    cxx_function::function_container< accountant<void>, accounting() > q;
    immovable< 5 > c5;
    auto five = [c5] { (void) c5; return accounting{}; };
    static_assert ( sizeof five == 5, "" );
    q = five;
    assert ( q.get_allocator().total() == 5 );
    auto r = q;
    assert ( q.get_allocator().total() == 5 );
    assert ( global_total == 10 );
    q = [q] { return q.get_allocator(); }; // Copy, then overwrite q.
    assert ( q.get_allocator().total() == sizeof (q) );
    assert ( global_total == 10 + sizeof (q) );
    cxx_function::function< accounting() > f;
    f = q; // Use q's allocator for a new wrapper targeting a container with a five.
    assert ( q.get_allocator().total() == 2 * sizeof (q) );
    assert ( global_total == 15 + 2 * sizeof (q) );
    
    f.assign( five, q.get_allocator() );
    assert ( q.get_allocator().total() == 5 + sizeof (q) );
    
    q = f;
    r = std::move( q );
    cxx_function::unique_function_container< accountant<void>, accounting() > s = r;
    s = std::move( r );
    #if __GNUC__ && defined(__has_warning)
    #   if __has_warning("-Wself-move")
    #       pragma GCC diagnostic ignored "-Wself-move"
    #   endif
    #endif
    s = std::move( s );
    assert ( s != nullptr );
}
