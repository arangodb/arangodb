
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#include <boost/bind.hpp>
#include <vector>
#include <functional>
#include <cassert>

//[condition_if
template<typename T>
class vector {
public:
    void push_back(T const& value) {
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Instead of `ASSERT(back() == value)` for T without `==`.
                BOOST_CONTRACT_ASSERT(
                    boost::contract::condition_if<boost::has_equal_to<T> >(
                        boost::bind(std::equal_to<T>(),
                            boost::cref(back()),
                            boost::cref(value)
                        )
                    )
                );
            })
        ;

        vect_.push_back(value);
    }

    /* ... */
//]

    T const& back() const { return vect_.back(); }

private:
    std::vector<T> vect_;
};

int main() {
    vector<int> v;
    v.push_back(1); // Type `int` has `==` so check postcondition.
    assert(v.back() == 1);

    struct i { int value; } j;
    j.value = 10;
    vector<i> w;
    w.push_back(j); // Type `i` has no `==` so skip postcondition.
    assert(j.value == 10);

    return 0;
}

