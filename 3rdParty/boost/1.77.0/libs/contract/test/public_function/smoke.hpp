
#ifndef BOOST_CONTRACT_TEST_PUBLIC_FUNCTION_CONTRACTS_HPP_
#define BOOST_CONTRACT_TEST_PUBLIC_FUNCTION_CONTRACTS_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test public member function subcontracting (also with old and return values).

#include "../detail/oteststream.hpp"
#include "../detail/counter.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/assert.hpp>
#include <boost/contract/old.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/override.hpp>
#include <boost/config.hpp>
#include <string>

boost::contract::test::detail::oteststream out;

struct s_tag;
typedef boost::contract::test::detail::counter<s_tag, std::string> s_type;

struct except_error {};

struct result_type {
    std::string value;
    explicit result_type(std::string const& s) : value(s) {}

private: // Test non-copyable and non-default-constructible result.
    result_type();
    result_type(result_type const&);
    result_type& operator=(result_type const&);
};

// Test base without additional bases and pure virtual.
template<char Id>
struct t {
    static void static_invariant() { out << Id << "::static_inv" << std::endl; }
    
    void invariant() const {
        out << Id << "::inv" << std::endl;
        BOOST_CONTRACT_ASSERT(z.value != "");
    }

    struct z_tag;
    typedef boost::contract::test::detail::counter<z_tag, std::string> z_type;
    z_type z;

    t() { z.value.push_back(Id); }

    virtual result_type& f(s_type& s, boost::contract::virtual_* v = 0) = 0;
};

template<char Id> // Requires: Only pass lower case Id so it'll never be 'X'.
result_type& t<Id>::f(s_type& s, boost::contract::virtual_* v) {
    std::ostringstream r; r << "none-" << Id;
    static result_type result(r.str());
    boost::contract::old_ptr<z_type> old_z =
            BOOST_CONTRACT_OLDOF(v, z_type::eval(z));
    boost::contract::old_ptr<s_type> old_s;
    boost::contract::check c = boost::contract::public_function(v, result, this)
        .precondition([&] {
            out << Id << "::f::pre" << std::endl;
            BOOST_CONTRACT_ASSERT(s.value[0] == Id || s.value[0] == 'X');
        })
        .old([&] {
            out << Id << "::f::old" << std::endl;
            old_s = BOOST_CONTRACT_OLDOF(v, s_type::eval(s));
        })
        .postcondition([&] (result_type const& result) {
            out << Id << "::f::post" << std::endl;
            BOOST_CONTRACT_ASSERT(z.value == old_z->value + old_s->value);
            BOOST_CONTRACT_ASSERT(s.value.find(old_z->value) !=
                    std::string::npos);
            BOOST_CONTRACT_ASSERT(result.value == old_s->value);
        })
        .except([&] {
            out << Id << "::f::except" << std::endl;
            BOOST_CONTRACT_ASSERT(z.value == old_z->value);
            BOOST_CONTRACT_ASSERT(s.value == old_s->value);
        })
    ;
    out << "t<" << Id << ">::f::body" << std::endl;
    if(s.value == "X") throw except_error();
    return result;
}

// Test base with other bases, multiple inheritance, and no subcontracting from
// protected and private bases (even if fully contracted).
struct c
    #define BASES public t<'d'>, protected t<'p'>, private t<'q'>, public t<'e'>
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() { out << "c::static_inv" << std::endl; }
    
    void invariant() const {
        out << "c::inv" << std::endl;
        BOOST_CONTRACT_ASSERT(y.value != "");
    }

    struct y_tag;
    typedef boost::contract::test::detail::counter<y_tag, std::string> y_type;
    y_type y;

    c() { y.value = "c"; }

    virtual result_type& f(s_type& s, boost::contract::virtual_* v = 0)
            /* override */ {
        static result_type result("none-c");
        boost::contract::old_ptr<y_type> old_y =
                BOOST_CONTRACT_OLDOF(v, y_type::eval(y));
        boost::contract::old_ptr<s_type> old_s;
        boost::contract::check c = boost::contract::public_function<
                override_f>(v, result, &c::f, this, s)
            .precondition([&] {
                out << "c::f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(s.value == "C" || s.value == "X");
            })
            .old([&] {
                out << "c::f::old" << std::endl;
                old_s = BOOST_CONTRACT_OLDOF(v, s_type::eval(s));
            })
            .postcondition([&] (result_type const& result) {
                out << "c::f::post" << std::endl;
                BOOST_CONTRACT_ASSERT(y.value == old_y->value + old_s->value);
                BOOST_CONTRACT_ASSERT(s.value.find(old_y->value) !=
                        std::string::npos);
                BOOST_CONTRACT_ASSERT(result.value == old_s->value);
            })
            .except([&] {
                out << "c::f::except" << std::endl;
                BOOST_CONTRACT_ASSERT(y.value == old_y->value);
                BOOST_CONTRACT_ASSERT(s.value == old_s->value);
            })
        ;

        out << "c::f::body" << std::endl;
        if(s.value == "X") throw except_error();
        std::string save_s = s.value;

        std::string save = y.value;
        y.value += save_s;
        s.value = save;

        save = t<'d'>::z.value;
        t<'d'>::z.value += save_s;
        s.value += save;

        save = t<'e'>::z.value;
        t<'e'>::z.value += save_s;
        s.value += save;

        result.value = save_s;
        return result;
    }
    BOOST_CONTRACT_OVERRIDE(f)
};

