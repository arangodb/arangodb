// Boost.TypeErasure library
//
// Copyright 2011 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// $Id$

#include <boost/config.hpp>

#ifdef BOOST_MSVC
#pragma warning(disable:4244) // conversion from double to int
#endif

#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/tuple.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/operators.hpp>
#include <boost/type_erasure/any_cast.hpp>
#include <boost/type_erasure/relaxed.hpp>
#include <boost/mpl/vector.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

using namespace boost::type_erasure;
using boost::core::demangle;

template<class T = _self>
struct common : ::boost::mpl::vector<
    copy_constructible<T>,
    typeid_<T>
> {};

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template<class T = _self>
struct movable_common : ::boost::mpl::vector<
    destructible<T>,
    constructible<T(T&&)>,
    typeid_<T>
> {};

template<class T = _self, class U = T>
struct move_assignable : ::boost::mpl::vector<
    assignable<T, U&&>
> {};

#endif

template<class T, bool CCtor, bool MoveCtor, bool CAssign, bool MoveAssign>
struct test_class
{
    explicit test_class(T n) : value(n) {}
    test_class(const test_class& src) : value(src.value)
    {
        BOOST_STATIC_ASSERT_MSG(CCtor, "Copy constructor not allowed.");
    }
    test_class& operator=(const test_class& src)
    {
        BOOST_STATIC_ASSERT_MSG(CAssign, "Copy assignment not allowed.");
        value = src.value;
        return *this;
    }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    test_class(test_class&& src) : value(src.value)
    {
        BOOST_STATIC_ASSERT_MSG(MoveCtor, "Move constructor not allowed.");
        src.value = 0;
    }
    test_class& operator=(test_class&& src)
    {
        BOOST_STATIC_ASSERT_MSG(MoveAssign, "Move assignment not allowed.");
        value = src.value;
        src.value = 0;
        return *this;
    }
#endif
    bool operator==(T n) const { return value == n; }
    T value;
};

template<class T, bool CCtor, bool MoveCtor, bool CAssign, bool MoveAssign>
std::ostream& operator<<(std::ostream& os, const test_class<T, CCtor, MoveCtor, CAssign, MoveAssign>& t)
{
    return os << t.value;
}

enum copy_info {
    id_lvalue = 1,
    id_const_lvalue = 2,
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    id_rvalue = 3,
    id_maybe_const_lvalue = id_lvalue, // used for the id of construction from a raw value
#else
    id_rvalue = id_const_lvalue,
    id_maybe_const_lvalue = id_const_lvalue,
#endif
    id_construct = 4,
    id_assign = 8,
    id_copy = 16,
    id_int = 32
};

#ifdef BOOST_MSVC
#pragma warning(disable:4521) // multiple copy constructors specified
#pragma warning(disable:4522) // multiple assignment operators specified
#endif

template<class T>
struct copy_tracker
{
    copy_tracker(const copy_tracker& src) : value(src.value), info(id_const_lvalue | id_construct | id_copy), moved_from(false) {}
    copy_tracker(copy_tracker& src) : value(src.value), info(id_lvalue | id_construct | id_copy), moved_from(false) {}
    copy_tracker& operator=(const copy_tracker& src) { value = (src.value); info = (id_const_lvalue | id_assign | id_copy); moved_from = false; return *this; }
    copy_tracker& operator=(copy_tracker& src) { value = (src.value); info = (id_lvalue | id_assign | id_copy); moved_from = false; return *this; }
    copy_tracker(const T& src) : value(src), info(id_const_lvalue | id_construct | id_int), moved_from(false) {}
    copy_tracker(T& src) : value(src), info(id_lvalue | id_construct | id_int), moved_from(false) {}
    copy_tracker& operator=(const T& src) { value = (src); info = (id_const_lvalue | id_assign | id_int); moved_from = false; return *this; }
    copy_tracker& operator=(T& src) { value = (src); info = (id_lvalue | id_assign | id_int); moved_from = false; return *this; }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    copy_tracker(copy_tracker&& src) : value(std::move(src.value)), info(id_rvalue | id_construct | id_copy), moved_from(false) { src.moved_from = true; }
    copy_tracker& operator=(copy_tracker&& src) { value = std::move(src.value); info = (id_rvalue | id_assign | id_copy); moved_from = false; src.moved_from = true; return *this; }
    copy_tracker(T&& src) : value(std::move(src)), info(id_rvalue | id_construct | id_int), moved_from(false) {}
    copy_tracker& operator=(T&& src) { value = std::move(src); info = (id_rvalue | id_assign | id_int); moved_from = false; return *this; }
#endif
    template<class U>
    explicit copy_tracker(const U& src) : value(src), info(id_const_lvalue | id_construct | id_int), moved_from(false) {}
    T value;
    int info;
    bool moved_from;
};

template<class T>
struct value_holder
{
    typedef copy_tracker<T> type;
    typedef copy_tracker<T> unwrapped_type;
    template<class U>
    value_holder(const U& u) : value(u) {}
    type value;
    type get() { return value; }
    const unwrapped_type& unwrap() { return value; }
    template<class U>
    const U& unwrap() { return value; }
};

