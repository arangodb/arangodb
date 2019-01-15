
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <vector>
#include <cassert>

//[pure_virtual_public_base_begin
template<typename Iterator>
class range {
public:
    // Pure virtual function declaration (contract in definition below).
    virtual Iterator begin(boost::contract::virtual_* v = 0) = 0;
//]

    // Could program class invariants and contracts for the following too.
    virtual Iterator end() = 0;
    virtual bool empty() const = 0;

//[pure_virtual_public_base_end
    /* ... */
};
//]

//[pure_virtual_public_base_impl
// Pure virtual function default implementation (out-of-line in C++).
template<typename Iterator>
Iterator range<Iterator>::begin(boost::contract::virtual_* v) {
    Iterator result; // As usual, virtual pass `result` right after `v`...
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .postcondition([&] (Iterator const& result) {
            if(empty()) BOOST_CONTRACT_ASSERT(result == end());
        })
    ;

    // Pure function body (never executed by this library).
    assert(false);
    return result;
}
//]

template<typename T>
class vector
    #define BASES public range<typename std::vector<T>::iterator>
    : BASES
{
public:
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES
    typedef typename std::vector<T>::iterator iterator;

    iterator begin(boost::contract::virtual_* v = 0) /* override */ {
        iterator result;
        // Again, pass result right after `v`...
        boost::contract::check c = boost::contract::public_function<
                override_begin>(v, result, &vector::begin, this)
            // ...plus postconditions take `result` as parameter (not capture).
            .postcondition([&] (iterator const& result) {
                if(!empty()) BOOST_CONTRACT_ASSERT(*result == front());
            })
        ;

        return result = vect_.begin();
    }
    BOOST_CONTRACT_OVERRIDE(begin)

    // Could program class invariants and contracts for the following too.
    iterator end() { return vect_.end(); }
    bool empty() const { return vect_.empty(); }
    T const& front() const { return vect_.front(); }
    void push_back(T const& value) { vect_.push_back(value); }

private:
    std::vector<T> vect_;
};

int main() {
    vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    range<std::vector<int>::iterator>& r = v;
    assert(*(r.begin()) == 1);
    return 0;
}

