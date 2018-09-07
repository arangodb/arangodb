#include "cxx_function.hpp"

template< typename t >
struct bad {
    bad() = default;
    bad( bad && ) = default;
    bad( bad const & )
        { t::nonsense(); }
    
    void operator () () {}
};

int main() {
    cxx_function::unique_function< void() > q = bad< void >{};
}
