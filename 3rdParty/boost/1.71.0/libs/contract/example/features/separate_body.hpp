
#ifndef SEPARATE_BODY_HPP_
#define SEPARATE_BODY_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>

//[separate_body_hpp
class iarray :
        private boost::contract::constructor_precondition<iarray> {
public:
    void invariant() const {
        BOOST_CONTRACT_ASSERT(size() <= capacity());
    }

    explicit iarray(unsigned max, unsigned count = 0) :
        boost::contract::constructor_precondition<iarray>([&] {
            BOOST_CONTRACT_ASSERT(count <= max);
        }),
        // Still, member initializations must be here.
        values_(new int[max]),
        capacity_(max)
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(capacity() == max);
                BOOST_CONTRACT_ASSERT(size() == count);
            })
        ;
        constructor_body(max, count); // Separate constructor body impl.
    }

    virtual ~iarray() {
        boost::contract::check c = boost::contract::destructor(this); // Inv.
        destructor_body(); // Separate destructor body implementation.
    }

    virtual void push_back(int value, boost::contract::virtual_* v = 0) {
        boost::contract::old_ptr<unsigned> old_size =
                BOOST_CONTRACT_OLDOF(v, size());
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(size() < capacity());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(size() == *old_size + 1);
            })
        ;
        push_back_body(value); // Separate member function body implementation.
    }

private:
    // Contracts in class declaration (above), but body implementations are not.
    void constructor_body(unsigned max, unsigned count);
    void destructor_body();
    void push_back_body(int value);

    /* ... */
//]
    
public:
    unsigned capacity() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return capacity_body();
    }
    
    unsigned size() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return size_body();
    }
    
private:
    unsigned size_body() const;
    unsigned capacity_body() const;

    int* values_;
    unsigned capacity_;
    unsigned size_;
};

#endif // #include guard

