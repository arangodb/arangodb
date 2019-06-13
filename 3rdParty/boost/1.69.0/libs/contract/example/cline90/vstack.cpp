
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[cline90_vstack
#include "vector.hpp"
#include <boost/contract.hpp>
#include <boost/optional.hpp>
#include <cassert>

// NOTE: Incomplete contract assertions, addressing only `empty` and `full`.
template<typename T>
class abstract_stack {
public:
    abstract_stack() {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                // AXIOM as empty() cannot actually be checked here to avoid
                // calling pure virtual function length() during construction).
                BOOST_CONTRACT_ASSERT_AXIOM(empty());
            })
        ;
    }

    virtual ~abstract_stack() {
        boost::contract::check c = boost::contract::destructor(this);
    }

    bool full() const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(result == (length() == capacity()));
            })
        ;

        return result = (length() == capacity());
    }

    bool empty() const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(result = (length() == 0));
            })
        ;

        return result = (length() == 0);
    }

    virtual int length(boost::contract::virtual_* v = 0) const = 0;
    virtual int capacity(boost::contract::virtual_* v = 0) const = 0;
    
    virtual T const& item(boost::contract::virtual_* v = 0) const = 0;
    
    virtual void push(T const& value, boost::contract::virtual_* v = 0) = 0;
    virtual T const& pop(boost::contract::virtual_* v = 0) = 0;
    
    virtual void clear(boost::contract::virtual_* v = 0) = 0;
};

template<typename T>
int abstract_stack<T>::length(boost::contract::virtual_* v) const {
    int result;
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .postcondition([&] (int const& result) {
            BOOST_CONTRACT_ASSERT(result >= 0);
        })
    ;
    assert(false);
    return result;
}

template<typename T>
int abstract_stack<T>::capacity(boost::contract::virtual_* v) const {
    int result;
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .postcondition([&] (int const& result) {
            BOOST_CONTRACT_ASSERT(result >= 0);
        })
    ;
    assert(false);
    return result;
}

template<typename T>
T const& abstract_stack<T>::item(boost::contract::virtual_* v) const {
    boost::optional<T const&> result;
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(!empty());
        })
    ;
    assert(false);
    return *result;
}

template<typename T>
void abstract_stack<T>::push(T const& value, boost::contract::virtual_* v) {
    boost::contract::check c = boost::contract::public_function(v, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(!full());
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(!empty());
        })
    ;
    assert(false);
}

template<typename T>
T const& abstract_stack<T>::pop(boost::contract::virtual_* v) {
    boost::optional<T const&> result;
    boost::contract::old_ptr<T> old_item = BOOST_CONTRACT_OLDOF(v, item());
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(!empty());
        })
        .postcondition([&] (boost::optional<T const&> const& result) {
            BOOST_CONTRACT_ASSERT(!full());
            BOOST_CONTRACT_ASSERT(*result == *old_item);
        })
    ;
    assert(false);
    return *result;
}

template<typename T>
void abstract_stack<T>::clear(boost::contract::virtual_* v) {
    boost::contract::check c = boost::contract::public_function(v, this)
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(empty());
        })
    ;
    assert(false);
}

template<typename T>
class vstack
    #define BASES private boost::contract::constructor_precondition< \
            vstack<T> >, public abstract_stack<T>
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    void invariant() const {
        BOOST_CONTRACT_ASSERT(length() >= 0);
        BOOST_CONTRACT_ASSERT(length() < capacity());
    }
    
    BOOST_CONTRACT_OVERRIDES(length, capacity, item, push, pop, clear)

public:
    explicit vstack(int count = 10) :
        boost::contract::constructor_precondition<vstack>([&] {
            BOOST_CONTRACT_ASSERT(count >= 0);
        }),
        vect_(count), // OK, executed after precondition so count >= 0.
        len_(0)
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(length() == 0);
                BOOST_CONTRACT_ASSERT(capacity() == count);
            })
        ;
    }

    virtual ~vstack() {
        boost::contract::check c = boost::contract::destructor(this);
    }

    // Inherited from abstract_stack.

    virtual int length(boost::contract::virtual_* v = 0) const /* override */ {
        int result;
        boost::contract::check c = boost::contract::public_function<
                override_length>(v, result, &vstack::length, this);
        return result = len_;
    }

    virtual int capacity(boost::contract::virtual_* v = 0)
            const /* override */ {
        int result;
        boost::contract::check c = boost::contract::public_function<
                override_capacity>(v, result, &vstack::capacity, this);
        return result = vect_.size();
    }

    virtual T const& item(boost::contract::virtual_* v = 0)
            const /* override */ {
        boost::optional<T const&> result;
        boost::contract::check c = boost::contract::public_function<
                override_item>(v, result, &vstack::item, this);
        return *(result = vect_[len_ - 1]);
    }

    virtual void push(T const& value, boost::contract::virtual_* v = 0)
            /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_push>(v, &vstack::push, this, value);
        vect_[len_++] = value;
    }

    virtual T const& pop(boost::contract::virtual_* v = 0) /* override */ {
        boost::optional<T const&> result;
        boost::contract::check c = boost::contract::public_function<
                override_pop>(v, result, &vstack::pop, this);
        return *(result = vect_[--len_]);
    }

    virtual void clear(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_clear>(v, &vstack::clear, this);
        len_ = 0;
    }

private:
    vector<T> vect_;
    int len_;
};

int main() {
    vstack<int> s(3);
    assert(s.capacity() == 3);

    s.push(123);
    assert(s.length() == 1);
    assert(s.pop() == 123);

    return 0;
}
//]

