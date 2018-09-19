
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cassert>

//[assertion_level_no_impl
// If valid iterator range (cannot implement in C++ but OK to use in AXIOM).
template<typename Iter>
bool valid(Iter first, Iter last); // Only declared, not actually defined.
//]

//[assertion_level_class_begin
template<typename T>
class vector {
//]

public:
    typedef typename std::vector<T>::iterator iterator;

    // Could program class invariants and contracts for the following.
    iterator begin() { return vect_.begin(); }
    iterator end() { return vect_.end(); }
    unsigned capacity() const { return vect_.capacity(); }
    bool operator==(vector const& other) { return vect_ == other.vect_; }

//[assertion_level_axiom
public:
    iterator insert(iterator where, T const& value) {
        iterator result;
        boost::contract::old_ptr<unsigned> old_capacity =
                BOOST_CONTRACT_OLDOF(capacity());
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(capacity() >= *old_capacity);
                if(capacity() > *old_capacity) {
                    BOOST_CONTRACT_ASSERT_AXIOM(!valid(begin(), end()));
                } else {
                    BOOST_CONTRACT_ASSERT_AXIOM(!valid(where, end()));
                }
            })
        ;

        return result = vect_.insert(where, value);
    }
//]

//[assertion_level_audit_old
public:
    void swap(vector& other) {
        boost::contract::old_ptr<vector> old_me, old_other;
        #ifdef BOOST_CONTRACT_AUDITS
            old_me = BOOST_CONTRACT_OLDOF(*this);
            old_other = BOOST_CONTRACT_OLDOF(other);
        #endif // Else, skip old value copies...
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // ...and also skip related assertions.
                BOOST_CONTRACT_ASSERT_AUDIT(*this == *old_other);
                BOOST_CONTRACT_ASSERT_AUDIT(other == *old_me);
            })
        ;

        vect_.swap(other.vect_);
    }
//]

//[assertion_level_class_end
    /* ... */

private:
    std::vector<T> vect_;
};
//]

//[assertion_level_audit
template<typename RandomIter, typename T>
RandomIter random_binary_search(RandomIter first, RandomIter last,
        T const& value) {
    RandomIter result;
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(first <= last); // Default, not expensive.
            // Expensive O(n) assertion (use AXIOM if prohibitive instead).
            BOOST_CONTRACT_ASSERT_AUDIT(std::is_sorted(first, last));
        })
        .postcondition([&] {
            if(result != last) BOOST_CONTRACT_ASSERT(*result == value);
        })
    ;

    /* ... */
//]

    RandomIter begin = first, end = last;
    while(begin < end) {
        RandomIter middle = begin + ((end - begin) >> 1);
        BOOST_CONTRACT_CHECK(*begin <= *middle || value < *middle ||
                *middle < value);
        
        if(value < *middle) end = middle;
        else if(value > *middle) begin = middle + 1;
        else return result = middle;
    }
    return result = last;
}

int main() {
    vector<char> v;
    v.insert(v.begin() + 0, 'a');
    v.insert(v.begin() + 1, 'b');
    v.insert(v.begin() + 2, 'c');
    
    vector<char>::iterator i = random_binary_search(v.begin(), v.end(), 'b');
    assert(i != v.end());
    assert(*i == 'b');
    
    vector<char> w;
    w.insert(w.begin() + 0, 'x');
    w.insert(w.begin() + 1, 'y');

    w.swap(v);
    assert(*(w.begin() + 0) == 'a');
    assert(*(w.begin() + 1) == 'b');
    assert(*(w.begin() + 2) == 'c');
    
    return 0;
}

