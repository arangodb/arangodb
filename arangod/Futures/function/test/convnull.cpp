#include "cxx_function.hpp"
#include <cassert>
using namespace cxx_function;

struct c {
    void operator() ( int ) {}
    operator function< void() > () const { return {}; }
};

struct d : private function< void() > {
    void operator() ( int ) {}
};

struct d2 : public function< void() > {
    void operator() ( int ) {}
};

int main() {
    function< void( int ) > a;
    function< void( long ) > b = a;
    assert ( ! b );
    b = c{};
    assert ( b );
    b = d{};
    assert ( b );
    b = d2{};
    assert ( b );
}

