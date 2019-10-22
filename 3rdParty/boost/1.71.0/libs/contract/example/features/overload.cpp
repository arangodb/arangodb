
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <boost/optional.hpp>
#include <string>
#include <sstream>
#include <cassert>

class lines {
public:
    virtual std::string str(boost::contract::virtual_* v = 0) const = 0;
    virtual std::string& str(boost::contract::virtual_* v = 0) = 0;
    
    virtual void put(std::string const& x,
            boost::contract::virtual_* v = 0) = 0;

    virtual void put(char x, boost::contract::virtual_* v = 0) = 0;
    
    virtual void put(int x, bool tab = false,
            boost::contract::virtual_* v = 0) = 0;
};

std::string lines::str(boost::contract::virtual_* v) const {
    std::string result;
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .postcondition([&] (std::string const& result) {
            if(result != "") BOOST_CONTRACT_ASSERT(*result.rbegin() == '\n');
        })
    ;
    assert(false);
    return result;
}

std::string& lines::str(boost::contract::virtual_* v) {
    boost::optional<std::string&> result;
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .postcondition([&] (boost::optional<std::string const&> const& result) {
            if(*result != "") BOOST_CONTRACT_ASSERT(*result->rbegin() == '\n');
        })
    ;
    assert(false);
    return *result;
}

void lines::put(std::string const& x, boost::contract::virtual_* v) {
    boost::contract::check c = boost::contract::public_function(v, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(*x.rbegin() != '\n');
        })
    ;
    assert(false);
}

void lines::put(char x, boost::contract::virtual_* v) {
    boost::contract::check c = boost::contract::public_function(v, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(x != '\n');
        })
    ;
    assert(false);
}

void lines::put(int x, bool tab,
        boost::contract::virtual_* v) {
    boost::contract::check c = boost::contract::public_function(v, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(x >= 0);
        })
    ;
    assert(false);
}

//[overload
class string_lines
    #define BASES public lines
    : BASES
{
public:
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES
    
    BOOST_CONTRACT_OVERRIDES(str) // Invoked only once for all `str` overloads.
    
    std::string str(boost::contract::virtual_* v = 0) const /* override */ {
        std::string result;
        boost::contract::check c = boost::contract::public_function<
                override_str>(
            v, result,
            // `static_cast` resolves overloaded function pointer ambiguities.
            static_cast<std::string (string_lines::*)(
                    boost::contract::virtual_*) const>(&string_lines::str),
            this
        );

        return result = str_;
    }
    
    // Overload on (absence of) `const` qualifier.
    std::string& str(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_str>(
            v, str_,
            // `static_cast` resolves overloaded function pointer ambiguities.
            static_cast<std::string& (string_lines::*)(
                    boost::contract::virtual_*)>(&string_lines::str),
            this
        );

        return str_;
    }
    
    BOOST_CONTRACT_OVERRIDES(put) // Invoked only once for all `put` overloads.

    void put(std::string const& x,
            boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::old_ptr<std::string> old_str =
                BOOST_CONTRACT_OLDOF(v, str());
        boost::contract::check c = boost::contract::public_function<
                override_put>(
            v,
            // `static_cast` resolves overloaded function pointer ambiguities.
            static_cast<void (string_lines::*)(std::string const&,
                    boost::contract::virtual_*)>(&string_lines::put),
            this, x
        )
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(str() == *old_str + x + '\n');
            })
        ;

        str_ = str_ + x + '\n';
    }
    
    // Overload on argument type.
    void put(char x, boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::old_ptr<std::string> old_str =
                BOOST_CONTRACT_OLDOF(v, str());
        boost::contract::check c = boost::contract::public_function<
                override_put>(
            v,
            // `static_cast` resolves overloaded function pointer ambiguities.
            static_cast<void (string_lines::*)(char,
                    boost::contract::virtual_*)>(&string_lines::put),
            this, x
        )
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(str() == *old_str + x + '\n');
            })
        ;
        
        str_ = str_ + x + '\n';
    }
    
    // Overload on argument type and arity (also with default parameter).
    void put(int x, bool tab = false,
            boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::old_ptr<std::string> old_str =
                BOOST_CONTRACT_OLDOF(v, str());
        boost::contract::check c = boost::contract::public_function<
                override_put>(
            v,
            // `static_cast` resolves overloaded function pointer ambiguities.
            static_cast<void (string_lines::*)(int, bool,
                    boost::contract::virtual_*)>(&string_lines::put),
            this, x, tab
        )
            .postcondition([&] {
                std::ostringstream s;
                s << x;
                BOOST_CONTRACT_ASSERT(
                        str() == *old_str + (tab ? "\t" : "") + s.str() + '\n');
            })
        ;

        std::ostringstream s;
        s << str_ << (tab ? "\t" : "") << x << '\n';
        str_ = s.str();
    }

private:
    std::string str_;
};
//]

int main() {
    string_lines s;
    s.put("abc");
    assert(s.str() == "abc\n");
    s.put('x');
    assert(s.str() == "abc\nx\n");
    s.put(10);
    assert(s.str() == "abc\nx\n10\n");
    s.put(20, true);
    lines const& l = s;
    assert(l.str() == "abc\nx\n10\n\t20\n");
    return 0;
}

