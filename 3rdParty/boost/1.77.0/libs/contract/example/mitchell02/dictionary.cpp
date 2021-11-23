
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_dictionary
#include <boost/contract.hpp>
#include <utility>
#include <map>
#include <cassert>

template<typename K, typename T>
class dictionary {
    friend class boost::contract::access;

    void invariant() const {
        BOOST_CONTRACT_ASSERT(count() >= 0); // Non-negative count.
    }

public:
    /* Creation */

    // Create empty dictionary.
    dictionary() {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == 0); // Empty.
            })
        ;
    }

    // Destroy dictionary.
    virtual ~dictionary() {
        // Check invariants.
        boost::contract::check c = boost::contract::destructor(this);
    }

    /* Basic Queries */

    // Number of key entries.
    int count() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return items_.size();
    }

    // Has entry for key?
    bool has(K const& key) const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Empty has no key.
                if(count() == 0) BOOST_CONTRACT_ASSERT(!result);
            })
        ;

        return result = (items_.find(key) != items_.end());
    }

    // Value for a given key.
    T const& value_for(K const& key) const {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(has(key)); // Has key.
            })
        ;

        // Find != end because of precondition (no defensive programming).
        return items_.find(key)->second;
    }

    /* Commands */

    // Add value of a given key.
    void put(K const& key, T const& value) {
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!has(key)); // Has not key already.
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == *old_count + 1); // Count inc.
                BOOST_CONTRACT_ASSERT(has(key)); // Has key.
                // Value set for key.
                BOOST_CONTRACT_ASSERT(value_for(key) == value);
            })
        ;

        items_.insert(std::make_pair(key, value));
    }

    // Remove value for given key.
    void remove(K const& key) {
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(has(key)); // Has key.
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == *old_count - 1); // Count dec.
                BOOST_CONTRACT_ASSERT(!has(key)); // Has not key.
            })
        ;

        items_.erase(key);
    }

private:
    std::map<K, T> items_;
};

int main() {
    std::string const js = "John Smith";

    dictionary<std::string, int> ages;
    assert(!ages.has(js));

    ages.put(js, 23);
    assert(ages.value_for(js) == 23);

    ages.remove(js);
    assert(ages.count() == 0);
    
    return 0;
}
//]

