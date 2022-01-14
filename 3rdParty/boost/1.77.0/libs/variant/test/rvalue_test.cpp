//-----------------------------------------------------------------------------
// boost-libs variant/test/rvalue_test.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2012-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/config.hpp"

#include "boost/core/lightweight_test.hpp"
#include "boost/variant.hpp"
#include "boost/type_traits/is_nothrow_move_assignable.hpp"
#include "boost/mpl/bool.hpp"

#include <boost/blank.hpp>
#include <boost/swap.hpp>

namespace swap_ambiguouty_test_ns {
    struct A {};
    struct B {};

    void swap_ambiguouty_test() {
        // If boost::blank is not used, then it compiles.
        typedef boost::variant<boost::blank, A, B> Variant;
        Variant v1, v2;
        swap(v1, v2);
    }
} // namespace swap_ambiguouty_test_ns

// Most part of tests from this file require rvalue references support

class move_copy_conting_class {
public:
    static unsigned int moves_count;
    static unsigned int copy_count;

    move_copy_conting_class(){}
    move_copy_conting_class(BOOST_RV_REF(move_copy_conting_class) ) {
        ++ moves_count;
    }

    move_copy_conting_class& operator=(BOOST_RV_REF(move_copy_conting_class) ) {
        ++ moves_count;
        return *this;
    }

    move_copy_conting_class(const move_copy_conting_class&) {
        ++ copy_count;
    }
    move_copy_conting_class& operator=(BOOST_COPY_ASSIGN_REF(move_copy_conting_class) ) {
        ++ copy_count;
        return *this;
    }
};

unsigned int move_copy_conting_class::moves_count = 0;
unsigned int move_copy_conting_class::copy_count = 0;

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES

void run()
{
    // Making sure that internals of Boost.Move do not interfere with
    // internals of Boost.Variant and in case of C++03 or C++98 compilation 
    // is still possible.
    typedef boost::variant<int, move_copy_conting_class> variant_I_type;
    variant_I_type v1, v2;
    v1 = move_copy_conting_class();
    v2 = v1; 
    v2 = boost::move(v1);
    v1.swap(v2);

    move_copy_conting_class val;
    v2 = boost::move(val);
    v2 = 10;

    variant_I_type v3(boost::move(val));
    variant_I_type v4(boost::move(v1));
}

void run1()
{
    BOOST_TEST(true);
}

void run_move_only()
{
    BOOST_TEST(true);
}

void run_moves_are_noexcept()
{
    BOOST_TEST(true);
}


void run_const_rvalues()
{
    BOOST_TEST(true);
}


#else 


void run()
{
    typedef boost::variant<int, move_copy_conting_class> variant_I_type;
    variant_I_type v1, v2;
    
    // Assuring that `move_copy_conting_class` was not created
    BOOST_TEST(move_copy_conting_class::copy_count == 0);
    BOOST_TEST(move_copy_conting_class::moves_count == 0);
    
    v1 = move_copy_conting_class();
    // Assuring that `move_copy_conting_class` was moved at least once
    BOOST_TEST(move_copy_conting_class::moves_count != 0);
    
    unsigned int total_count = move_copy_conting_class::moves_count + move_copy_conting_class::copy_count;
    move_copy_conting_class var;
    v1 = 0;
    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;
    v1 = var;
    // Assuring that move assignment operator moves/copyes value not more times than copy assignment operator
    BOOST_TEST(total_count <= move_copy_conting_class::moves_count + move_copy_conting_class::copy_count);

    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;
    v2 = boost::move(v1);
    // Assuring that `move_copy_conting_class` in v1 was moved at least once and was not copied
    BOOST_TEST(move_copy_conting_class::moves_count != 0);
    BOOST_TEST(move_copy_conting_class::copy_count == 0);

    v1 = move_copy_conting_class();
    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;
    v2 = boost::move(v1);
    // Assuring that `move_copy_conting_class` in v1 was moved at least once and was not copied
    BOOST_TEST(move_copy_conting_class::moves_count != 0);
    BOOST_TEST(move_copy_conting_class::copy_count == 0);
    total_count = move_copy_conting_class::moves_count + move_copy_conting_class::copy_count;
    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;
    v1 = v2;
    // Assuring that move assignment operator moves/copyes value not more times than copy assignment operator
    BOOST_TEST(total_count <= move_copy_conting_class::moves_count + move_copy_conting_class::copy_count);


    typedef boost::variant<move_copy_conting_class, int> variant_II_type;
    variant_II_type v3;
    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;
    v1 = boost::move(v3);
    // Assuring that `move_copy_conting_class` in v3 was moved at least once (v1 and v3 have different types)
    BOOST_TEST(move_copy_conting_class::moves_count != 0);

    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;
    v2 = boost::move(v1);
    // Assuring that `move_copy_conting_class` in v1 was moved at least once (v1 and v3 have different types)
    BOOST_TEST(move_copy_conting_class::moves_count != 0);

    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;
    variant_I_type v5(boost::move(v1));
    // Assuring that `move_copy_conting_class` in v1 was moved at least once and was not copied
    BOOST_TEST(move_copy_conting_class::moves_count != 0);
    BOOST_TEST(move_copy_conting_class::copy_count == 0);

    total_count = move_copy_conting_class::moves_count + move_copy_conting_class::copy_count;
    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;
    variant_I_type v6(v1);
    // Assuring that move constructor moves/copyes value not more times than copy constructor
    BOOST_TEST(total_count <= move_copy_conting_class::moves_count + move_copy_conting_class::copy_count);
}

