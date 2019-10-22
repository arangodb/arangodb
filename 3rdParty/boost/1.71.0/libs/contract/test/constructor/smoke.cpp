
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test constructor subcontracting.

#include "../detail/oteststream.hpp"
#include "../detail/counter.hpp"
#include <boost/contract/constructor.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/assert.hpp>
#include <boost/contract/old.hpp>
#include <boost/contract/check.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

template<char Id>
struct t
    #define BASES private boost::contract::constructor_precondition<t<Id> >
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

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

    struct z_tag;
    typedef boost::contract::test::detail::counter<z_tag, int> z_type;

    explicit t(z_type& z) :
        boost::contract::constructor_precondition<t<Id> >([&] {
            out << Id << "::ctor::pre" << std::endl;
            BOOST_CONTRACT_ASSERT(z.value < 0);
        })
    {
        boost::contract::old_ptr<z_type> old_z;
        boost::contract::old_ptr<l_type> old_l =
                BOOST_CONTRACT_OLDOF(l_type::eval(l));
        boost::contract::check c = boost::contract::constructor(this)
            .old([&] {
                out << Id << "::ctor::old" << std::endl;
                old_z = BOOST_CONTRACT_OLDOF(z_type::eval(z));
            })
            .postcondition([&] {
                out << Id << "::ctor::post" << std::endl;
                BOOST_CONTRACT_ASSERT(k_ == old_z->value);
                BOOST_CONTRACT_ASSERT(z.value == l.value);
                BOOST_CONTRACT_ASSERT(l.value == old_l->value + 1);
            })
        ;
        out << Id << "::ctor::body" << std::endl;
        k_ = z.value;
        z.value = ++l.value;
    }

    virtual ~t() { --l.value; }

private:
    int k_;
};
template<char Id> typename t<Id>::l_type t<Id>::l;

// Test deep inheritance (2 vertical levels), multiple inheritance (4
// horizontal levels), and that all public/protected/private part of
// subcontracting for constructors (not just public, because all access levels
// are part of C++ object construction mechanism).
struct c
    #define BASES private boost::contract::constructor_precondition<c>, \
            public t<'d'>, protected t<'p'>, private t<'q'>, public t<'e'>
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

    struct y_tag;
    typedef boost::contract::test::detail::counter<y_tag, int> y_type;

    explicit c(y_type& y, t<'d'>::z_type& dz, t<'p'>::z_type& pz,
            t<'q'>::z_type& qz, t<'e'>::z_type& ez) :
        boost::contract::constructor_precondition<c>([&] {
            out << "c::ctor::pre" << std::endl;
            BOOST_CONTRACT_ASSERT(y.value < 0);
        }),
        t<'d'>(dz), t<'p'>(pz), t<'q'>(qz), t<'e'>(ez)
    {
        boost::contract::old_ptr<y_type> old_y =
                BOOST_CONTRACT_OLDOF(y_type::eval(y));
        boost::contract::old_ptr<m_type> old_m;
        boost::contract::check c = boost::contract::constructor(this)
            .old([&] {
                out << "c::ctor::old" << std::endl;
                old_m = BOOST_CONTRACT_OLDOF(m_type::eval(m));
            })
            .postcondition([&] {
                out << "c::ctor::post" << std::endl;
                BOOST_CONTRACT_ASSERT(j_ == old_y->value);
                BOOST_CONTRACT_ASSERT(y.value == m.value);
                BOOST_CONTRACT_ASSERT(m.value == old_m->value + 1);
            })
        ;
        out << "c::ctor::body" << std::endl;
        j_ = y.value;
        y.value = ++m.value;
    }

    virtual ~c() { --m.value; }

private:
    int j_;
};
c::m_type c::m;

// Test not (fully) contracted base is not part of constructor subcontracting.
struct b
    #define BASES private boost::contract::constructor_precondition<b>
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

    explicit b() {} // No contract.
    virtual ~b() {}
};