template<class C, class P>
struct value_holder<any<C, P> >
{
    typedef any<C, P> type;
    typedef any<C, P> unwrapped_type;
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    template<class U>
    value_holder(U&& u) : value(std::forward<U>(u)) {}
#else
    template<class U>
    value_holder(U& u) : value(u) {}
    template<class U>
    value_holder(const U& u) : value(u) {}
#endif
    type value;
    type get() { return value; }
    const unwrapped_type& unwrap() { return value; }
    template<class U>
    const U& unwrap() { return any_cast<const U&>(value); }
};

template<class T>
struct value_holder<T&>
{
    typedef typename value_holder<T>::type& type;
    typedef typename value_holder<T>::unwrapped_type unwrapped_type;
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    template<class U>
    value_holder(U&& u) : impl(std::forward<U>(u)) {}
#else
    template<class U>
    value_holder(U& u) : impl(u) {}
    template<class U>
    value_holder(const U& u) : impl(u) {}
#endif
    value_holder<T> impl;
    type get() { return impl.value; }
    const unwrapped_type& unwrap() { return unwrap<unwrapped_type>(); }
    template<class U>
    const U& unwrap() { return impl.template unwrap<unwrapped_type>(); }
};

template<class T>
struct value_holder<const T&>
{
    typedef const typename value_holder<T>::type& type;
    typedef typename value_holder<T>::unwrapped_type unwrapped_type;
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    template<class U>
    value_holder(U&& u) : impl(std::forward<U>(u)) {}
#else
    template<class U>
    value_holder(U& u) : impl(u) {}
    template<class U>
    value_holder(const U& u) : impl(u) {}
#endif
    value_holder<T> impl;
    type get() { return impl.value; }
    const unwrapped_type& unwrap() { return unwrap<unwrapped_type>(); }
    template<class U>
    const U& unwrap() { return impl.template unwrap<unwrapped_type>(); }
};

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template<class T>
struct value_holder<T&&>
{
    typedef typename value_holder<T>::type&& type;
    typedef typename value_holder<T>::unwrapped_type unwrapped_type;
    template<class U>
    value_holder(U&& u) : impl(std::forward<U>(u)) {}
    value_holder<T> impl;
    type get() { return std::move(impl.value); }
    const unwrapped_type& unwrap() { return unwrap<unwrapped_type>(); }
    template<class U>
    const U& unwrap() { return impl.template unwrap<unwrapped_type>(); }
};

#endif

template<class T, class A>
struct value_holder<T(A)>
{
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    template<class U>
    value_holder(U&& u) : init(std::forward<U>(u)), impl(init.get()) {}
#else
    template<class U>
    value_holder(U& u) : init(u), impl(init.get()) {}
    template<class U>
    value_holder(const U& u) : init(u), impl(init.get()) {}
#endif
    value_holder<A> init;
    value_holder<T> impl;
    typedef typename value_holder<T>::type type;
    typedef typename value_holder<A>::unwrapped_type unwrapped_type;
    type get() { return impl.get(); }
    const unwrapped_type& unwrap() { return unwrap<unwrapped_type>(); }
    template<class U>
    const U& unwrap() { return impl.template unwrap<unwrapped_type>(); }
};

#define TEST_ASSIGNMENT_I(LHS, RHS, result, ptr_op, R)                      \
    try                                                                     \
    {                                                                       \
        value_holder<LHS> lhs(17);                                          \
        value_holder<RHS> rhs(23);                                          \
        const void * original = any_cast<const void*>(&lhs.get());          \
        lhs.get() = rhs.get();                                              \
        typedef value_holder<R>::unwrapped_type expected_type;              \
        if(const expected_type* ptr =                                       \
            any_cast<const expected_type*>(&lhs.get()))                     \
        {                                                                   \
            BOOST_TEST(ptr->value == 23);                                   \
            BOOST_TEST(ptr->info == (result));                              \
            BOOST_TEST(ptr ptr_op original);                                \
        }                                                                   \
        else                                                                \
        {                                                                   \
            BOOST_ERROR("Wrong type: Expected "                             \
                << boost::core::demangle(typeid(expected_type).name())      \
                << " but got "                                              \
                << boost::core::demangle(typeid_of(lhs.get()).name()));     \
        }                                                                   \
    }                                                                       \
    catch(bad_function_call&)                                               \
    {                                                                       \
        BOOST_ERROR("bad_function_call in "                                 \
            << demangle(typeid(value_holder<LHS>::type).name())             \
            << " = "                                                        \
            << demangle(typeid(value_holder<RHS>::type).name()));           \
    }

#define TEST_ASSIGNMENT_id_dispatch(LHS, RHS, result)                       \
    TEST_ASSIGNMENT_I(LHS, RHS, result, ==, LHS)

#define TEST_ASSIGNMENT_id_fallback(LHS, RHS, result)                       \
    TEST_ASSIGNMENT_I(LHS, RHS, result, !=, RHS)

