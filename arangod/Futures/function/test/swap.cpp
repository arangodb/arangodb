#include "cxx_function.hpp"
#include <cassert>

using namespace cxx_function;

struct vf {
    int operator () () { return m; }
    int m;
};


int main() {
    function< int() > f = vf{ 3 }, g = vf{ 10 };
    std::swap( f, g );

    assert ( f() == 10 );
    assert ( g() == 3 );
}

