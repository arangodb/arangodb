
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[cline90_stack
#include <boost/contract.hpp>
#include <cassert>

// NOTE: Incomplete contract assertions, addressing only `empty` and `full`.
template<typename T>
class stack
    #define BASES private boost::contract::constructor_precondition<stack<T> >
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

public:
    explicit stack(int capacity) :
        boost::contract::constructor_precondition<stack>([&] {
            BOOST_CONTRACT_ASSERT(capacity >= 0);
        }),
        data_(new T[capacity]), capacity_(capacity), size_(0)
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(empty());
                BOOST_CONTRACT_ASSERT(full() == (capacity == 0));
            })
        ;
        
        for(int i = 0; i < capacity_; ++i) data_[i] = T();
    }

    virtual ~stack() {
        boost::contract::check c = boost::contract::destructor(this);
        delete[] data_;
    }

    bool empty() const {
        boost::contract::check c = boost::contract::public_function(this);
        return size_ == 0;
    }

    bool full() const {
        boost::contract::check c = boost::contract::public_function(this);
        return size_ == capacity_;
    }

    void push(T const& value) {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!full());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!empty());
            })
        ;

        data_[size_++] = value;
    }

    T pop() {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!empty());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!full());
            })
        ;

        return data_[--size_];
    }

private:
    T* data_;
    int capacity_;
    int size_;
};

int main() {
    stack<int> s(3);
    s.push(123);
    assert(s.pop() == 123);
    return 0;
}
//]

