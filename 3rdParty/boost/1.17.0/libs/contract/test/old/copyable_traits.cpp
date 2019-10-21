
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test specializations of old value copy type traits.

#include <boost/contract/old.hpp>
#include <boost/type_traits.hpp>
#include <boost/noncopyable.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <cassert>

template<typename T>
void f(T& x) {
    // No OLDOF here so C++11 not required for this test.
    boost::contract::old_ptr_if_copyable<T> old_x = boost::contract::make_old(
            boost::contract::copy_old() ? x : boost::contract::null_old());
}

// Test copyable type but...
struct w {
    w() {}
    w(w const&) { BOOST_TEST(false); } // Test this doesn't get copied.
};

// ...never copy old values for type `w` (because its copy is too expensive).
namespace boost { namespace contract {
    template<>
    struct is_old_value_copyable<w> : boost::false_type {};
} } // namespace

// Test non-copyable type but...
struct p : private boost::noncopyable { // Non-copyable via Boost.
    static bool copied;
    p() : num_(new int(0)) {}
    ~p() { delete num_; }
private:
    int* num_;
    friend struct boost::contract::old_value_copy<p>;
};
bool p::copied = false;

// ...still copy old values for type `p` (using a deep copy).
namespace boost { namespace contract {
    template<>
    struct old_value_copy<p> {
        explicit old_value_copy(p const& old) {
            *old_.num_ = *old.num_; // Deep copy pointed value.
            p::copied = true;
        }

        p const& old() const { return old_; }

    private:
        p old_;
    };
    
    template<>
    struct is_old_value_copyable<p> : boost::true_type {};
} } // namespace

// Non-copyable type so...
struct n {
    n() {}
private:
    n(n const&); // Non-copyable (but not via Boost).
};

// ...specialize `boost::is_copy_constructible` (no need for this on C++11).
namespace boost { namespace contract {
    template<>
    struct is_old_value_copyable<n> : boost::false_type {};
} } // namespace

int main() {
    // NOTE: No test::detail::counter below because that is for copyable types.

    {
        int x; // Test has copy ctor so copy olds.
        f(x);
    }
    
    {
        w x; // Test has copy ctor but never copy olds (see TEST(...) above).
        f(x);
    }

    p::copied = false;
    {
        p x; // Test no copy ctor but still old copies.
        f(x);
    }
    #ifndef BOOST_CONTRACT_NO_OLDS
        BOOST_TEST(p::copied);
    #else
        BOOST_TEST(!p::copied);
    #endif

    {
        n x; // Test no copy ctor so no old copies.
        f(x);
    }
  
    return boost::report_errors();
}

