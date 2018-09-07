#include "cxx_function.hpp"
#include <cassert>

using namespace cxx_function;

struct vf {
    virtual int fn() { return m; }
    int m = 5;
};


int main() {
    function< int( vf ) const volatile > f = & vf::m, g = & vf::fn;
    vf o;

    assert ( f( o ) == 5 );
    o.m = 42;
    assert ( g( o ) == 42 );

    int (vf::* ptmf)() = & vf::fn;
    g = ptmf;
    assert ( g( o ) == 42 );
    ptmf = nullptr;
    g = ptmf;
    assert ( ! g );

    int vf::* ptm = & vf::m;
    assert ( f( o ) == 42 );
    ptm = nullptr;
    f = ptm;
    assert ( ! f );
}

