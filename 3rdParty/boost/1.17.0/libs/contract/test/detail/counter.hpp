
#ifndef BOOST_CONTRACT_TEST_DETAIL_COUNTER_HPP_
#define BOOST_CONTRACT_TEST_DETAIL_COUNTER_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

namespace boost { namespace contract { namespace test { namespace detail {

// Helper to count copies and evaluations of type (e.g., for old values).
template<class Tag, typename T>
struct counter {
    T value;

    counter() : value() { ++ctors_; }
    static unsigned ctors() { return ctors_; }

    ~counter() { ++dtors_; }
    static unsigned dtors() { return dtors_; }

    /* implicit */ counter(counter const& other) : value(other.value) {
        ++ctor_copies_;
        ++ctors_;
    }

    counter& operator=(counter const& other) {
        value = other.value;
        ++op_copies_;
        return *this;
    }
    
    static unsigned copies() { return ctor_copies_ + op_copies_; }

    static counter const& eval(counter const& me) { ++me.evals_; return me; }
    static unsigned evals() { return evals_; }

private:
    static unsigned ctors_; // Total constructions (including copies).
    static unsigned dtors_;
    static unsigned ctor_copies_;
    static unsigned op_copies_;
    static unsigned evals_;
};

template<class Tag, typename T> unsigned counter<Tag, T>::ctors_ = 0;
template<class Tag, typename T> unsigned counter<Tag, T>::dtors_ = 0;
template<class Tag, typename T> unsigned counter<Tag, T>::ctor_copies_ = 0;
template<class Tag, typename T> unsigned counter<Tag, T>::op_copies_ = 0;
template<class Tag, typename T> unsigned counter<Tag, T>::evals_ = 0;
    
} } } } // namespace

#endif // #include guard