#define TEST_ASSIGNMENT_id_throw(LHS, RHS, result)                          \
    {                                                                       \
        value_holder<LHS> lhs(17);                                          \
        value_holder<RHS> rhs(23);                                          \
        BOOST_CHECK_THROW(lhs.get() = rhs.get(), bad_function_call);        \
    }

#define TEST_ASSIGNMENT(LHS, RHS, tag, result)                              \
    BOOST_PP_CAT(TEST_ASSIGNMENT_, tag)(LHS, RHS, result)

BOOST_AUTO_TEST_CASE(test_nothing)
{
    // Nothing to test.  Assignment is entirely illegal
}

BOOST_AUTO_TEST_CASE(test_nothing_relaxed)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        constructible<_self(int)>,
        relaxed
    > test_concept;
    typedef test_class<int, false, false, false, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;

    // Compile-time check.
    binding<test_concept> test_binding = make_binding< ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > >();
    any_val v1(test_binding, 1);

    // assignment of same type

    // different stored type

    // raw value

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_fallback, id_const_lvalue | id_construct | id_int);

    // reference with a different type
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_int);

    // reference with raw value
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), long&, id_fallback, id_const_lvalue | id_construct | id_int);

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs

    // assignment to an rvalue reference
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(int), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with a different type
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with raw value
    TEST_ASSIGNMENT(any_rref&(int&&), int&&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), long&&, id_fallback, id_const_lvalue | id_construct | id_int);
#endif
}

BOOST_AUTO_TEST_CASE(test_copy_assignable)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        assignable<>,
        constructible<_self(int)>
    > test_concept;
    typedef test_class<int, false, false, true, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    binding<test_concept> test_binding = make_binding< ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > >();
    any_val v1(test_binding, 1);

    // assignment of same type
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    TEST_ASSIGNMENT(any_val&(int), any_val&&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
#endif
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type (undefined behavior)

    // raw value (compile error)

    // assignment to a reference
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
#endif
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // reference with a different type (undefined behavior)

    // reference with raw value (compile error)

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // rvalue reference with a different type (undefined behavior)

    // rvalue reference with raw value (compile error)
#endif
}

BOOST_AUTO_TEST_CASE(test_copy_assignable_relaxed)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        assignable<>,
        constructible<_self(int)>,
        relaxed
    > test_concept;
    typedef test_class<int, false, false, true, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    binding<test_concept> test_binding = make_binding< ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > >();
    any_val v1(test_binding, 1);

    // assignment of same type
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    TEST_ASSIGNMENT(any_val&(int), any_val&&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
#endif
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    TEST_ASSIGNMENT(any_val&(int), any_val&&(long), id_throw, 0);
#endif
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_throw, 0);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_maybe_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_maybe_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment to a reference
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
#endif
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // reference with a different type
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(long), id_throw, 0);
#endif
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(long&), id_throw, 0);

    // reference with raw value
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), long&, id_fallback, id_const_lvalue | id_construct | id_int);

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_throw, );
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(long&&), id_throw, 0);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // rvalue reference with a different type
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with raw value
    TEST_ASSIGNMENT(any_rref&(int&&), int&&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), long&&, id_fallback, id_const_lvalue | id_construct | id_int);
#endif
}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

BOOST_AUTO_TEST_CASE(test_move_assignable)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        move_assignable<>,
        constructible<_self(int)>
    > test_concept;
    typedef test_class<int, false, false, false, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;

    // Compile-time check.
    binding<test_concept> test_binding = make_binding< ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > >();
    any_val v1(test_binding, 1);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);

    // different stored type

    // raw value

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);

    // reference with a different type

    // reference with raw value

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);;

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // rvalue reference with a different type

    // rvalue reference with raw value
}

BOOST_AUTO_TEST_CASE(test_move_assignable_relaxed)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        move_assignable<>,
        constructible<_self(int)>,
        relaxed
    > test_concept;
    typedef test_class<int, false, false, false, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;

    // Compile-time check.
    binding<test_concept> test_binding = make_binding< ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > >();
    any_val v1(test_binding, 1);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val&&(long), id_throw, 0);

    // raw value

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_fallback, id_const_lvalue | id_construct | id_int);

    // reference with a different type
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_int);

    // reference with raw value
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), long&, id_fallback, id_const_lvalue | id_construct | id_int);

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(long&&), id_throw, 0);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // rvalue reference with a different type
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with raw value
    TEST_ASSIGNMENT(any_rref&(int&&), int&&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), long&&, id_fallback, id_const_lvalue | id_construct | id_int);
}

#ifndef BOOST_NO_CXX11_REF_QUALIFIERS

