
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[cline90_vector
#ifndef VECTOR_HPP_
#define VECTOR_HPP_

#include <boost/contract.hpp>

// NOTE: Incomplete contract assertions, addressing only `size`.
template<typename T>
class vector
    #define BASES private boost::contract::constructor_precondition<vector<T> >
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    void invariant() const {
        BOOST_CONTRACT_ASSERT(size() >= 0);
    }

public:
    explicit vector(int count = 10) :
        boost::contract::constructor_precondition<vector>([&] {
            BOOST_CONTRACT_ASSERT(count >= 0);
        }),
        data_(new T[count]),
        size_(count)
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(size() == count);
            })
        ;

        for(int i = 0; i < size_; ++i) data_[i] = T();
    }

    virtual ~vector() {
        boost::contract::check c = boost::contract::destructor(this);
        delete[] data_;
    }

    int size() const {
        boost::contract::check c = boost::contract::public_function(this);
        return size_; // Non-negative result already checked by invariant.
    }

    void resize(int count) {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(count >= 0);
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(size() == count);
            })
        ;

        T* slice = new T[count];
        for(int i = 0; i < count && i < size_; ++i) slice[i] = data_[i];
        delete[] data_;
        data_ = slice;
        size_ = count;
    }

    T& operator[](int index) {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(index >= 0);
                BOOST_CONTRACT_ASSERT(index < size());
            })
        ;

        return data_[index];
    }
    
    T const& operator[](int index) const {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(index >= 0);
                BOOST_CONTRACT_ASSERT(index < size());
            })
        ;

        return data_[index];
    }

private:
    T* data_;
    int size_;
};

#endif // #include guard
//]

