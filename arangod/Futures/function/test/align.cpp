#include "cxx_function.hpp"
#include <cassert>

bool did_move = false;

struct alignas(32) x {
    void operator () () {}
    x() = default;
    x( x const & ) = default;
    x( x && ) noexcept { did_move = true; }
};

int main() {
    cxx_function::function< void() > q = { cxx_function::in_place_t< x >{} }, r = std::move( q );
    static_assert ( alignof (x) > alignof (decltype (q)), "test defeated" );
    static_assert ( alignof (decltype(q)) == alignof (std::aligned_storage< sizeof(void *[2]) >::type), "Unexpected alignment." );
    assert ( ! did_move );
}