// Test constructor with both non-contracted and contracted bases.
struct a
    #define BASES private boost::contract::constructor_precondition<a>, \
            public b, public c
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
    
    struct n_tag;
    typedef boost::contract::test::detail::counter<n_tag, int> n_type;
    static n_type n;

    struct x_tag;
    typedef boost::contract::test::detail::counter<x_tag, int> x_type;

    explicit a(x_type& x, c::y_type& y, t<'d'>::z_type& dz,
            t<'p'>::z_type& pz, t<'q'>::z_type& qz, t<'e'>::z_type& ez) :
        boost::contract::constructor_precondition<a>([&] {
            out << "a::ctor::pre" << std::endl;
            BOOST_CONTRACT_ASSERT(x.value < 0);
        }),
        b(), c(y, dz, pz, qz, ez)
    {
        boost::contract::old_ptr<x_type> old_x;
        boost::contract::old_ptr<n_type> old_n =
                BOOST_CONTRACT_OLDOF(n_type::eval(n));
        boost::contract::check c = boost::contract::constructor(this)
            .old([&] {
                out << "a::ctor::old" << std::endl;
                old_x = BOOST_CONTRACT_OLDOF(x_type::eval(x));
            })
            .postcondition([&] {
                out << "a::ctor::post" << std::endl;
                BOOST_CONTRACT_ASSERT(i_ == old_x->value);
                BOOST_CONTRACT_ASSERT(x.value == n.value);
                BOOST_CONTRACT_ASSERT(n.value == old_n->value + 1);
            })
        ;
        out << "a::ctor::body" << std::endl;
        i_ = x.value;
        x.value = ++n.value;
    }

    virtual ~a() { --n.value; }

private:
    int i_;
};
a::n_type a::n;

int main() {
    std::ostringstream ok;

    {
        t<'e'>::z_type ez; ez.value = -5;
        t<'q'>::z_type qz; qz.value = -5;
        t<'p'>::z_type pz; pz.value = -4;
        t<'d'>::z_type dz; dz.value = -3;
        c::y_type y; y.value = -2;
        a::x_type x; x.value = -1;

        out.str("");
        a aa(x, y, dz, pz, qz, ez);
        ok.str(""); ok
            // Test all constructor pre checked first.
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "a::ctor::pre" << std::endl
                << "c::ctor::pre" << std::endl
            #endif

            // Test static inv, but not const inv, checked before ctor body.
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "d::ctor::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "d::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "d::ctor::old" << std::endl
            #endif
            << "d::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "d::static_inv" << std::endl
                << "d::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "d::ctor::post" << std::endl
            #endif
            
            // Test check also protected bases (because part of C++ constr.).
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "p::ctor::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "p::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "p::ctor::old" << std::endl
            #endif
            << "p::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "p::static_inv" << std::endl
                << "p::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "p::ctor::post" << std::endl
            #endif
            
            // Test check also private bases (because part of C++ constr.).
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "q::ctor::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "q::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "q::ctor::old" << std::endl
            #endif
            << "q::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "q::static_inv" << std::endl
                << "q::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "q::ctor::post" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "e::ctor::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "e::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "e::ctor::old" << std::endl
            #endif
            << "e::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "e::static_inv" << std::endl
                << "e::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "e::ctor::post" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "c::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "c::ctor::old" << std::endl
            #endif
            << "c::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "c::static_inv" << std::endl
                << "c::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "c::ctor::post" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "a::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::ctor::old" << std::endl
            #endif
            << "a::ctor::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::ctor::post" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    }

    #ifndef BOOST_CONTRACT_NO_OLDS
        #define BOOST_CONTRACT_TEST_old 1u
    #else
        #define BOOST_CONTRACT_TEST_old 0u
    #endif

    std::clog << a::x_type::copies() << std::endl;
    std::clog << BOOST_CONTRACT_TEST_old << std::endl;
    BOOST_TEST_EQ(a::x_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(a::x_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(a::x_type::ctors(), a::x_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(c::y_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(c::y_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(c::y_type::ctors(), c::y_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(t<'d'>::z_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'d'>::z_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'d'>::z_type::ctors(), t<'d'>::z_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(t<'p'>::z_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'p'>::z_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'p'>::z_type::ctors(), t<'p'>::z_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(t<'q'>::z_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'q'>::z_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'q'>::z_type::ctors(), t<'q'>::z_type::dtors()); // No leak.

    BOOST_TEST_EQ(t<'e'>::z_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::z_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::z_type::ctors(), t<'e'>::z_type::dtors()); // No leak.

    // Following destroy only copies (actual objects are static data members).

    BOOST_TEST_EQ(a::n_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(a::n_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(a::n_type::copies(), a::n_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(c::m_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(c::m_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(c::m_type::copies(), c::m_type::dtors()); // No leak.
    
    BOOST_TEST_EQ(t<'d'>::l_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'d'>::l_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::l_type::copies(), t<'e'>::l_type::dtors()); // No leak
    
    BOOST_TEST_EQ(t<'p'>::l_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'p'>::l_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::l_type::copies(), t<'e'>::l_type::dtors()); // No leak
    
    BOOST_TEST_EQ(t<'q'>::l_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'q'>::l_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::l_type::copies(), t<'e'>::l_type::dtors()); // No leak
    
    BOOST_TEST_EQ(t<'e'>::l_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::l_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(t<'e'>::l_type::copies(), t<'e'>::l_type::dtors()); // No leak

    #undef BOOST_CONTRACT_TEST_old
    return boost::report_errors();
}

