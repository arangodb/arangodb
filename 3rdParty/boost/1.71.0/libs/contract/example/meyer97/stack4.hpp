
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[meyer97_stack4
// File: stack4.hpp
#ifndef STACK4_HPP_
#define STACK4_HPP_

#include <boost/contract.hpp>

// Dispenser with LIFO access policy and fixed max capacity.
template<typename T>
class stack4
    #define BASES private boost::contract::constructor_precondition<stack4<T> >
    : BASES
{
    friend boost::contract::access;
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    void invariant() const {
        BOOST_CONTRACT_ASSERT(count() >= 0); // Count non-negative.
        BOOST_CONTRACT_ASSERT(count() <= capacity()); // Count bounded.
        BOOST_CONTRACT_ASSERT(empty() == (count() == 0)); // Empty if no elem.
    }

public:
    /* Initialization */

    // Allocate static from a maximum of n elements.
    explicit stack4(int n) :
        boost::contract::constructor_precondition<stack4>([&] {
            BOOST_CONTRACT_ASSERT(n >= 0); // Non-negative capacity.
        })
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(capacity() == n); // Capacity set.
            })
        ;

        capacity_ = n;
        count_ = 0;
        array_ = new T[n];
    }

    // Deep copy via constructor.
    /* implicit */ stack4(stack4 const& other) {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(capacity() == other.capacity());
                BOOST_CONTRACT_ASSERT(count() == other.count());
                BOOST_CONTRACT_ASSERT_AXIOM(*this == other);
            })
        ;

        capacity_ = other.capacity_;
        count_ = other.count_;
        array_ = new T[other.capacity_];
        for(int i = 0; i < other.count_; ++i) array_[i] = other.array_[i];
    }

    // Deep copy via assignment.
    stack4& operator=(stack4 const& other) {
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(capacity() == other.capacity());
                BOOST_CONTRACT_ASSERT(count() == other.count());
                BOOST_CONTRACT_ASSERT_AXIOM(*this == other);
            })
        ;

        delete[] array_;
        capacity_ = other.capacity_;
        count_ = other.count_;
        array_ = new T[other.capacity_];
        for(int i = 0; i < other.count_; ++i) array_[i] = other.array_[i];
        return *this;
    }

    // Destroy this stack.
    virtual ~stack4() {
        // Check invariants.
        boost::contract::check c = boost::contract::destructor(this);
        delete[] array_;
    }

    /* Access */

    // Max number of stack elements.
    int capacity() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return capacity_;
    }

    // Number of stack elements.
    int count() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return count_;
    }

    // Top element.
    T const& item() const {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!empty()); // Not empty (i.e., count > 0).
            })
        ;

        return array_[count_ - 1];
    }
    
    /* Status Report */

    // Is stack empty?
    bool empty() const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Empty definition.
                BOOST_CONTRACT_ASSERT(result == (count() == 0));
            })
        ;

        return result = (count_ == 0);
    }

    // Is stack full?
    bool full() const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT( // Full definition.
                        result == (count() == capacity()));
            })
        ;

        return result = (count_ == capacity_);
    }

    /* Element Change */

    // Add x on top.
    void put(T const& x) {
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!full()); // Not full.
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!empty()); // Not empty.
                BOOST_CONTRACT_ASSERT(item() == x); // Added to top.
                BOOST_CONTRACT_ASSERT(count() == *old_count + 1); // One more.
            })
        ;

        array_[count_++] = x;
    }

    // Remove top element.
    void remove() {
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!empty()); // Not empty (i.e., count > 0).
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!full()); // Not full.
                BOOST_CONTRACT_ASSERT(count() == *old_count - 1); // One less.
            })
        ;

        --count_;
    }

    /* Friend Helpers */

    friend bool operator==(stack4 const& left, stack4 const& right) {
        boost::contract::check inv1 = boost::contract::public_function(&left);
        boost::contract::check inv2 = boost::contract::public_function(&right);
        if(left.count_ != right.count_) return false;
        for(int i = 0; i < left.count_; ++i) {
            if(left.array_[i] != right.array_[i]) return false;
        }
        return true;
    }

private:
    int capacity_;
    int count_;
    T* array_; // Internally use C-style array.
};

#endif // #include guard
//]