BOOST_AUTO_TEST_CASE(test_move_and_copy_assignable)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        assignable<>,
        move_assignable<>,
        constructible<_self(int)>
    > test_concept;
    typedef test_class<int, false, false, true, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    binding<test_concept> test_binding = make_binding< ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > >();
    any_val v1(test_binding, 1);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type (undefined behavior)

    // raw value (compile error)

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // reference with a different type (undefined behavior)

    // reference with raw value (compile error)

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // rvalue reference with a different type (undefined behavior)

    // rvalue reference with raw value (compile error)
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_assignable_relaxed)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        assignable<>,
        move_assignable<>,
        constructible<_self(int)>,
        relaxed
    > test_concept;
    typedef test_class<int, false, false, true, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    binding<test_concept> test_binding = make_binding< ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > >();
    any_val v1(test_binding, 1);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val&&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_throw, 0);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // reference with a different type
    TEST_ASSIGNMENT(any_ref&(int&), any_val&&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(long&), id_throw, 0);

    // reference with raw value
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), long&, id_fallback, id_const_lvalue | id_construct | id_int);

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(long&&), id_throw, 0);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // rvalue reference with a different type
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with raw value
    TEST_ASSIGNMENT(any_rref&(int&&), int&&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), long&&, id_fallback, id_const_lvalue | id_construct | id_int);
}

#endif // BOOST_NO_CXX11_REF_QUALIFIERS

#endif // BOOST_NO_CXX11_RVALUE_REFERENCES

BOOST_AUTO_TEST_CASE(test_copy_constructible_only)
{
    // Nothing to test.  Assignment is entirely illegal
}

BOOST_AUTO_TEST_CASE(test_copy_constructible_only_relaxed)
{
    typedef ::boost::mpl::vector<
        common<>,
        relaxed
    > test_concept;
    typedef test_class<int, true, false, false, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(int), test_type&, id_fallback, id_maybe_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_maybe_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_maybe_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_fallback, id_const_lvalue | id_construct | id_int);

    // reference with a different type
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_int);

    // reference with raw value
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), long&, id_fallback, id_const_lvalue | id_construct | id_int);

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment to an rvalue reference
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(int), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with a different type
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with raw value
    TEST_ASSIGNMENT(any_rref&(int&&), int&&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), long&&, id_fallback, id_const_lvalue | id_construct | id_int);
#endif
}

BOOST_AUTO_TEST_CASE(test_copy)
{
    typedef ::boost::mpl::vector<
        common<>,
        assignable<>
    > test_concept;
    typedef test_class<int, true, false, true, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(test_type&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(const test_type&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type (undefined behavior)

    // raw value (compile error)

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // reference with a different type (undefined behavior)

    // reference with raw value (compile error)

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // rvalue reference with a different type (undefined behavior)

    // rvalue reference with raw value (compile error)
#endif
}

BOOST_AUTO_TEST_CASE(test_copy_relaxed)
{
    typedef ::boost::mpl::vector<
        common<>,
        assignable<>,
        relaxed
    > test_concept;
    typedef test_class<int, true, false, true, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(int), test_type&, id_fallback, id_maybe_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_maybe_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_maybe_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // reference with a different type
    TEST_ASSIGNMENT(any_ref&(int&), any_val(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(long&), id_throw, 0);

    // reference with raw value
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), long&, id_fallback, id_const_lvalue | id_construct | id_int);

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(long&&), id_throw, 0);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // rvalue reference with a different type
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with raw value
    TEST_ASSIGNMENT(any_rref&(int&&), int&&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), long&&, id_fallback, id_const_lvalue | id_construct | id_int);
#endif
}


#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

BOOST_AUTO_TEST_CASE(test_copy_constructable_move_assignable)
{
    typedef ::boost::mpl::vector<
        common<>,
        move_assignable<>
    > test_concept;
    typedef test_class<int, true, false, false, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(test_type&), any_val(test_type&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(test_type&), any_val(test_type const&), id_dispatch, id_rvalue | id_assign | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);

    // different stored type

    // raw value

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);

    // reference with a different type

    // reference with raw value

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);;

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // rvalue reference with a different type

    // rvalue reference with raw value
}

BOOST_AUTO_TEST_CASE(test_copy_constructable_move_assignable_relaxed)
{
    typedef ::boost::mpl::vector<
        common<>,
        move_assignable<>,
        relaxed
    > test_concept;
    typedef test_class<int, true, false, false, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(int), test_type&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_fallback, id_const_lvalue | id_construct | id_int);

    // reference with a different type
    TEST_ASSIGNMENT(any_ref&(int&), any_val(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_int);

    // reference with raw value
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), long&, id_fallback, id_const_lvalue | id_construct | id_int);

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(long&&), id_throw, 0);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // rvalue reference with a different type
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with raw value
    TEST_ASSIGNMENT(any_rref&(int&&), int&&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), long&&, id_fallback, id_const_lvalue | id_construct | id_int);
}

#ifndef BOOST_NO_CXX11_REF_QUALIFIERS

