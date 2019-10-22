
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <vector>
#include <algorithm>
#include <cassert>

//[public_class_begin
class unique_identifiers :
    private boost::contract::constructor_precondition<unique_identifiers>
{
public:
    void invariant() const {
        BOOST_CONTRACT_ASSERT(size() >= 0);
    }
//]

//[public_constructor
public:
    // Contract for a constructor.
    unique_identifiers(int from, int to) :
        boost::contract::constructor_precondition<unique_identifiers>([&] {
            BOOST_CONTRACT_ASSERT(from <= to);
        })
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(size() == (to - from + 1));
            })
        ;

        // Constructor body.
        for(int id = from; id <= to; ++id) vect_.push_back(id);
    }
//]
    
//[public_destructor
public:
    // Contract for a destructor.
    virtual ~unique_identifiers() {
        // Following contract checks class invariants.
        boost::contract::check c = boost::contract::destructor(this);

        // Destructor body here... (do nothing in this example).
    }
//]

    int size() const {
        // Following contract checks invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return vect_.size();
    }

//[public_function
public:
    // Contract for a public function (but no static, virtual, or override).
    bool find(int id) const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                if(size() == 0) BOOST_CONTRACT_ASSERT(!result);
            })
        ;

        // Function body.
        return result = std::find(vect_.begin(), vect_.end(), id) !=
                vect_.end();
    }
//]

//[public_virtual_function
public:
    // Contract for a public virtual function (but no override).
    virtual int push_back(int id, boost::contract::virtual_* v = 0) { // Extra `v`.
        int result;
        boost::contract::old_ptr<bool> old_find =
                BOOST_CONTRACT_OLDOF(v, find(id)); // Pass `v`.
        boost::contract::old_ptr<int> old_size =
                BOOST_CONTRACT_OLDOF(v, size()); // Pass `v`.
        boost::contract::check c = boost::contract::public_function(
                v, result, this) // Pass `v` and `result`.
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!find(id)); // ID cannot be already present.
            })
            .postcondition([&] (int const result) {
                if(!*old_find) {
                    BOOST_CONTRACT_ASSERT(find(id));
                    BOOST_CONTRACT_ASSERT(size() == *old_size + 1);
                }
                BOOST_CONTRACT_ASSERT(result == id);
            })
        ;

        // Function body.
        vect_.push_back(id);
        return result = id;
    }
//]
    
private:
    std::vector<int> vect_;
//[public_class_end
    /* ... */
};
//]

//[public_derived_class_begin
class identifiers
    #define BASES public unique_identifiers
    : BASES
{
public:
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types; // Bases typedef.
    #undef BASES
    
    void invariant() const { // Check in AND with bases.
        BOOST_CONTRACT_ASSERT(empty() == (size() == 0));
    }
//]
    
//[public_function_override
public:
    // Contract for a public function override.
    int push_back(int id, boost::contract::virtual_* v = 0) /* override */ {
        int result;
        boost::contract::old_ptr<bool> old_find =
                BOOST_CONTRACT_OLDOF(v, find(id));
        boost::contract::old_ptr<int> old_size =
                BOOST_CONTRACT_OLDOF(v, size());
        boost::contract::check c = boost::contract::public_function<
            override_push_back // Pass override plus below function pointer...
        >(v, result, &identifiers::push_back, this, id) // ...and arguments.
            .precondition([&] { // Check in OR with bases.
                BOOST_CONTRACT_ASSERT(find(id)); // ID can be already present.
            })
            .postcondition([&] (int const result) { // Check in AND with bases.
                if(*old_find) BOOST_CONTRACT_ASSERT(size() == *old_size);
            })
        ;

        // Function body.
        if(!find(id)) unique_identifiers::push_back(id); // Else, do nothing.
        return result = id;
    }
    BOOST_CONTRACT_OVERRIDE(push_back) // Define `override_push_back`.
//]
    
    bool empty() const {
        // Following contract checks invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return size() == 0;
    }
    
    identifiers(int from, int to) : unique_identifiers(from, to) {
        // Following contract checks invariants.
        boost::contract::check c = boost::contract::constructor(this);
    }

//[public_derived_class_end
    /* ... */
};
//]
    
int main() {
    unique_identifiers uids(1, 4);
    assert(uids.find(2));
    assert(!uids.find(5));
    uids.push_back(5);
    assert(uids.find(5));

    identifiers ids(10, 40);
    assert(!ids.find(50));
    ids.push_back(50);
    ids.push_back(50);
    assert(ids.find(50));
    
    return 0;
}

