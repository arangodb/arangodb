
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <cassert>

//[friend_invariant
template<typename T>
class positive {
public:
    void invariant() const {
        BOOST_CONTRACT_ASSERT(value() > 0);
    }

    // Can be considered an extension of enclosing class' public interface...
    friend void swap(positive& object, T& value) {
        boost::contract::old_ptr<T> old_object_value =
                BOOST_CONTRACT_OLDOF(object.value());
        boost::contract::old_ptr<T> old_value = BOOST_CONTRACT_OLDOF(value);
        // ...so it can be made to check invariants via `public_function`.
        boost::contract::check c = boost::contract::public_function(&object)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(value > 0);
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(object.value() == *old_value);
                BOOST_CONTRACT_ASSERT(value == *old_object_value);
            })
        ;

        T saved = object.value_;
        object.value_ = value;
        value = saved;
    }
    
private:
    T value_;

    /* ... */
//]

public:
    // Could program contracts for following too.
    explicit positive(T const& value) : value_(value) {}
    T value() const { return value_; }
};

int main() {
    positive<int> i(123);
    int x = 456;
    swap(i, x);
    assert(i.value() == 456);
    assert(x == 123);
    return 0;
}

