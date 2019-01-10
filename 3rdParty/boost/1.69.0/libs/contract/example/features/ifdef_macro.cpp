
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <vector>
#include <limits>
#include <cassert>

//[ifdef_macro_function
// Use macro interface to completely disable contract code compilation.
#include <boost/contract_macro.hpp>

int inc(int& x) {
    int result;
    BOOST_CONTRACT_OLD_PTR(int)(old_x, x);
    BOOST_CONTRACT_FUNCTION()
        BOOST_CONTRACT_PRECONDITION([&] {
            BOOST_CONTRACT_ASSERT(x < std::numeric_limits<int>::max());
        })
        BOOST_CONTRACT_POSTCONDITION([&] {
            BOOST_CONTRACT_ASSERT(x == *old_x + 1);
            BOOST_CONTRACT_ASSERT(result == *old_x);
        })
    ;

    return result = x++;
}
//]

template<typename T>
class pushable {
    friend class boost::contract::access; // Left in code (almost no overhead).

    BOOST_CONTRACT_INVARIANT({
        BOOST_CONTRACT_ASSERT(capacity() <= max_size());
    })

public:
    virtual void push_back(
        T const& x,
        boost::contract::virtual_* v = 0 // Left in code (almost no overhead).
    ) = 0;

protected:
    virtual unsigned capacity() const = 0;
    virtual unsigned max_size() const = 0;
};

template<typename T>
void pushable<T>::push_back(T const& x, boost::contract::virtual_* v) {
    BOOST_CONTRACT_OLD_PTR(unsigned)(v, old_capacity, capacity());
    BOOST_CONTRACT_PUBLIC_FUNCTION(v, this)
        BOOST_CONTRACT_PRECONDITION([&] {
            BOOST_CONTRACT_ASSERT(capacity() < max_size());
        })
        BOOST_CONTRACT_POSTCONDITION([&] {
            BOOST_CONTRACT_ASSERT(capacity() >= *old_capacity);
        })
    ;
    assert(false); // Shall never execute this body.
}

//[ifdef_macro_class
class integers
    #define BASES public pushable<int>
    :
        // Left in code (almost no overhead).
        private boost::contract::constructor_precondition<integers>,
        BASES
{
    // Followings left in code (almost no overhead).
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    BOOST_CONTRACT_INVARIANT({
        BOOST_CONTRACT_ASSERT(size() <= capacity());
    })

public:
    integers(int from, int to) :
        BOOST_CONTRACT_CONSTRUCTOR_PRECONDITION(integers)([&] {
            BOOST_CONTRACT_ASSERT(from <= to);
        }),
        vect_(to - from + 1)
    {
        BOOST_CONTRACT_CONSTRUCTOR(this)
            BOOST_CONTRACT_POSTCONDITION([&] {
                BOOST_CONTRACT_ASSERT(int(size()) == (to - from + 1));
            })
        ;

        for(int x = from; x <= to; ++x) vect_.at(x - from) = x;
    }
    
    virtual ~integers() {
        BOOST_CONTRACT_DESTRUCTOR(this); // Check invariants.
    }

    virtual void push_back(
        int const& x,
        boost::contract::virtual_* v = 0 // Left in code (almost no overhead).
    ) /* override */ {
        BOOST_CONTRACT_OLD_PTR(unsigned)(old_size);
        BOOST_CONTRACT_PUBLIC_FUNCTION_OVERRIDE(override_push_back)(
                v, &integers::push_back, this, x)
            BOOST_CONTRACT_PRECONDITION([&] {
                BOOST_CONTRACT_ASSERT(size() < max_size());
            })
            BOOST_CONTRACT_OLD([&] {
                old_size = BOOST_CONTRACT_OLDOF(v, size());
            })
            BOOST_CONTRACT_POSTCONDITION([&] {
                BOOST_CONTRACT_ASSERT(size() == *old_size + 1);
            })
            BOOST_CONTRACT_EXCEPT([&] {
                BOOST_CONTRACT_ASSERT(size() == *old_size);
            })
        ;

        vect_.push_back(x);
    }

private:
    BOOST_CONTRACT_OVERRIDE(push_back) // Left in code (almost no overhead).

    /* ... */
//]

public: // Could program contracts for these too...
    unsigned size() const { return vect_.size(); }
    unsigned max_size() const { return vect_.max_size(); }
    unsigned capacity() const { return vect_.capacity(); }

private:
    std::vector<int> vect_;
};

int main() {
    integers i(1, 10);
    int x = 123;
    i.push_back(inc(x));
    assert(x == 124);
    assert(i.size() == 11);
    return 0;
}

