
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <vector>
#include <algorithm>
#include <cassert>

template<typename T>
class pushable {
public:
    void invariant() const {
        BOOST_CONTRACT_ASSERT(capacity() <= max_size());
    }

    virtual void push_back(T x, boost::contract::virtual_* v = 0) = 0;

protected:
    virtual unsigned capacity() const = 0;
    virtual unsigned max_size() const = 0;
};

template<typename T>
void pushable<T>::push_back(T x, boost::contract::virtual_* v) {
    boost::contract::old_ptr<unsigned> old_capacity =
            BOOST_CONTRACT_OLDOF(v, capacity());
    boost::contract::check c = boost::contract::public_function(v, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(capacity() < max_size());
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(capacity() >= *old_capacity);
        })
    ;
    assert(false); // Shall never execute this body.
}

struct has_size { virtual unsigned size() const = 0; };
struct has_empty { virtual bool empty() const = 0; };

class unique_chars
    : private boost::contract::constructor_precondition<unique_chars>
{
public:
    void invariant() const {
        BOOST_CONTRACT_ASSERT(size() >= 0);
    }

    unique_chars(char from, char to) :
        boost::contract::constructor_precondition<unique_chars>([&] {
            BOOST_CONTRACT_ASSERT(from <= to);
        })
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(int(size()) == (to - from + 1));
            })
        ;

        for(char x = from; x <= to; ++x) vect_.push_back(x);
    }

    virtual ~unique_chars() {
        boost::contract::check c = boost::contract::destructor(this);
    }

    unsigned size() const {
        boost::contract::check c = boost::contract::public_function(this);
        return vect_.size();
    }

    bool find(char x) const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                if(size() == 0) BOOST_CONTRACT_ASSERT(!result);
            })
        ;

        return result = std::find(vect_.begin(), vect_.end(), x) != vect_.end();
    }

    virtual void push_back(char x, boost::contract::virtual_* v = 0) {
        boost::contract::old_ptr<bool> old_find =
                BOOST_CONTRACT_OLDOF(v, find(x));
        boost::contract::old_ptr<unsigned> old_size =
                BOOST_CONTRACT_OLDOF(v, size());
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!find(x));
            })
            .postcondition([&] {
                if(!*old_find) {
                    BOOST_CONTRACT_ASSERT(find(x));
                    BOOST_CONTRACT_ASSERT(size() == *old_size + 1);
                }
            })
        ;

        vect_.push_back(x);
    }

protected:
    unique_chars() {}

    std::vector<char> const& vect() const { return vect_; }

private:
    std::vector<char> vect_;
};

//[base_types
class chars
    #define BASES /* local macro (for convenience) */ \
        private boost::contract::constructor_precondition<chars>, \
        public unique_chars, \
        public virtual pushable<char>, \
        virtual protected has_size, \
        private has_empty
    : BASES // Bases of this class.
{
public:
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types; // Bases typedef.
    #undef BASES // Undefine local macro.

    /* ... */
//]
    
    void invariant() const {
        BOOST_CONTRACT_ASSERT(empty() == (size() == 0));
    }
    
    chars(char from, char to) : unique_chars(from, to) {
        boost::contract::check c = boost::contract::constructor(this);
    }

    chars(char const* const c_str) :
        boost::contract::constructor_precondition<chars>([&] {
            BOOST_CONTRACT_ASSERT(c_str[0] != '\0');
        })
    {
        boost::contract::check c = boost::contract::constructor(this);

        for(unsigned i = 0; c_str[i] != '\0'; ++i) push_back(c_str[i]);
    }
    
    void push_back(char x, boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::old_ptr<bool> old_find =
                BOOST_CONTRACT_OLDOF(v, find(x));
        boost::contract::old_ptr<unsigned> old_size =
                BOOST_CONTRACT_OLDOF(v, size());
        boost::contract::check c = boost::contract::public_function<
                override_push_back>(v, &chars::push_back, this, x)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(find(x));
            })
            .postcondition([&] {
                if(*old_find) BOOST_CONTRACT_ASSERT(size() == *old_size);
            })
        ;

        if(!find(x)) unique_chars::push_back(x);
    }
    BOOST_CONTRACT_OVERRIDE(push_back);

    bool empty() const {
        boost::contract::check c = boost::contract::public_function(this);
        return size() == 0;
    }

    unsigned size() const { return unique_chars::size(); }

protected:
    unsigned max_size() const { return vect().max_size(); }
    unsigned capacity() const { return vect().capacity(); }
};
    
int main() {
    chars s("abc");
    assert(s.find('a'));
    assert(s.find('b'));
    assert(!s.find('x'));
    return 0;
}

