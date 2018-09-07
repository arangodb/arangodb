#include "cxx_function.hpp"
#include <cassert>

using namespace cxx_function;

int c[ 2 ] = {};

struct mc {
    int x;
    mc operator() ( mc ) { return {1}; }
    mc( int i ) : x{ i } {}
    mc( mc && o ) noexcept : x(o.x) { ++ c[ x ]; }
    mc( mc const & o ) = default;
};

int main() {
    mc m {0};
    function< mc( mc ) > q = m; // Standard mandates a move here because constructor passes by value.
    assert ( c[0] == 0 ); // Better not to.
    q( m );
    assert ( c[0] == 1 );
    q = m; // Copy-and-swap adds an unnecessary move.
    assert ( c[0] == 2 );
    int reliable_copy_elision =
#ifdef _MSC_VER
        0;
#else
        1;
#endif
    assert ( c[1] == 1 - reliable_copy_elision ); // Copy elision of return prvalues.
}