BOOST_AUTO_TEST_CASE(test_copy_constructible_move_and_copy_assignable)
{
    typedef ::boost::mpl::vector<
        common<>,
        assignable<>,
        move_assignable<>
    > test_concept;
    typedef test_class<int, true, false, true, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(test_type&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(const test_type&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type (undefined behavior)

    // raw value (compile error)

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // reference with a different type (undefined behavior)

    // reference with raw value (compile error)

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // rvalue reference with a different type (undefined behavior)

    // rvalue reference with raw value (compile error)
}

BOOST_AUTO_TEST_CASE(test_copy_constructible_move_and_copy_assignable_relaxed)
{
    typedef ::boost::mpl::vector<
        common<>,
        assignable<>,
        move_assignable<>,
        relaxed
    > test_concept;
    typedef test_class<int, true, false, true, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(int), test_type&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment to a reference
    TEST_ASSIGNMENT(any_ref&(int&), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // reference with a different type
    TEST_ASSIGNMENT(any_ref&(int&), any_val(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_val&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_cref const&(long&), id_throw, 0);

    // reference with raw value
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_ref&(int&), long&, id_fallback, id_const_lvalue | id_construct | id_int);

    typedef any<test_concept, _self&&> any_rref;

    // rvalue reference as the rhs
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref&(long&&), id_throw, 0);
    TEST_ASSIGNMENT(any_ref&(int&), any_rref const&(long&&), id_throw, 0);

    // assignment to an rvalue reference (same dispatching behavior as lvalue reference)
    TEST_ASSIGNMENT(any_rref&(int&&), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // rvalue reference with a different type
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&&(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_val const&(long), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_ref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_cref const&(long&), id_throw, 0);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), any_rref const&(long&&), id_fallback, id_const_lvalue | id_construct | id_int);

    // rvalue reference with raw value
    TEST_ASSIGNMENT(any_rref&(int&&), int&&, id_fallback, id_const_lvalue | id_construct | id_int);
    TEST_ASSIGNMENT(any_rref&(int&&), long&&, id_fallback, id_const_lvalue | id_construct | id_int);
}

#endif // BOOST_NO_CXX11_REF_QUALIFIERS

#endif // BOOST_NO_CXX11_RVALUE_REFERENCES

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_NO_CXX11_REF_QUALIFIERS)

