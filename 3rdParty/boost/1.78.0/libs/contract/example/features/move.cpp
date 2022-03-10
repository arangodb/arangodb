
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <vector>
#include <utility>
#include <cassert>

//[move
class circular_buffer :
        private boost::contract::constructor_precondition<circular_buffer> {
public:
    void invariant() const {
        if(!moved()) { // Do not check (some) invariants for moved-from objects.
            BOOST_CONTRACT_ASSERT(index() < size());
        }
        // More invariants here that hold also for moved-from objects (e.g.,
        // all assertions necessary to successfully destroy moved-from objects).
    }
    
    // Move constructor.
    circular_buffer(circular_buffer&& other) :
        boost::contract::constructor_precondition<circular_buffer>([&] {
            BOOST_CONTRACT_ASSERT(!other.moved());
        })
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!moved());
                BOOST_CONTRACT_ASSERT(other.moved());
            })
        ;

        move(std::forward<circular_buffer>(other));
    }
    
    // Move assignment.
    circular_buffer& operator=(circular_buffer&& other) {
        // Moved-from can be (move) assigned (so no pre `!moved()` here).
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!other.moved());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!moved());
                BOOST_CONTRACT_ASSERT(other.moved());
            })
        ;

        return move(std::forward<circular_buffer>(other));
    }
    
    ~circular_buffer() {
        // Moved-from can always be destroyed (in fact no preconditions).
        boost::contract::check c = boost::contract::destructor(this);
    }
    
    bool moved() const {
        boost::contract::check c = boost::contract::public_function(this);
        return moved_;
    }

private:
    bool moved_;

    /* ... */
//]

public:
    explicit circular_buffer(std::vector<char> const& data,
            unsigned start = 0) :
        boost::contract::constructor_precondition<circular_buffer>([&] {
            BOOST_CONTRACT_ASSERT(start < data.size());
        }),
        moved_(false),
        data_(data),
        index_(start)
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!moved());
            })
        ;
    }

    // Copy constructor.
    circular_buffer(circular_buffer const& other) :
        boost::contract::constructor_precondition<circular_buffer>([&] {
            BOOST_CONTRACT_ASSERT(!other.moved());
        })
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!moved());
            })
        ;

        copy(other);
    }

    // Copy assignment.
    circular_buffer& operator=(circular_buffer const& other) {
        // Moved-from can be (copy) assigned (so no pre `!moved()` here).
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!other.moved());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!moved());
            })
        ;

        return copy(other);
    }

    char read() {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!moved());
            })
        ;

        unsigned i = index_++;
        if(index_ == data_.size()) index_ = 0; // Circular.
        return data_.at(i);
    }

private:
    circular_buffer& copy(circular_buffer const& other) {
        data_ = other.data_;
        index_ = other.index_;
        moved_ = false;
        return *this;
    }

    circular_buffer& move(circular_buffer&& other) {
        data_ = std::move(other.data_);
        index_ = std::move(other.index_);
        moved_ = false;
        other.moved_ = true; // Mark moved-from object.
        return *this;
    }

    std::vector<char> data_;
    unsigned index_;

public:
    unsigned index() const {
        boost::contract::check c = boost::contract::public_function(this);
        return index_;
    }

    unsigned size() const {
        boost::contract::check c = boost::contract::public_function(this);
        return data_.size();
    }
};

int main() {
    struct err {};
    boost::contract::set_precondition_failure(
            [] (boost::contract::from) { throw err(); });

    {
        circular_buffer x({'a', 'b', 'c', 'd'}, 2);
        assert(x.read() == 'c');

        circular_buffer y1 = x; // Copy constructor.
        assert(y1.read() == 'd');
        assert(x.read() == 'd');

        circular_buffer y2({'h'});
        y2 = x; // Copy assignment.
        assert(y2.read() == 'a');
        assert(x.read() == 'a');

        circular_buffer z1 = std::move(x); // Move constructor.
        assert(z1.read() == 'b');
        // Calling `x.read()` would fail `!moved()` precondition.

        x = y1; // Moved-from `x` can be copy assigned.
        assert(x.read() == 'a');
        assert(y1.read() == 'a');

        circular_buffer z2({'k'});
        z2 = std::move(x); // Move assignment.
        assert(z2.read() == 'b');
        // Calling `x.read()` would fail `!moved()` precondition.

        x = std::move(y2); // Moved-from `x` can be move assigned.
        assert(x.read() == 'b');
        // Calling `y2.read()` would fail `!moved()` precondition.

    } // Moved-from `y2` can be destroyed.

    return 0;
}

