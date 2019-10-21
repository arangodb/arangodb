
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_stack
#include <boost/contract.hpp>
#include <boost/optional.hpp>
#include <vector>
#include <cassert>

template<typename T>
class stack {
    friend class boost::contract::access;

    void invariant() const {
        BOOST_CONTRACT_ASSERT(count() >= 0); // Non-negative count.
    }

public:
    /* Creation */

    // Create empty stack.
    stack() {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == 0); // Empty.
            })
        ;
    }

    // Destroy stack.
    virtual ~stack() {
        // Check invariants.
        boost::contract::check c = boost::contract::destructor(this);
    }

    /* Basic Queries */

    // Number of items.
    int count() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return items_.size();
    }

    // Item at index in [1, count()] (as in Eiffel).
    T const& item_at(int index) const {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(index > 0); // Positive index.
                BOOST_CONTRACT_ASSERT(index <= count()); // Index within count.
            })
        ;

        return items_[index - 1];
    }

    /* Derived Queries */

    // If no items.
    bool is_empty() const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Consistent with count.
                BOOST_CONTRACT_ASSERT(result == (count() == 0));
            })
        ;

        return result = (count() == 0);
    }

    // Top item.
    T const& item() const {
        boost::optional<T const&> result; // Avoid extra construction of T.
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(count() > 0); // Not empty.
            })
            .postcondition([&] {
                // Item on top.
                BOOST_CONTRACT_ASSERT(*result == item_at(count()));
            })
        ;

        return *(result = item_at(count()));
    }

    /* Commands */

    // Push item to the top.
    void put(T const& new_item) {
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == *old_count + 1); // Count inc.
                BOOST_CONTRACT_ASSERT(item() == new_item); // Item set.
            })
        ;

        items_.push_back(new_item);
    }

    // Pop top item.
    void remove() {
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(count() > 0); // Not empty.
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == *old_count - 1); // Count dec.
            })
        ;

        items_.pop_back();
    }

private:
    std::vector<T> items_;
};

int main() {
    stack<int> s;
    assert(s.count() == 0);

    s.put(123);
    assert(s.item() == 123);

    s.remove();
    assert(s.is_empty());

    return 0;
}
//]