BOOST_AUTO_TEST_CASE(test_move_and_copy_constructible_only)
{
    // Nothing to test.  Assignment is entirely illegal
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_constructible_only_relaxed)
{
    typedef ::boost::mpl::vector<
        common<>,
        movable_common<>,
        relaxed
    > test_concept;
    typedef test_class<int, true, true, false, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;
    typedef any<test_concept, _self&&> any_rref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(int), test_type&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_fallback, id_rvalue | id_construct | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_fallback, id_rvalue | id_construct | id_copy);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_constructible_copy_assignable)
{
    typedef ::boost::mpl::vector<
        common<>,
        movable_common<>,
        assignable<>
    > test_concept;
    typedef test_class<int, true, true, true, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;
    typedef any<test_concept, _self&&> any_rref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(test_type&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(const test_type&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type (undefined behavior)
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_constructible_copy_assignable_relaxed)
{
    typedef ::boost::mpl::vector<
        common<>,
        movable_common<>,
        assignable<>,
        relaxed
    > test_concept;
    typedef test_class<int, true, true, true, false> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;
    typedef any<test_concept, _self&&> any_rref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(int), test_type&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_fallback, id_rvalue | id_construct | id_copy);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_constructable_move_assignable)
{
    typedef ::boost::mpl::vector<
        common<>,
        movable_common<>,
        move_assignable<>
    > test_concept;
    typedef test_class<int, true, true, false, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&&> any_rref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(test_type&), any_val(test_type), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(test_type&), any_val(test_type&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(test_type&), any_val(test_type const&), id_dispatch, id_rvalue | id_assign | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // different stored type

    // raw value
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_constructable_move_assignable_relaxed)
{
    typedef ::boost::mpl::vector<
        common<>,
        movable_common<>,
        move_assignable<>,
        relaxed
    > test_concept;
    typedef test_class<int, true, true, false, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;
    typedef any<test_concept, _self&&> any_rref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(int), test_type, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_fallback, id_rvalue | id_construct | id_copy);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_constructible_move_and_copy_assignable)
{
    typedef ::boost::mpl::vector<
        common<>,
        movable_common<>,
        assignable<>,
        move_assignable<>
    > test_concept;
    typedef test_class<int, true, true, true, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;
    typedef any<test_concept, _self&&> any_rref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(test_type), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(test_type&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(test_type&), any_val&(const test_type&), id_dispatch, id_const_lvalue | id_assign | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // different stored type (undefined behavior)
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_constructible_move_and_copy_assignable_relaxed)
{
    typedef ::boost::mpl::vector<
        common<>,
        movable_common<>,
        assignable<>,
        move_assignable<>,
        relaxed
    > test_concept;
    typedef test_class<int, true, true, true, true> test_type;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;
    typedef any<test_concept, const _self&> any_cref;
    typedef any<test_concept, _self&&> any_rref;

    // Compile-time check.
    TEST_ASSIGNMENT(any_val&(int), test_type, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), test_type const&, id_fallback, id_const_lvalue | id_construct | id_copy);

    // assignment of same type
    TEST_ASSIGNMENT(any_val&(int), any_val(int), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(int), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(int&), id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(int&&), id_dispatch, id_rvalue | id_assign | id_copy);

    // different stored type
    TEST_ASSIGNMENT(any_val&(int), any_val(long), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_val const&(long), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_ref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_cref const&(long&), id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref(long&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref&(long&&), id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), any_rref const&(long&&), id_fallback, id_rvalue | id_construct | id_copy);

    // raw value
    TEST_ASSIGNMENT(any_val&(int), int, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_fallback, id_const_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long, id_fallback, id_rvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long&, id_fallback, id_lvalue | id_construct | id_copy);
    TEST_ASSIGNMENT(any_val&(int), long const&, id_fallback, id_const_lvalue | id_construct | id_copy);
}

#endif // BOOST_NO_RVALUE_REFERENCES || BOOST_NO_CXX11_REF_QUALIFIERS

BOOST_AUTO_TEST_CASE(test_basic_int)
{
    typedef ::boost::mpl::vector<
        common<>,
        assignable<_self, copy_tracker<int> >
    > test_concept;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;

    TEST_ASSIGNMENT(any_val&(int), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    
    TEST_ASSIGNMENT(any_ref&(int&), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    typedef any<test_concept, _self&&> any_rref;
    TEST_ASSIGNMENT(any_rref&(int&&), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);
#endif
}

BOOST_AUTO_TEST_CASE(test_basic_relaxed_int)
{
    typedef ::boost::mpl::vector<
        common<>,
        assignable<_self, copy_tracker<int> >,
        relaxed
    > test_concept;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;

    TEST_ASSIGNMENT(any_val&(int), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    
    TEST_ASSIGNMENT(any_ref&(int&), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    typedef any<test_concept, _self&&> any_rref;
    TEST_ASSIGNMENT(any_rref&(int&&), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);
#endif
}

BOOST_AUTO_TEST_CASE(test_relaxed_no_copy_int)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        assignable<_self, copy_tracker<int> >,
        relaxed
    > test_concept;

    typedef any<test_concept> any_val;
    typedef any<test_concept, _self&> any_ref;

    TEST_ASSIGNMENT(any_val&(int), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_val&(int), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    
    TEST_ASSIGNMENT(any_ref&(int&), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_ref&(int&), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    typedef any<test_concept, _self&&> any_rref;
    TEST_ASSIGNMENT(any_rref&(int&&), int, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), int&, id_dispatch, id_const_lvalue | id_assign | id_copy);
    TEST_ASSIGNMENT(any_rref&(int&&), int const&, id_dispatch, id_const_lvalue | id_assign | id_copy);
#endif
}

BOOST_AUTO_TEST_CASE(test_relaxed_no_assign_int)
{
    typedef ::boost::mpl::vector<
        common<>,
        relaxed
    > test_concept;
    any<test_concept> x(1);
    x = 2;
    BOOST_CHECK_EQUAL(any_cast<int>(x), 2);
}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

struct move_only_assign_int
{
    explicit move_only_assign_int(int i)
        : value(i) {}
    move_only_assign_int(move_only_assign_int&& other)
        : value(other.value) { other.value = 0; }
    move_only_assign_int& operator=(move_only_assign_int&& other)
        { value = other.value; other.value = 0; return *this; }
    move_only_assign_int& operator=(int&& i)
        { value = i; return *this; }
    int value;
private:
    move_only_assign_int(const move_only_assign_int&);
    move_only_assign_int& operator=(const move_only_assign_int&);
};

struct only_assign_int
{
    explicit only_assign_int(int i)
        : value(i) {}
    only_assign_int& operator=(int&& i)
        { value = i; return *this; }
    int value;
private:
    only_assign_int(only_assign_int&&);
    only_assign_int(const only_assign_int&);
    only_assign_int& operator=(only_assign_int&&);
    only_assign_int& operator=(const only_assign_int&);
};

struct move_and_copy_assign_int
{
    explicit move_and_copy_assign_int(int i)
        : value(i) {}
    move_and_copy_assign_int& operator=(move_and_copy_assign_int&& other)
        { value = other.value; other = 0; return *this; }
    move_and_copy_assign_int& operator=(const move_and_copy_assign_int& other)
        { value = other.value; return *this; }
    move_and_copy_assign_int& operator=(int&& i)
        { value = i; return *this; }
    move_and_copy_assign_int& operator=(const int& i)
        { value = i; return *this; }
    int value;
private:
    move_and_copy_assign_int(move_and_copy_assign_int&&);
    move_and_copy_assign_int(const move_and_copy_assign_int&);
};

struct move_and_copy_construct_assign_int
{
    explicit move_and_copy_construct_assign_int(int i)
        : value(i) {}
    move_and_copy_construct_assign_int(move_and_copy_construct_assign_int&& other)
        : value(other.value) { other.value = 0; }
    move_and_copy_construct_assign_int(const move_and_copy_construct_assign_int& other)
        : value(other.value) {}
    move_and_copy_construct_assign_int& operator=(move_and_copy_construct_assign_int&& other)
        { value = other.value; other = 0; return *this; }
    move_and_copy_construct_assign_int& operator=(const move_and_copy_construct_assign_int& other)
        { value = other.value; return *this; }
    move_and_copy_construct_assign_int& operator=(int&& i)
        { value = i; return *this; }
    move_and_copy_construct_assign_int& operator=(const int& i)
        { value = i; return *this; }
    int value;
};

struct copy_only_assign_int
{
    explicit copy_only_assign_int(int i)
        : value(i) {}
    copy_only_assign_int(const copy_only_assign_int& other)
        : value(other.value) {}
    copy_only_assign_int& operator=(const copy_only_assign_int& other)
        { value = other.value; return *this; }
    copy_only_assign_int& operator=(int&& i)
        { value = i; return *this; }
    copy_only_assign_int& operator=(const int& i)
        { value = i; return *this; }
    int value;
private:
    copy_only_assign_int(copy_only_assign_int&&);
    copy_only_assign_int& operator=(copy_only_assign_int&&);
};

struct copy_only_assign_double
{
    explicit copy_only_assign_double(double i)
        : value(i) {}
    copy_only_assign_double(const copy_only_assign_double& other)
        : value(other.value) {}
    copy_only_assign_double& operator=(const copy_only_assign_double& other)
        { value = other.value; return *this; }
    copy_only_assign_double& operator=(double&& i)
        { value = i; return *this; }
    copy_only_assign_double& operator=(const double& i)
        { value = i; return *this; }
    double value;
private:
    copy_only_assign_double(copy_only_assign_double&&);
    copy_only_assign_double& operator=(copy_only_assign_double&&);
};

BOOST_AUTO_TEST_CASE(test_movable_basic_int)
{
    typedef ::boost::mpl::vector<
        movable_common<>,
        move_assignable<_self, int>
    > test_concept;
    typedef move_only_assign_int test_type;
    test_type source_x(1);

    any<test_concept> x(std::move(source_x));
    test_type* ip = any_cast<test_type*>(&x);
    x = std::move(2);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 2);
    // make sure that we're actually using assignment
    // of the underlying object, not copy and swap.
    BOOST_CHECK_EQUAL(any_cast<test_type*>(&x), ip);
}

BOOST_AUTO_TEST_CASE(test_movable_basic_relaxed_int)
{
    typedef ::boost::mpl::vector<
        movable_common<>,
        move_assignable<_self, int>,
        relaxed
    > test_concept;
    typedef move_only_assign_int test_type;
    test_type source_x(1);

    any<test_concept> x(std::move(source_x));
    test_type* ip = any_cast<test_type*>(&x);
    x = std::move(2);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 2);
    // make sure that we're actually using assignment
    // of the underlying object, not copy and swap.
    BOOST_CHECK_EQUAL(any_cast<test_type*>(&x), ip);
}

BOOST_AUTO_TEST_CASE(test_relaxed_no_constructible_int)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        constructible<_self(int)>,
        move_assignable<_self, int>,
        relaxed
    > test_concept;
    typedef only_assign_int test_type;

    typedef ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > type;
    any<test_concept> x(binding<test_concept>(make_binding<type>()), 1);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 1);
    test_type* ip = any_cast<test_type*>(&x);
    x = std::move(2);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 2);
    // make sure that we're actually using assignment
    // of the underlying object, not copy and swap.
    BOOST_CHECK_EQUAL(any_cast<test_type*>(&x), ip);
}

