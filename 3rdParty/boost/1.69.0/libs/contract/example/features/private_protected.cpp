
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <limits>
#include <cassert>

//[private_protected
class counter {
protected: // Protected functions use `function()` (like non-members).
    void set(int n) {
        boost::contract::check c = boost::contract::function()
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(n <= 0);
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(get() == n);
            })
        ;

        n_ = n;
    }

private: // Private functions use `function()` (like non-members).
    void dec() {
        boost::contract::old_ptr<int> old_get = BOOST_CONTRACT_OLDOF(get());
        boost::contract::check c = boost::contract::function()
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(
                        get() + 1 >= std::numeric_limits<int>::min());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(get() == *old_get - 1);
            })
        ;

        set(get() - 1);
    }
    
    int n_;
    
    /* ... */
//]

public:
    int get() const {
        int result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(result <= 0);
                BOOST_CONTRACT_ASSERT(result == n_);
            })
        ;

        return result = n_;
    }

    counter() : n_(0) {} // Should contract constructor and destructor too.
    
    void invariant() const {
        BOOST_CONTRACT_ASSERT(get() <= 0);
    }

    friend int main();
};

int main() {
    counter cnt;
    assert(cnt.get() == 0);
    cnt.dec();
    assert(cnt.get() == -1);
    return 0;
}

