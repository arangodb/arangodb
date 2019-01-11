
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[meyer97_stack4_main
#include "stack4.hpp"
#include <cassert>

int main() {
    stack4<int> s(3);
    assert(s.capacity() == 3);
    assert(s.count() == 0);
    assert(s.empty());
    assert(!s.full());

    s.put(123);
    assert(!s.empty());
    assert(!s.full());
    assert(s.item() == 123);

    s.remove();
    assert(s.empty());
    assert(!s.full());

    return 0;
}
//]

