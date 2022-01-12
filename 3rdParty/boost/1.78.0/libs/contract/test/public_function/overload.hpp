
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test public function overloads.

#include "../detail/oteststream.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>

boost::contract::test::detail::oteststream out;

struct b {
    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

    virtual void f(int /* x */, boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "b::f(int)::pre" << std::endl; })
            .old([] { out << "b::f(int)::old" << std::endl; })
            .postcondition([] { out << "b::f(int)::post" << std::endl; })
        ;
        out << "b::f(int)::body" << std::endl;
    }
    
    virtual void f(char const* /* x */, boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "b::f(char const*)::pre" << std::endl; })
            .old([] { out << "b::f(char const*)::old" << std::endl; })
            .postcondition(
                    [] { out << "b::f(char const*)::post" << std::endl; })
        ;
        out << "b::f(char const*)::body" << std::endl;
    }

    virtual void f(int /* x */, int /* y */, boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "b::f(int, int)::pre" << std::endl; })
            .old([] { out << "b::f(int, int)::old" << std::endl; })
            .postcondition([] { out << "b::f(int, int)::post" << std::endl; })
        ;
        out << "b::f(int, int)::body" << std::endl;
    }
    
    virtual void f(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "b::f()::pre" << std::endl; })
            .old([] { out << "b::f()::old" << std::endl; })
            .postcondition([] { out << "b::f()::post" << std::endl; })
        ;
        out << "b::f()::body" << std::endl;
    }
    
    void f(int /* x */[2][3], boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "b::f(int[2][3])::pre" << std::endl; })
            .old([] { out << "b::f(int[2][3])::old" << std::endl; })
            .postcondition([] { out << "b::f(int[2][3])::post" << std::endl; })
        ;
        out << "b::f(int[2][3])::body" << std::endl;
    }
    
    void f(void (* /* x */)(int), boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition(
                    [] { out << "b::f(void (*)(int))::pre" << std::endl; })
            .old(
                    [] { out << "b::f(void (*)(int))::old" << std::endl; })
            .postcondition(
                    [] { out << "b::f(void (*)(int))::post" << std::endl; })
        ;
        out << "b::f(void (*)(int))::body" << std::endl;
    }
};

struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() { out << "a::static_inv" << std::endl; }
    void invariant() const { out << "a::inv" << std::endl; }

    void f(int x, boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
            v,
            static_cast<void (a::*)(int, boost::contract::virtual_*)>(&a::f),
            this, x
        )
            .precondition([] { out << "a::f(int)::pre" << std::endl; })
            .old([] { out << "a::f(int)::old" << std::endl; })
            .postcondition([] { out << "a::f(int)::post" << std::endl; })
        ;
        out << "a::f(int)::body" << std::endl;
    }
    
    // Test overload via argument type.
    void f(char const* x, boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
            v,
            static_cast<void (a::*)(char const*, boost::contract::virtual_*)>(
                    &a::f),
            this, x
        )
            .precondition([] { out << "a::f(char const*)::pre" << std::endl; })
            .old([] { out << "a::f(char const*)::old" << std::endl; })
            .postcondition(
                    [] { out << "a::f(char const*)::post" << std::endl; })
        ;
        out << "a::f(char const*)::body" << std::endl;
    }
    
    // Test overload via argument count.
    void f(int x, int y, boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
            v,
            static_cast<void (a::*)(int, int, boost::contract::virtual_*)>(
                    &a::f),
            this, x, y
        )
            .precondition([] { out << "a::f(int, int)::pre" << std::endl; })
            .old([] { out << "a::f(int, int)::old" << std::endl; })
            .postcondition([] { out << "a::f(int, int)::post" << std::endl; })
        ;
        out << "a::f(int, int)::body" << std::endl;
    }

    // Test overload via template argument type.
    template<typename T>
    void f(T /* x */) { // Template cannot be virtual (or override) in C++.
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([] { out << "a::f(T)::pre" << std::endl; })
            .old([] { out << "a::f(T)::old" << std::endl; })
            .postcondition([] { out << "a::f(T)::post" << std::endl; })
        ;
        out << "a::f(T)::body" << std::endl;
    }

    // Test no overload ambiguity in public_function called by these two cases.

    // NOTE: In *all* other cases, public_function is always called with a
    // different number of arguments so there cannot be ambiguity either
    // (0 args for static, 1 arg for non-virtual, 2 or 3 args for virtual,
    // >= 3 for override, so only in cases below of 3 args for virtual and 3
    // for override there could be ambiguity but there is not because of
    // presence or absence of override_... template parameter).
    
    typedef void (a::* f0_ptr)(boost::contract::virtual_*);

    void f(boost::contract::virtual_* v = 0) /* override */ {
        f0_ptr f0 = static_cast<f0_ptr>(&a::f);
        // Test this and public_function call in func below both take same 3
        // args but they are ambiguous because of presence override_f.
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, f0, this)
            .precondition([] { out << "a::f()::pre" << std::endl; })
            .old([] { out << "a::f()::old" << std::endl; })
            .postcondition([] { out << "a::f()::post" << std::endl; })
        ;
        out << "a::f()::body" << std::endl;
    }

    virtual f0_ptr f(bool /* x */, boost::contract::virtual_* v = 0)
            /* not an override */ {
        f0_ptr f0 = static_cast<f0_ptr>(&a::f);
        // Test this and public_function call in func above both take same 3
        // args but they are ambiguous because of lack of override_f.
        boost::contract::check c = boost::contract::public_function(
                v, f0, this)
            .precondition([] { out << "a::f(bool)::pre" << std::endl; })
            .old([] { out << "a::f(bool)::old" << std::endl; })
            .postcondition([] (f0_ptr const&) {
                    out << "a::f(bool)::post" << std::endl; })
        ;
        out << "a::f(bool)::body" << std::endl;
        return f0;
    }
    
    // Test overload with array parameter.
    void f(int x[2][3], boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
            v,
            static_cast<void (a::*)(int[2][3], boost::contract::virtual_*)>(
                    &a::f),
            this, x
        )
            .precondition([] { out << "a::f(int[2][3])::pre" << std::endl; })
            .old([] { out << "a::f(int[2][3])::old" << std::endl; })
            .postcondition([] { out << "a::f(int[2][3])::post" << std::endl; })
        ;
        out << "a::f(int[2][3])::body" << std::endl;
    }
    
    // Test overload with function pointer parameter.
    void f(void (*x)(int), boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
            v,
            static_cast<void (a::*)(void (*)(int), boost::contract::virtual_*)>(
                    &a::f),
            this, x
        )
            .precondition(
                    [] { out << "a::f(void (*)(int))::pre" << std::endl; })
            .old(
                    [] { out << "a::f(void (*)(int))::old" << std::endl; })
            .postcondition(
                    [] { out << "a::f(void (*)(int))::post" << std::endl; })
        ;
        out << "a::f(void (*)(int))::body" << std::endl;
    }
    
    BOOST_CONTRACT_OVERRIDE(f)
};

void g(int) {}
        
std::string ok_args(std::string const& args) {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::f(" << args << ")::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::f(" << args << ")::old" << std::endl
            << "a::f(" << args << ")::old" << std::endl
        #endif
        << "a::f(" << args << ")::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::f(" << args << ")::old" << std::endl
            << "b::f(" << args << ")::post" << std::endl
            << "a::f(" << args << ")::post" << std::endl
        #endif
    ;
    return ok.str();
}

int main() {
    std::ostringstream ok;
    a aa;

    out.str("");
    aa.f(123);
    ok.str(""); ok << ok_args("int");
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    aa.f("abc");
    ok.str(""); ok << ok_args("char const*");
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    aa.f(123, 456);
    ok.str(""); ok << ok_args("int, int");
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    struct {} zz;
    aa.f(zz); // Call template (so no override because no virtual).
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::f(T)::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::f(T)::old" << std::endl
        #endif
        << "a::f(T)::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::f(T)::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    aa.f();
    ok.str(""); ok << ok_args("");
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    aa.f(true); // This does not override (public_function ambiguity testing).
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::f(bool)::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::f(bool)::old" << std::endl
        #endif
        << "a::f(bool)::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::f(bool)::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    int i[2][3];
    aa.f(i);
    ok.str(""); ok << ok_args("int[2][3]");
    BOOST_TEST(out.eq(ok.str()));
    
    out.str("");
    aa.f(&g);
    ok.str(""); ok << ok_args("void (*)(int)");
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

