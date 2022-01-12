
// Copyright (C) 2008-2019 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#include <utility>
#include <cassert>

//[if_constexpr
template<typename T>
void swap(T& x, T& y) {
    constexpr bool b = boost::contract::is_old_value_copyable<T>::value &&
            boost::has_equal_to<T>::value;
    boost::contract::old_ptr<T> old_x, old_y;
    if constexpr(b) { // Contract requires copyable T...
        old_x = BOOST_CONTRACT_OLDOF(x);
        old_y = BOOST_CONTRACT_OLDOF(y);
    }
    boost::contract::check c = boost::contract::function()
        .postcondition([&] {
            if constexpr(b) { // ... and T with `==`...
                BOOST_CONTRACT_ASSERT(x == *old_y);
                BOOST_CONTRACT_ASSERT(y == *old_x);
            }
        })
    ;

    T t = std::move(x); // ...but body only requires movable T.
    x = std::move(y);
    y = std::move(t);
}
//]

struct i { // Non-copyable but has operator==.
    explicit i(int n) : n_(n) {}

    i(i const&) = delete; // Non-copyable.
    i& operator=(i const&) = delete;

    i(i const&& o) : n_(o.n_) {}
    i& operator=(i const&& o) { n_ = o.n_; return *this; }

    friend bool operator==(i const& l, i const& r) { // Operator==.
        return l.n_ == r.n_;
    }

private:
    int n_;
};

struct j { // Copyable but no operator==.
    explicit j(int n) : n_(n) {}

    j(j const& o) : n_(o.n_) {} // Copyable.
    j& operator=(j const& o) { n_ = o.n_; return *this; }

    j(j const&& o) : n_(o.n_) {}
    j& operator=(j const&& o) { n_ = o.n_; return *this; }

    // No operator==.

private:
    int n_;
};

struct k { // Non-copyable and no operator==.
    explicit k(int n) : n_(n) {}

    k(k const&) = delete; // Non-copyable.
    k& operator=(k const&) = delete;

    k(k const&& o) : n_(o.n_) {}
    k& operator=(k const&& o) { n_ = o.n_; return *this; }

    // No operator==.

private:
    int n_;
};

int main() {
    { // Copyable and operator== (so checks postconditions).
        int x = 123, y = 456;
        swap(x, y);
        assert(x == 456);
        assert(y == 123);
    }
    
    { // Non-copyable (so does not check postconditions).
        i x{123}, y{456};
        swap(x, y);
        assert(x == i{456});
        assert(y == i{123});
    }
    
    { // No operator== (so does not check postconditions).
        j x{123}, y{456};
        swap(x, y);
        // Cannot assert x and y because no operator==.
    }
    
    { // Non-copyable and no operator== (so does not check postconditions).
        k x{123}, y{456};
        swap(x, y);
        // Cannot assert x and y because no operator==.
    }
    
    return 0;
}

