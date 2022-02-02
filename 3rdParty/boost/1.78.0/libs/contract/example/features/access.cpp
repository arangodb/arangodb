
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <vector>
#include <cassert>

template<typename T>
class pushable {
    friend class boost::contract::access;

    void invariant() const {
        BOOST_CONTRACT_ASSERT(capacity() <= max_size());
    }

public:
    virtual void push_back(T const& value, boost::contract::virtual_* v = 0)
            = 0;

protected:
    virtual unsigned capacity() const = 0;
    virtual unsigned max_size() const = 0;
};

template<typename T>
void pushable<T>::push_back(T const& value, boost::contract::virtual_* v) {
    boost::contract::old_ptr<unsigned> old_capacity =
            BOOST_CONTRACT_OLDOF(v, capacity());
    boost::contract::check c = boost::contract::public_function(v, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(capacity() < max_size());
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(capacity() >= *old_capacity);
        })
    ;
    assert(false);
}

//[access
template<typename T>
class vector
    #define BASES public pushable<T>
    : BASES
{ // Private section of the class.
    friend class boost::contract::access; // Friend `access` class so...

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types; // ...private bases.
    #undef BASES
    
    void invariant() const { // ...private invariants.
        BOOST_CONTRACT_ASSERT(size() <= capacity());
    }
    
    BOOST_CONTRACT_OVERRIDE(push_back) // ...private overrides.
    
public: // Public section of the class.
    void push_back(T const& value, boost::contract::virtual_* v = 0)
            /* override */ {
        boost::contract::old_ptr<unsigned> old_size =
                BOOST_CONTRACT_OLDOF(v, size());
        boost::contract::check c = boost::contract::public_function<
                override_push_back>(v, &vector::push_back, this, value)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(size() < max_size());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(size() == *old_size + 1);
            })
        ;

        vect_.push_back(value);
    }

    /* ... */
//]

    unsigned size() const { return vect_.size(); }
    unsigned max_size() const { return vect_.max_size(); }
    unsigned capacity() const { return vect_.capacity(); }

private: // Another private section.
    std::vector<T> vect_;
};

int main() {
    vector<int> vect;
    vect.push_back(123);
    assert(vect.size() == 1);
    return 0;
}

