
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_name_list
#include <boost/contract.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>

// List of names.
class name_list {
    friend class boost::contract::access;

    void invariant() const {
        BOOST_CONTRACT_ASSERT(count() >= 0); // Non-negative count.
    }
    
public:
    /* Creation */

    // Create an empty list.
    name_list() {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == 0); // Empty list.
            })
        ;
    }

    // Destroy list.
    virtual ~name_list() {
        // Check invariants.
        boost::contract::check c = boost::contract::destructor(this);
    }

    /* Basic Queries */

    // Number of names in list.
    int count() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return names_.size();
    }

    // Is name in list?
    bool has(std::string const& name) const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // If empty, has not.
                if(count() == 0) BOOST_CONTRACT_ASSERT(!result);
            })
        ;

        return result = names_.cend() != std::find(names_.cbegin(),
                names_.cend(), name);
    }

    /* Commands */

    // Add name to list, if name not already in list.
    virtual void put(std::string const& name,
            boost::contract::virtual_* v = 0) {
        boost::contract::old_ptr<bool> old_has_name =
                BOOST_CONTRACT_OLDOF(v, has(name));
        boost::contract::old_ptr<int> old_count =
                BOOST_CONTRACT_OLDOF(v, count());
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!has(name)); // Not already in list.
            })
            .postcondition([&] {
                if(!*old_has_name) { // If-guard allows to relax subcontracts.
                    BOOST_CONTRACT_ASSERT(has(name)); // Name in list.
                    BOOST_CONTRACT_ASSERT(count() == *old_count + 1); // Inc.
                }
            })
        ;

        names_.push_back(name);
    }

private:
    std::vector<std::string> names_;
};

class relaxed_name_list
    #define BASES public name_list
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types; // Subcontracting.
    #undef BASES
    
    BOOST_CONTRACT_OVERRIDE(put);

public:
    /*  Commands */

    // Add name to list, or do nothing if name already in list (relaxed).
    void put(std::string const& name,
            boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::old_ptr<bool> old_has_name =
                BOOST_CONTRACT_OLDOF(v, has(name));
        boost::contract::old_ptr<int> old_count =
                BOOST_CONTRACT_OLDOF(v, count());
        boost::contract::check c = boost::contract::public_function<
                override_put>(v, &relaxed_name_list::put, this, name)
            .precondition([&] { // Relax inherited preconditions.
                BOOST_CONTRACT_ASSERT(has(name)); // Already in list.
            })
            .postcondition([&] { // Inherited post. not checked given if-guard.
                if(*old_has_name) {
                    // Count unchanged if name already in list.
                    BOOST_CONTRACT_ASSERT(count() == *old_count);
                }
            })
        ;

        if(!has(name)) name_list::put(name); // Else, do nothing.
    }
};

int main() {
    std::string const js = "John Smith";

    relaxed_name_list rl;
    rl.put(js);
    assert(rl.has(js));
    rl.put(js); // OK, relaxed contracts allow calling this again (do nothing).

    name_list nl;
    nl.put(js);
    assert(nl.has(js));
    // nl.put(js); // Error, contracts do not allow calling this again.

    return 0;
}
//]

