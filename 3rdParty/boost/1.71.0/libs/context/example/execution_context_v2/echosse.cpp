
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <emmintrin.h>

#include <boost/context/execution_context.hpp>

namespace ctx = boost::context;

void echoSSE( int i) {
    __m128i xmm;
    xmm = _mm_set_epi32( i, i + 1, i + 2, i + 3);
    uint32_t v32[4];
    memcpy( & v32, & xmm, 16);
    std::cout << v32[0]; 
    std::cout << v32[1]; 
    std::cout << v32[2]; 
    std::cout << v32[3]; 
}

ctx::execution_context< int > echo( ctx::execution_context< int > && ctx, int i) {
    for (;;) {
        std::cout << i;
        echoSSE( i);
        std::cout << " ";
        std::tie( ctx, i) = ctx( 0);
    }
    return std::move( ctx);
}

int main( int argc, char * argv[]) {
    ctx::execution_context< int > ctx( echo);
    for ( int i = 0; i < 10; ++i) {
        ctx = std::get< 0 >( ctx( i) );
    }
    std::cout << "\nDone" << std::endl;
    return EXIT_SUCCESS;
}
