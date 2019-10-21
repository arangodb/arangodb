
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <vector>
#include <limits>
#include <cassert>

//[ifdef_function
// Use #ifdef to completely disable contract code compilation.
#include <boost/contract/core/config.hpp>
#ifndef BOOST_CONTRACT_NO_ALL
    #include <boost/contract.hpp>
#endif

int inc(int& x) {
    int result;
    #ifndef BOOST_CONTRACT_NO_OLDS
        boost::contract::old_ptr<int> old_x = BOOST_CONTRACT_OLDOF(x);
    #endif
    #ifndef BOOST_CONTRACT_NO_FUNCTIONS
        boost::contract::check c = boost::contract::function()
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                .precondition([&] {
                    BOOST_CONTRACT_ASSERT(x < std::numeric_limits<int>::max());
                })
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                .postcondition([&] {
                    BOOST_CONTRACT_ASSERT(x == *old_x + 1);
                    BOOST_CONTRACT_ASSERT(result == *old_x);
                })
            #endif
        ;
    #endif

    return result = x++;
}
//]

template<typename T>
class pushable {
    #ifndef BOOST_CONTRACT_NO_ALL
        friend class boost::contract::access;
    #endif

    #ifndef BOOST_CONTRACT_NO_INVARIANTS
        void invariant() const {
            BOOST_CONTRACT_ASSERT(capacity() <= max_size());
        }
    #endif

public:
    virtual void push_back(
        T const& x
        #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            , boost::contract::virtual_* v = 0
        #endif
    ) = 0;

protected:
    virtual unsigned capacity() const = 0;
    virtual unsigned max_size() const = 0;
};

template<typename T>
void pushable<T>::push_back(
    T const& x
    #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
        , boost::contract::virtual_* v
    #endif
) {
    #ifndef BOOST_CONTRACT_NO_OLDS
        boost::contract::old_ptr<unsigned> old_capacity =
                BOOST_CONTRACT_OLDOF(v, capacity());
    #endif
    #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
        boost::contract::check c = boost::contract::public_function(v, this)
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                .precondition([&] {
                    BOOST_CONTRACT_ASSERT(capacity() < max_size());
                })
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                .postcondition([&] {
                    BOOST_CONTRACT_ASSERT(capacity() >= *old_capacity);
                })
            #endif
        ;
    #endif
    assert(false); // Shall never execute this body.
}

//[ifdef_class
class integers
    #define BASES public pushable<int>
    :
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            private boost::contract::constructor_precondition<integers>, BASES
        #else
            BASES
        #endif
{
    #ifndef BOOST_CONTRACT_NO_ALL
        friend class boost::contract::access;
    #endif

    #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
        typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #endif
    #undef BASES

    #ifndef BOOST_CONTRACT_NO_INVARIANTS
        void invariant() const {
            BOOST_CONTRACT_ASSERT(size() <= capacity());
        }
    #endif

public:
    integers(int from, int to) :
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            boost::contract::constructor_precondition<integers>([&] {
                BOOST_CONTRACT_ASSERT(from <= to);
            }),
        #endif
        vect_(to - from + 1)
    {
        #ifndef BOOST_CONTRACT_NO_CONSTRUCTORS
            boost::contract::check c = boost::contract::constructor(this)
                #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                    .postcondition([&] {
                        BOOST_CONTRACT_ASSERT(int(size()) == (to - from + 1));
                    })
                #endif
            ;
        #endif

        for(int x = from; x <= to; ++x) vect_.at(x - from) = x;
    }
    
    virtual ~integers() {
        #ifndef BOOST_CONTRACT_NO_DESTRUCTORS
            // Check invariants.
            boost::contract::check c = boost::contract::destructor(this);
        #endif
    }

    virtual void push_back(
        int const& x
        #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            , boost::contract::virtual_* v = 0
        #endif
    ) /* override */ {
        #ifndef BOOST_CONTRACT_NO_OLDS
            boost::contract::old_ptr<unsigned> old_size;
        #endif
        #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            boost::contract::check c = boost::contract::public_function<
                    override_push_back>(v, &integers::push_back, this, x)
                #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                    .precondition([&] {
                        BOOST_CONTRACT_ASSERT(size() < max_size());
                    })
                #endif
                #ifndef BOOST_CONTRACT_NO_OLDS
                    .old([&] {
                        old_size = BOOST_CONTRACT_OLDOF(v, size());
                    })
                #endif
                #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                    .postcondition([&] {
                        BOOST_CONTRACT_ASSERT(size() == *old_size + 1);
                    })
                #endif
                #ifndef BOOST_CONTRACT_NO_EXCEPTS
                    .except([&] {
                        BOOST_CONTRACT_ASSERT(size() == *old_size);
                    })
                #endif
            ;
        #endif

        vect_.push_back(x);
    }

private:
    #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
        BOOST_CONTRACT_OVERRIDE(push_back)
    #endif

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

