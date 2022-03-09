
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <iostream>
#include <cstring>
#include <cassert>

//[throw_on_failure_class_begin
struct too_large_error {};

template<unsigned MaxSize>
class cstring
    #define BASES private boost::contract::constructor_precondition<cstring< \
            MaxSize> >
    : BASES
{
//]
public:
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

//[throw_on_failure_ctor
public:
    /* implicit */ cstring(char const* chars) :
        boost::contract::constructor_precondition<cstring>([&] {
            BOOST_CONTRACT_ASSERT(chars); // Throw `assertion_failure`.
            // Or, throw user-defined exception.
            if(std::strlen(chars) > MaxSize) throw too_large_error();
        })
    {
//]
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(size() == std::strlen(chars));
            })
        ;

        size_ = std::strlen(chars);
        for(unsigned i = 0; i < size_; ++i) chars_[i] = chars[i];
        chars_[size_] = '\0';
    }
    
//[throw_on_failure_dtor
public:
    void invariant() const {
        if(size() > MaxSize) throw too_large_error(); // Throw user-defined ex.
        BOOST_CONTRACT_ASSERT(chars_); // Or, throw `assertion_failure`.
        BOOST_CONTRACT_ASSERT(chars_[size()] == '\0');
    }

    ~cstring() noexcept { // Exception specifiers apply to contract code.
        // Check invariants.
        boost::contract::check c = boost::contract::destructor(this);
    }
//]

    unsigned size() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return size_;
    }
    
private:
    char chars_[MaxSize + 1];
    unsigned size_;
//[throw_on_failure_class_end
    /* ... */
};
//]

void bad_throwing_handler() { // For docs only (not actually used here).
    //[throw_on_failure_bad_handler
    /* ... */

    // Warning... might cause destructors to throw (unless declared noexcept).
    boost::contract::set_invariant_failure(
        [] (boost::contract::from) {
            throw; // Throw no matter if from destructor, etc.
        }
    );

    /* ... */
    //]
}

//[throw_on_failure_handlers
int main() {
    boost::contract::set_precondition_failure(
    boost::contract::set_postcondition_failure(
    boost::contract::set_invariant_failure(
    boost::contract::set_old_failure(
        [] (boost::contract::from where) {
            if(where == boost::contract::from_destructor) {
                // Shall not throw from C++ destructors.
                std::clog << "ignored destructor contract failure" << std::endl;
            } else throw; // Re-throw (assertion_failure, user-defined, etc.).
        }
    ))));
    boost::contract::set_except_failure(
        [] (boost::contract::from) {
            // Already an active exception so shall not throw another...
            std::clog << "ignored exception guarantee failure" << std::endl;
        }
    );
    boost::contract::set_check_failure(
        [] {
            // But now CHECK shall not be used in destructor implementations.
            throw; // Re-throw (assertion_failure, user-defined, etc.).
        }
    );

    /* ... */
//]

    {
        cstring<3> s("abc");
        assert(s.size() == 3);
    }

    #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
        // These failures properly handled only when preconditions checked.

        try {
            char* c = 0;
            cstring<3> s(c);
            assert(false);
        } catch(boost::contract::assertion_failure const& error) {
            // OK (expected).
            std::clog << "ignored: " << error.what() << std::endl;
        } catch(...) { assert(false); }
        
        try {
            cstring<3> s("abcd");
            assert(false);
        } catch(too_large_error const&) {} // OK (expected).
        catch(...) { assert(false); }
    #endif

    return 0;
}