BOOST_AUTO_TEST_CASE(test_movable_relaxed_no_assign_int)
{
    typedef ::boost::mpl::vector<
        movable_common<>,
        relaxed
    > test_concept;
    typedef move_only_assign_int test_type;
    test_type source_x(1);

    any<test_concept> x(std::move(source_x));
    x = test_type(2);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 2);
}

#ifndef BOOST_NO_CXX11_REF_QUALIFIERS

BOOST_AUTO_TEST_CASE(test_move_and_copy_assignable_int)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        constructible<_self(int)>,
        assignable<_self, _self>,
        move_assignable<_self, _self>,
        assignable<_self, int>,
        move_assignable<_self, int>,
        relaxed
    > test_concept;
    typedef move_and_copy_assign_int test_type;
    typedef ::boost::mpl::map< ::boost::mpl::pair<_self, test_type> > type;

    any<test_concept> x(binding<test_concept>(make_binding<type>()), 1);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 1);

    any<test_concept> y(binding<test_concept>(make_binding<type>()), 2);
    x = y;
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 2);

    any<test_concept> z(binding<test_concept>(make_binding<type>()), 3);
    x = std::move(z);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 3);

    x = 4;
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 4);

    x = std::move(5);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 5);
}

BOOST_AUTO_TEST_CASE(test_move_and_copy_assignable_copy_construct_int)
{
    typedef ::boost::mpl::vector<
        common<>,
        assignable<_self, _self>,
        move_assignable<_self, _self>,
        assignable<_self, int>,
        move_assignable<_self, int>,
        relaxed
    > test_concept;
    typedef move_and_copy_construct_assign_int test_type;
    test_type source_x(1);
    test_type source_y(2);
    test_type source_z(3);

    any<test_concept> x(source_x);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 1);
    test_type* ip = any_cast<test_type*>(&x);

    any<test_concept> y(source_y);
    x = y;
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 2);
    // make sure that we're actually using assignment
    // of the underlying object, not copy and swap.
    BOOST_CHECK_EQUAL(any_cast<test_type*>(&x), ip);

    any<test_concept> z(source_z);
    x = std::move(z);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 3);
    // make sure that we're actually using move-assignment
    // of the underlying object, not copy and swap.
    BOOST_CHECK_EQUAL(any_cast<test_type*>(&x), ip);

    x = 4;
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 4);
    // make sure that we're actually using assignment
    // of the underlying object, not copy and swap.
    BOOST_CHECK_EQUAL(any_cast<test_type*>(&x), ip);

    x = std::move(5);
    BOOST_CHECK_EQUAL(any_cast<const test_type&>(x).value, 5);
    // make sure that we're actually using move-assignment
    // of the underlying object, not copy and swap.
    BOOST_CHECK_EQUAL(any_cast<test_type*>(&x), ip);
}

