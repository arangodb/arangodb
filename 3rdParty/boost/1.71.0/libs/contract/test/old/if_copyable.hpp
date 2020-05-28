
#ifndef BOOST_CONTRACT_TEST_IF_COPYABLE_HPP_
#define BOOST_CONTRACT_TEST_IF_COPYABLE_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test old values of non-copyable types.

#define BOOST_CONTRACT_TEST_IF_COPYABLE_TYPE(class_) \
    public: \
        explicit class_(int value) : value_(value) {} \
        \
        friend class_& operator++(class_& me) { ++me.value_; return me; } \
        \
        friend bool operator>(class_ const& left, class_ const& right) { \
            return left.value_ > right.value_; \
        } \
        \
        friend bool operator==(class_ const& left, class_ const& right) { \
            return left.value_ == right.value_; \
        } \
        \
        friend class_ operator+(class_ const& left, class_ const& right) { \
            return class_(left.value_ + right.value_); \
        } \
        \
    private: \
        int value_;

struct cp { // Copyable type.
    BOOST_CONTRACT_TEST_IF_COPYABLE_TYPE(cp)
};

struct ncp {
    BOOST_CONTRACT_TEST_IF_COPYABLE_TYPE(ncp)

private: // Non (publicly) copyable type.
    ncp(ncp const& other) : value_(other.value_) {}
};

#endif // #include guard

