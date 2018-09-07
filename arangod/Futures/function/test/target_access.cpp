#include "cxx_function.hpp"
#include <cassert>

using namespace cxx_function;

int * alias;

struct vf {
    virtual void operator() ( int ) {}
    int m = 5;

    vf() = default;
    vf( vf const & ) noexcept { alias = & m; }
};


int main() {
    function< void( int ) > f = vf();
    * alias = 3;
    assert ( f.target_type() == typeid (vf) );
    assert ( f.target< vf >()->m == 3 );
    assert ( function< void() >().target_type() == typeid (void) );
    assert ( function< void() >().target< void >() == nullptr );
}

