
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include "no_lambdas.hpp"
#include <boost/bind.hpp>
#include <cassert>

//[no_lambdas_cpp
iarray::iarray(unsigned max, unsigned count) :
    boost::contract::constructor_precondition<iarray>(boost::bind(
            &iarray::constructor_precondition, max, count)),
    values_(new int[max]), // Member initializations can be here.
    capacity_(max)
{
    boost::contract::old_ptr<int> old_instances;
    boost::contract::check c = boost::contract::constructor(this)
        .old(boost::bind(&iarray::constructor_old, boost::ref(old_instances)))
        .postcondition(boost::bind(
            &iarray::constructor_postcondition,
            this,
            boost::cref(max),
            boost::cref(count),
            boost::cref(old_instances)
        ))
    ;

    for(unsigned i = 0; i < count; ++i) values_[i] = int();
    size_ = count;
    ++instances_;
}

iarray::~iarray() {
    boost::contract::old_ptr<int> old_instances;
    boost::contract::check c = boost::contract::destructor(this)
        .old(boost::bind(&iarray::destructor_old, this,
                boost::ref(old_instances)))
        .postcondition(boost::bind(&iarray::destructor_postcondition,
                boost::cref(old_instances)))
    ;
    
    delete[] values_;
    --instances_;
}

void iarray::push_back(int value, boost::contract::virtual_* v) {
    boost::contract::old_ptr<unsigned> old_size;
    boost::contract::check c = boost::contract::public_function(v, this)
        .precondition(boost::bind(&iarray::push_back_precondition, this))
        .old(boost::bind(&iarray::push_back_old, this, boost::cref(v),
                boost::ref(old_size)))
        .postcondition(boost::bind(&iarray::push_back_postcondition, this,
                boost::cref(old_size)))
    ;

    values_[size_++] = value;
}

unsigned iarray::capacity() const {
    // Check invariants.
    boost::contract::check c = boost::contract::public_function(this);
    return capacity_;
}

unsigned iarray::size() const {
    // Check invariants.
    boost::contract::check c = boost::contract::public_function(this);
    return size_;
}

int iarray::instances() {
    // Check static invariants.
    boost::contract::check c = boost::contract::public_function<iarray>();
    return instances_;
}

int iarray::instances_ = 0;
//]

int main() {
    iarray a(3, 2);
    assert(a.capacity() == 3);
    assert(a.size() == 2);
    
    a.push_back(-123);
    assert(a.size() == 3);

    return 0;
}

