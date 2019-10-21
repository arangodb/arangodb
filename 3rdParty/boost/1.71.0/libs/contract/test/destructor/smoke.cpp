
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test destructor subcontracting.

#include "../detail/oteststream.hpp"
#include "../detail/counter.hpp"
#include <boost/contract/destructor.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/assert.hpp>
#include <boost/contract/old.hpp>
#include <boost/contract/check.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

template<char Id>
struct t {
    static void static_invariant() {
        out << Id << "::static_inv" << std::endl;
        BOOST_CONTRACT_ASSERT(l.value >= 0);
    }
    
    void invariant() const {
        out << Id << "::inv" << std::endl;
        BOOST_CONTRACT_ASSERT(k_ < 0);
    }

    struct l_tag;
    typedef boost::contract::test::detail::counter<l_tag, int> l_type;
    static l_type l;

    explicit t() : k_(-1) { ++l.value; }

    virtual ~t() {
        boost::contract::old_ptr<l_type> old_l;
        boost::contract::check c = boost::contract::destructor(this)
            .old([&] {
                out << Id << "::dtor::old" << std::endl;
                old_l = BOOST_CONTRACT_OLDOF(l_type::eval(l));
            })
            .postcondition([&old_l] {
                out << Id << "::dtor::post" << std::endl;
                BOOST_CONTRACT_ASSERT(t<Id>::l.value == old_l->value - 1);
            })
        ;
        out << Id << "::dtor::body" << std::endl;
        --l.value;
    }

private:
    int k_;
};
template<char Id> typename t<Id>::l_type t<Id>::l;

// Test deep inheritance (2 vertical levels), multiple inheritance (4
// horizontal levels), and that all public/protected/private part of
// subcontracting for destructors (not just public, because all access levels
// are part of C++ object destruction mechanism).
struct c
    #define BASES public t<'d'>, protected t<'p'>, private t<'q'>, public t<'e'>
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() {
        out << "c::static_inv" << std::endl;
        BOOST_CONTRACT_ASSERT(m.value >= 0);
    }
    
    void invariant() const {
        out << "c::inv" << std::endl;
        BOOST_CONTRACT_ASSERT(j_ < 0);
    }
    
    struct m_tag;
    typedef boost::contract::test::detail::counter<m_tag, int> m_type;
    static m_type m;

    explicit c() : j_(-1) { ++m.value; }

    virtual ~c() {
        boost::contract::old_ptr<m_type> old_m =
                BOOST_CONTRACT_OLDOF(m_type::eval(m));
        boost::contract::check c = boost::contract::destructor(this)
            .old([] {
                out << "c::dtor::old" << std::endl;
                // Test old-of assignment above instead.
            })
            .postcondition([&old_m] {
                out << "c::dtor::post" << std::endl;
                BOOST_CONTRACT_ASSERT(c::m.value == old_m->value - 1);
            })
        ;
        out << "c::dtor::body" << std::endl;
        --m.value;
    }

private:
    int j_;
};
c::m_type c::m;

// Test not (fully) contracted base is not part of destructor subcontracting.
struct b {
    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

    explicit b() {}
    virtual ~b() {} // No contract.
};

struct a_n_tag; // Global decl so visible in MSVC10 lambdas.
typedef boost::contract::test::detail::counter<a_n_tag, int> a_n_type;

// Test destructor with both non-contracted and contracted bases.
struct a
    #define BASES public b, public c
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() {
        out << "a::static_inv" << std::endl;
        BOOST_CONTRACT_ASSERT(n.value >= 0);
    }

    void invariant() const {
        out << "a::inv" << std::endl;
        BOOST_CONTRACT_ASSERT(i_ < 0);
    }

    static a_n_type n;

    explicit a() : i_(-1) {
        ++i_; --i_; // To avoid a warning when all contracts off.
        ++n.value;
    }

    virtual ~a() {
        boost::contract::old_ptr<a_n_type> old_n;
        boost::contract::check c = boost::contract::destructor(this)
            .old([&] {
                out << "a::dtor::old" << std::endl;
                old_n = BOOST_CONTRACT_OLDOF(a_n_type::eval(n));
            })
            .postcondition([&old_n] {
                out << "a::dtor::post" << std::endl;
                BOOST_CONTRACT_ASSERT(a::n.value == old_n->value - 1);
            })
        ;
        out << "a::dtor::body" << std::endl;
        --n.value;
    }

private:
    int i_;
};
a_n_type a::n;

int main() {
    std::ostringstream ok;

    {
        a aa;
        out.str("");
    } // Call aa's destructor.
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::dtor::old" << std::endl
        #endif
        << "a::dtor::body" << std::endl
        // Test static inv, but not const inv, checked after destructor body.
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::dtor::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
            << "c::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::dtor::old" << std::endl
        #endif
        << "c::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "c::dtor::post" << std::endl
        #endif
        
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "e::static_inv" << std::endl
            << "e::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "e::dtor::old" << std::endl
        #endif
        << "e::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "e::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "e::dtor::post" << std::endl
        #endif
       
        // Test check also private bases (because part of C++ destruction).
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "q::static_inv" << std::endl
            << "q::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "q::dtor::old" << std::endl
        #endif
        << "q::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "q::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "q::dtor::post" << std::endl
        #endif
        
        // Test check also protected bases (because part of C++ destruction).
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "p::static_inv" << std::endl
            << "p::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "p::dtor::old" << std::endl
        #endif
        << "p::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "p::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "p::dtor::post" << std::endl
        #endif
        
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "d::static_inv" << std::endl
            << "d::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "d::dtor::old" << std::endl
        #endif
        << "d::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "d::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "d::dtor::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    #ifndef BOOST_CONTRACT_NO_OLDS
        #define BOOST_CONTRACT_TEST_old 1u
    #else
        #define BOOST_CONTRACT_TEST_old 0u
    #endif

    // Followings destroy only copies (actual objects are static data members).

    BOOST_TEST_EQ(a_n_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(a_n_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(a_n_type::copies(), a_n_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(c::m_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(c::m_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(c::m_type::copies(), c::m_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(t<'d'>::l_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'d'>::l_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'d'>::l_type::copies(), t<'d'>::l_type::dtors()); // No leak
    
    BOOST_TEST_EQ(t<'p'>::l_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'p'>::l_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'p'>::l_type::copies(), t<'p'>::l_type::dtors()); // No leak
    
    BOOST_TEST_EQ(t<'q'>::l_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'q'>::l_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'q'>::l_type::copies(), t<'q'>::l_type::dtors()); // No leak
    
    BOOST_TEST_EQ(t<'e'>::l_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::l_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::l_type::copies(), t<'e'>::l_type::dtors()); // No leak

    #undef BOOST_CONTRACT_TEST_old
    return boost::report_errors();
}

