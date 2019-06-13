
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <string>
#include <cassert>

//[friend_byte
class buffer;

class byte {
    friend bool operator==(buffer const& left, byte const& right);
    
private:
    char value_;

    /* ... */
//]

public:
    // Could program invariants and contracts for following too.
    explicit byte(char value) : value_(value) {}
    bool empty() const { return value_ == '\0'; }
};

//[friend_buffer
class buffer {
    // Friend functions are not member functions...
    friend bool operator==(buffer const& left, byte const& right) {
        // ...so check contracts via `function` (which won't check invariants).
        boost::contract::check c = boost::contract::function()
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!left.empty());
                BOOST_CONTRACT_ASSERT(!right.empty());
            })
        ;
        
        for(char const* x = left.values_.c_str(); *x != '\0'; ++x) {
            if(*x != right.value_) return false;
        }
        return true;
    }

private:
    std::string values_;

    /* ... */
//]

public:
    // Could program invariants and contracts for following too.
    explicit buffer(std::string const& values) : values_(values) {}
    bool empty() const { return values_ == ""; }
};

int main() {
    buffer p("aaa");
    byte a('a');
    assert(p == a);

    buffer q("aba");
    assert(!(q == a)); // No operator!=.
    
    return 0;
}