void run1()
{
    move_copy_conting_class::moves_count = 0;
    move_copy_conting_class::copy_count = 0;

    move_copy_conting_class c1;
    typedef boost::variant<int, move_copy_conting_class> variant_I_type;
    variant_I_type v1(boost::move(c1));
    
    // Assuring that `move_copy_conting_class` was not copyied
    BOOST_TEST(move_copy_conting_class::copy_count == 0);
    BOOST_TEST(move_copy_conting_class::moves_count > 0);
}

struct move_only_structure {
    move_only_structure(){}
    move_only_structure(move_only_structure&&){}
    move_only_structure& operator=(move_only_structure&&) { return *this; }

private:
    move_only_structure(const move_only_structure&);
    move_only_structure& operator=(const move_only_structure&);
};

struct visitor_returning_move_only_type: boost::static_visitor<move_only_structure> {
    template <class T>
    move_only_structure operator()(const T&) const {
        return move_only_structure();
    }
};

void run_move_only()
{
    move_only_structure mo;
    boost::variant<int, move_only_structure > vi, vi2(static_cast<move_only_structure&&>(mo));
    BOOST_TEST(vi.which() == 0);
    BOOST_TEST(vi2.which() == 1);

    vi = 10;
    vi2 = 10;
    BOOST_TEST(vi.which() == 0);
    BOOST_TEST(vi2.which() == 0);

    vi = static_cast<move_only_structure&&>(mo);
    vi2 = static_cast<move_only_structure&&>(mo);
    BOOST_TEST(vi.which() == 1);

    boost::variant<move_only_structure, int > rvi (1);
    BOOST_TEST(rvi.which() == 1);
    rvi = static_cast<move_only_structure&&>(mo);
    BOOST_TEST(rvi.which() == 0);
    rvi = 1;
    BOOST_TEST(rvi.which() == 1);
    rvi = static_cast<boost::variant<int, move_only_structure >&&>(vi2);
    BOOST_TEST(rvi.which() == 0);

    move_only_structure from_visitor = boost::apply_visitor(visitor_returning_move_only_type(), vi);
    (void)from_visitor;
}

void run_moves_are_noexcept() {
#if !defined(BOOST_NO_CXX11_NOEXCEPT) && (!defined(__GNUC__) || defined(__clang__) || __GNUC__ > 4 || __GNUC_MINOR__ >= 8)
    typedef boost::variant<int, short, double> variant_noexcept_t;
    BOOST_TEST(boost::is_nothrow_move_assignable<variant_noexcept_t>::value);
    BOOST_TEST(boost::is_nothrow_move_constructible<variant_noexcept_t>::value);

    typedef boost::variant<int, short, double, move_only_structure> variant_except_t;
    BOOST_TEST(!boost::is_nothrow_move_assignable<variant_except_t>::value);
    BOOST_TEST(!boost::is_nothrow_move_constructible<variant_except_t>::value);
#endif
}

inline const std::string get_string() { return "test"; } 
inline const boost::variant<int, std::string> get_variant() { return std::string("test"); } 
inline const boost::variant<std::string, int> get_variant2() { return std::string("test"); } 

void run_const_rvalues()
{
    typedef boost::variant<int, std::string> variant_t;
    const variant_t v1(get_string());
    const variant_t v2(get_variant());
    const variant_t v3(get_variant2());
    
    variant_t v4, v5, v6, v7;
    v4 = get_string();
    v5 = get_variant();
    v6 = get_variant2();
    v7 = boost::move(v1);
}

#endif

struct nothrow_copyable_throw_movable {
    nothrow_copyable_throw_movable(){}
    nothrow_copyable_throw_movable(const nothrow_copyable_throw_movable&) BOOST_NOEXCEPT {}
    nothrow_copyable_throw_movable& operator=(const nothrow_copyable_throw_movable&) BOOST_NOEXCEPT { return *this; }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    nothrow_copyable_throw_movable(nothrow_copyable_throw_movable&&) BOOST_NOEXCEPT_IF(false) {}
    nothrow_copyable_throw_movable& operator=(nothrow_copyable_throw_movable&&) BOOST_NOEXCEPT_IF(false) { return *this; }
#endif
};

// This test is created to cover the following situation:
// https://svn.boost.org/trac/boost/ticket/8772
void run_tricky_compilation_test()
{
    boost::variant<int, nothrow_copyable_throw_movable> v;
    v = nothrow_copyable_throw_movable();
}

template <typename T>
struct is_container : boost::mpl::false_ {};

template <typename T>
struct is_container<boost::variant<T> > : is_container<T> {};

template <BOOST_VARIANT_ENUM_PARAMS(typename T)>
struct is_container<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> >
  : boost::mpl::bool_<is_container<T0>::value
     || is_container<boost::variant<BOOST_VARIANT_ENUM_SHIFTED_PARAMS(T)> >::value>
{};

void run_is_container_compilation_test()
{
    BOOST_TEST((!is_container<boost::variant<double, int> >::value));
    BOOST_TEST((!is_container<boost::variant<double, int, char> >::value));
    BOOST_TEST((!is_container<boost::variant<double, int, char, float> >::value));
}

int main()
{
   swap_ambiguouty_test_ns::swap_ambiguouty_test();
   run();
   run1();
   run_move_only();
   run_moves_are_noexcept();
   run_tricky_compilation_test();
   run_const_rvalues();
   run_is_container_compilation_test();

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__ > 6)
#   ifdef BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES
        BOOST_TEST(false && 
            "Something wrong with macro definitions. GCC-4.7+ is known to work with variadic templates"
        );
#   endif
#endif

   return boost::report_errors();
}