// Test no subcontracting from not (fully) contracted base.
struct b {
    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

    virtual ~b() {}

    // No contract (no virtual_ so this is not actually overridden by a::f).
    virtual result_type& f(s_type& s) {
        static result_type result("none-b");
        out << "b::f::body" << std::endl;
        result.value = s.value;
        return result;
    }
};

// Test public function with both non-contracted and contracted bases.
struct a
    #define BASES public b, public c
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() { out << "a::static_inv" << std::endl; }
    
    void invariant() const {
        out << "a::inv" << std::endl;
        BOOST_CONTRACT_ASSERT(x.value != "");
    }

    struct x_tag;
    typedef boost::contract::test::detail::counter<x_tag, std::string> x_type;
    x_type x;

    a() { x.value = "a"; }

    #if defined(BOOST_GCC)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Woverloaded-virtual" // For a::f.
    #elif defined(BOOST_CLANG)
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Woverloaded-virtual" // For a::f.
    #endif

    // Must use virtual_ even if no longer decl virtual for correct overloading.
    // NOTE: This intentionally hides but does not override `b::f` (it overrides
    // `c::f` instead). This generates warnings on some compilers (Clang, etc.).
    result_type& f(s_type& s, boost::contract::virtual_* v = 0)
            /* override */ {
        static result_type result("none-a");
        boost::contract::old_ptr<x_type> old_x =
                BOOST_CONTRACT_OLDOF(v, x_type::eval(x));
        boost::contract::old_ptr<s_type> old_s;
        boost::contract::check c = boost::contract::public_function<
                override_f>(v, result, &a::f, this, s)
            .precondition([&] {
                out << "a::f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(s.value == "A" || s.value == "X");
            })
            .old([&] {
                out << "a::f::old" << std::endl;
                old_s = BOOST_CONTRACT_OLDOF(v, s_type::eval(s));
            })
            .postcondition([&] (result_type const& result) {
                out << "a::f::post" << std::endl;
                BOOST_CONTRACT_ASSERT(x.value == old_x->value + old_s->value);
                BOOST_CONTRACT_ASSERT(s.value.find(old_x->value) !=
                        std::string::npos);
                BOOST_CONTRACT_ASSERT(result.value == old_s->value);
            })
            .except([&] {
                out << "a::f::except" << std::endl;
                BOOST_CONTRACT_ASSERT(x.value == old_x->value);
                BOOST_CONTRACT_ASSERT(s.value == old_s->value);
            })
        ;

        out << "a::f::body" << std::endl;
        if(s.value == "X") throw except_error();
        std::string save_s = s.value;

        std::string save = x.value;
        x.value += save_s;
        s.value = save;

        save = y.value;
        y.value += save_s;
        s.value += save;

        save = t<'d'>::z.value;
        t<'d'>::z.value += save_s;
        s.value += save;

        save = t<'e'>::z.value;
        t<'e'>::z.value += save_s;
        s.value += save;

        result.value = save_s;
        return result;
    }
    BOOST_CONTRACT_OVERRIDE(f)

    #if defined(BOOST_GCC)
        #pragma GCC diagnostic pop
    #elif defined(BOOST_CLANG)
        #pragma clang diagnostic pop
    #endif
};
    
#endif // #include guard