#endif // BOOST_NO_CXX11_REF_QUALIFIERS

BOOST_AUTO_TEST_CASE(test_assignable_with_conversion)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        assignable<_self, int>,
        relaxed
    > test_concept;
    typedef move_and_copy_construct_assign_int test_type1;
    typedef copy_only_assign_int               test_type2;
    typedef copy_only_assign_double            test_type3;

    test_type1 source_x(1);
    any<test_concept> x(source_x);
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 1);

    // assigning raw values is supported because "relaxed" is in concept.
    test_type2 source_y(2);
    x = source_y;
    BOOST_CHECK_EQUAL(any_cast<const test_type2&>(x).value, 2);

    // move-assigning raw values is supported because "relaxed" is in concept.
    x = test_type1(3);
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 3);

    // assigning int is supported because appropriate "assignable" is in concept.
    int int4 = 4;
    x = int4;
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 4);
    int int5 = 5;
    x = std::move(int5);
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 5);

    // assigning double (which is implicitly convertible to int) is supported
    // because appropriate "assignable" is in concept.
    double double6 = 6.5;
    x = double6;  // truncates value
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 6);
    double double7 = 7.5;
    x = std::move(double7);  // truncates value
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 7);

    // assigning raw values is supported because "relaxed" is in concept.
    test_type3 source_z(8.5);
    x = source_z;
    BOOST_CHECK_EQUAL(any_cast<const test_type3&>(x).value, 8.5);

    // assigning double (which is implicitly convertible to int) is supported
    // because appropriate "assignable" is in concept.
    double double9 = 9.5;
    x = double9;  // truncates value
    BOOST_CHECK_EQUAL(any_cast<const test_type3&>(x).value, 9.0);
    double double10 = 10.5;
    x = std::move(double10);  // truncates value
    BOOST_CHECK_EQUAL(any_cast<const test_type3&>(x).value, 10.0);
}

BOOST_AUTO_TEST_CASE(test_move_assignable_with_conversion)
{
    typedef ::boost::mpl::vector<
        destructible<>,
        typeid_<>,
        move_assignable<_self, int>,
        relaxed
    > test_concept;
    typedef move_and_copy_construct_assign_int test_type1;
    typedef copy_only_assign_int               test_type2;
    typedef copy_only_assign_double            test_type3;

    test_type1 source_x(1);
    any<test_concept> x(source_x);
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 1);

    // assigning raw values is supported because "relaxed" is in concept.
    test_type2 source_y(2);
    x = source_y;
    BOOST_CHECK_EQUAL(any_cast<const test_type2&>(x).value, 2);

    // move-assigning raw values is supported because "relaxed" is in concept.
    x = test_type1(3);
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 3);

    // move-assigning int is supported because appropriate "move_assignable" is in concept.
    int int4 = 4;
    x = std::move(int4);
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 4);
    // copy-assigning int is assigning a raw_value.
    int int5 = 5;
    x = int5;
    BOOST_CHECK_EQUAL(any_cast<const int&>(x), 5);
    x = source_x;  // reset.

    // move-assigning double (which is implicitly convertible to int) is supported
    // because appropriate "move_assignable" is in concept.
    double double6 = 6.5;
    x = std::move(double6);  // truncates value
    BOOST_CHECK_EQUAL(any_cast<const test_type1&>(x).value, 6);

    // assigning raw values is supported because "relaxed" is in concept.
    test_type3 source_z(8.5);
    x = source_z;
    BOOST_CHECK_EQUAL(any_cast<const test_type3&>(x).value, 8.5);

    // move-assigning double (which is implicitly convertible to int) is supported
    // because appropriate "move_assignable" is in concept.
    double double9 = 9.5;
    x = std::move(double9);  // truncates value
    BOOST_CHECK_EQUAL(any_cast<const test_type3&>(x).value, 9.0);
}

#endif
