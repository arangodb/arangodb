
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <boost/optional.hpp>
#include <vector>
#include <cassert>

template<typename T>
class accessible {
public:
    virtual T& at(unsigned index, boost::contract::virtual_* v = 0) = 0;

    // Could program class invariants and contracts for following too.
    virtual T const& operator[](unsigned index) const = 0;
    virtual unsigned size() const = 0;
};

//[optional_result_virtual
template<typename T>
T& accessible<T>::at(unsigned index, boost::contract::virtual_* v) {
    boost::optional<T&> result;
    // Pass `result` right after `v`...
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(index < size());
        })
        // ...plus postconditions take `result` as a parameter (not capture).
        .postcondition([&] (boost::optional<T const&> const& result) {
            BOOST_CONTRACT_ASSERT(*result == operator[](index));
        })
    ;

    assert(false);
    return *result;
}
//]

template<typename T>
class vector
    #define BASES public accessible<T>
    : BASES
{
public:
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    T& at(unsigned index, boost::contract::virtual_* v = 0) /* override */ {
        boost::optional<T&> result;
        // Pass `result` right after `v`...
        boost::contract::check c = boost::contract::public_function<
                override_at>(v, result, &vector::at, this, index)
            // ...plus postconditions take `result` as parameter (not capture).
            .postcondition([&] (boost::optional<T const&> const& result) {
                if(index == 0) BOOST_CONTRACT_ASSERT(*result == front());
            })
        ;

        return *(result = vect_[index]);
    }

    // Could program class invariants and contracts for following too.
    T const& operator[](unsigned index) const { return vect_[index]; }
    unsigned size() const { return vect_.size(); }
    T const& front() const { return vect_.front(); }
    void push_back(T const& value) { vect_.push_back(value); }

    BOOST_CONTRACT_OVERRIDE(at)

private:
    std::vector<T> vect_;
};

int main() {
    vector<int> v;
    v.push_back(123);
    v.push_back(456);
    v.push_back(789);
    int& x = v.at(1);
    assert(x == 456);
    x = -456;
    assert(v.at(1) == -456);
    return 0;
}

