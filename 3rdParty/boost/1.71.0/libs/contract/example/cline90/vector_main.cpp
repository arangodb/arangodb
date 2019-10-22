
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[cline90_vector_main
#include "vector.hpp"
#include <cassert>

int main() {
    vector<int> v (3);
    assert(v.size() == 3);
    
    v[0] = 123;
    v.resize(2);
    assert(v[0] == 123);
    assert(v.size() == 2);
    
    return 0;
}
//]

