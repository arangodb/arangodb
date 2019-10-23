
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include "separate_body.hpp"
#include <cassert>

//[separate_body_cpp
void iarray::constructor_body(unsigned max, unsigned count) {
    for(unsigned i = 0; i < count; ++i) values_[i] = int();
    size_ = count;
}

void iarray::destructor_body() { delete[] values_; }

void iarray::push_back_body(int value) { values_[size_++] = value; }

/* ... */
//]

unsigned iarray::capacity_body() const { return capacity_; }
unsigned iarray::size_body() const { return size_; }

int main() {
    iarray a(3, 2);
    assert(a.capacity() == 3);
    assert(a.size() == 2);

    a.push_back(-123);
    assert(a.size() == 3);
    
    return 0;
}

