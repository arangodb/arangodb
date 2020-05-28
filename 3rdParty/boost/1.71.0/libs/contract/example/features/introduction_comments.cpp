
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <cassert>

//[introduction_comments
void inc(int& x)
    // Precondition:    x < std::numeric_limit<int>::max()
    // Postcondition:   x == oldof(x) + 1
{
    ++x; // Function body.
}
//]

int main() {
    int x = 10;
    inc(x);
    assert(x == 11);
    return 0;
}

