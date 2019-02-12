
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <cassert>
#include <vector>

//[old_no_macro
template<typename T>
class vector {
public:
    virtual void push_back(T const& value, boost::contract::virtual_* v = 0) {
        // Program old value instead of using `OLD(size())` macro.
        boost::contract::old_ptr<unsigned> old_size =
            boost::contract::make_old(v, boost::contract::copy_old(v) ?
                    size() : boost::contract::null_old())
        ;

        boost::contract::check c = boost::contract::public_function(v, this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(size() == *old_size + 1);
            })
        ;

        vect_.push_back(value);
    }

    /* ... */
//]

    unsigned size() const { return vect_.size(); }

private:
    std::vector<T> vect_;
};

int main() {
    vector<int> vect;
    vect.push_back(123);
    assert(vect.size() == 1);
    return 0;
}

