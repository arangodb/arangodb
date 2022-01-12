//-----------------------------------------------------------------------------
// boost-libs variant/test/variant_get_test.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2014-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4181) // qualifier applied to reference type; ignored
#endif

#include "boost/variant/get.hpp"
#include "boost/variant/variant.hpp"
#include "boost/variant/polymorphic_get.hpp"
#include "boost/variant/recursive_wrapper.hpp"
#include "boost/core/lightweight_test.hpp"

struct base {
    int trash;

    base() : trash(123) {}
    base(const base& b) : trash(b.trash) { int i = 100; (void)i; }
    const base& operator=(const base& b) {
        trash = b.trash;
        int i = 100; (void)i;

        return *this;
    }

    virtual ~base(){}
};

struct derived1 : base{};
struct derived2 : base{};

struct vbase { short trash; virtual ~vbase(){} virtual int foo() const { return 0; }  };
struct vderived1 : virtual vbase{ virtual int foo() const { return 1; } };
struct vderived2 : virtual vbase{ virtual int foo() const { return 3; } };
struct vderived3 : vderived1, vderived2 { virtual int foo() const { return 3; } };

typedef boost::variant<int, base, derived1, derived2, std::string> var_t;
typedef boost::variant<int, derived1, derived2, std::string> var_t_shortened;
typedef boost::variant<base, derived1, derived2> var_t_no_fallback;
typedef boost::variant<int&, base&, derived1&, derived2&, std::string&> var_ref_t;
typedef boost::variant<const int&, const base&, const derived1&, const derived2&, const std::string&> var_cref_t;

struct recursive_structure;
typedef boost::variant<
    int, base, derived1, derived2, std::string, boost::recursive_wrapper<recursive_structure>
> var_req_t;
struct recursive_structure { var_req_t var; };

template <class TypeInVariant, class V, class TestType>
inline void check_polymorphic_get_on_types_impl_single_type(V* v)
{
    typedef typename boost::add_reference<TestType>::type ref_test_t;
    typedef typename boost::add_reference<const TestType>::type cref_test_t;
    const bool exact_same = !!boost::is_same<TypeInVariant, TestType>::value;
    const bool ref_same = !!boost::is_same<TypeInVariant, ref_test_t>::value;

    if (exact_same || ref_same) {
        BOOST_TEST(boost::polymorphic_get<TestType>(v));
        BOOST_TEST(boost::polymorphic_get<const TestType>(v));
        BOOST_TEST(boost::polymorphic_strict_get<TestType>(v));
        BOOST_TEST(boost::polymorphic_strict_get<const TestType>(v));
        BOOST_TEST(boost::polymorphic_relaxed_get<TestType>(v));
        BOOST_TEST(boost::polymorphic_relaxed_get<const TestType>(v));

        BOOST_TEST(boost::polymorphic_get<cref_test_t>(v));
        BOOST_TEST(boost::polymorphic_strict_get<cref_test_t>(v));
        BOOST_TEST(boost::polymorphic_relaxed_get<cref_test_t>(v));

        if (ref_same) {
            BOOST_TEST(boost::polymorphic_get<ref_test_t>(v));
            BOOST_TEST(boost::polymorphic_get<cref_test_t>(v));
            BOOST_TEST(boost::polymorphic_strict_get<ref_test_t>(v));
            BOOST_TEST(boost::polymorphic_strict_get<cref_test_t>(v));
            BOOST_TEST(boost::polymorphic_relaxed_get<ref_test_t>(v));
            BOOST_TEST(boost::polymorphic_relaxed_get<cref_test_t>(v));
        }
    } else {
        BOOST_TEST(!boost::polymorphic_get<TestType>(v));
        BOOST_TEST(!boost::polymorphic_get<const TestType>(v));
        BOOST_TEST(!boost::polymorphic_strict_get<TestType>(v));
        BOOST_TEST(!boost::polymorphic_strict_get<const TestType>(v));
        BOOST_TEST(!boost::polymorphic_relaxed_get<TestType>(v));
        BOOST_TEST(!boost::polymorphic_relaxed_get<const TestType>(v));
    }
}

template <class T, class V, class TestType>
inline void check_get_on_types_impl_single_type(V* v)
{
    typedef typename boost::add_reference<TestType>::type ref_test_t;
    typedef typename boost::add_reference<const TestType>::type cref_test_t;
    const bool exact_same = !!boost::is_same<T, TestType>::value;
    const bool ref_same = !!boost::is_same<T, ref_test_t>::value;

    if (exact_same || ref_same) {
        BOOST_TEST(boost::get<TestType>(v));
        BOOST_TEST(boost::get<const TestType>(v));
        BOOST_TEST(boost::strict_get<TestType>(v));
        BOOST_TEST(boost::strict_get<const TestType>(v));
        BOOST_TEST(boost::relaxed_get<TestType>(v));
        BOOST_TEST(boost::relaxed_get<const TestType>(v));

        BOOST_TEST(boost::get<cref_test_t>(v));
        BOOST_TEST(boost::strict_get<cref_test_t>(v));
        BOOST_TEST(boost::relaxed_get<cref_test_t>(v));

        if (ref_same) {
            BOOST_TEST(boost::get<ref_test_t>(v));
            BOOST_TEST(boost::get<cref_test_t>(v));
            BOOST_TEST(boost::strict_get<ref_test_t>(v));
            BOOST_TEST(boost::strict_get<cref_test_t>(v));
            BOOST_TEST(boost::relaxed_get<ref_test_t>(v));
            BOOST_TEST(boost::relaxed_get<cref_test_t>(v));
        }
    } else {
        BOOST_TEST(!boost::get<TestType>(v));
        BOOST_TEST(!boost::get<const TestType>(v));
        BOOST_TEST(!boost::strict_get<TestType>(v));
        BOOST_TEST(!boost::strict_get<const TestType>(v));
        BOOST_TEST(!boost::relaxed_get<TestType>(v));
        BOOST_TEST(!boost::relaxed_get<const TestType>(v));
    }
}

template <class T, class V>
inline void check_get_on_types_impl(V* v)
{
    check_get_on_types_impl_single_type<T, V, int>(v);
    check_polymorphic_get_on_types_impl_single_type<T, V, int>(v);

    check_get_on_types_impl_single_type<T, V, base>(v);

    check_get_on_types_impl_single_type<T, V, derived1>(v);
    check_polymorphic_get_on_types_impl_single_type<T, V, derived1>(v);

    check_get_on_types_impl_single_type<T, V, derived2>(v);
    check_polymorphic_get_on_types_impl_single_type<T, V, derived2>(v);

    check_get_on_types_impl_single_type<T, V, std::string>(v);
    check_polymorphic_get_on_types_impl_single_type<T, V, std::string>(v);

    // Never exist in here
    BOOST_TEST(!boost::relaxed_get<short>(v));
    BOOST_TEST(!boost::relaxed_get<const short>(v));
    BOOST_TEST(!boost::relaxed_get<char>(v));
    BOOST_TEST(!boost::relaxed_get<char*>(v));
    BOOST_TEST(!boost::relaxed_get<bool>(v));
    BOOST_TEST(!boost::relaxed_get<const bool>(v));

    BOOST_TEST(!boost::polymorphic_relaxed_get<short>(v));
    BOOST_TEST(!boost::polymorphic_relaxed_get<const short>(v));
    BOOST_TEST(!boost::polymorphic_relaxed_get<char>(v));
    BOOST_TEST(!boost::polymorphic_relaxed_get<char*>(v));
    BOOST_TEST(!boost::polymorphic_relaxed_get<bool>(v));
    BOOST_TEST(!boost::polymorphic_relaxed_get<const bool>(v));

    boost::get<T>(*v);              // Must compile
    boost::get<const T>(*v);        // Must compile
    boost::strict_get<T>(*v);         // Must compile
    boost::strict_get<const T>(*v);   // Must compile

    bool is_ref = boost::is_lvalue_reference<T>::value;
    (void)is_ref;
    if (!is_ref) {
        boost::polymorphic_get<T>(*v);              // Must compile
        boost::polymorphic_get<const T>(*v);        // Must compile
        boost::polymorphic_strict_get<T>(*v);         // Must compile
        boost::polymorphic_strict_get<const T>(*v);   // Must compile
    }
}

template <class T, class V>
inline void check_get_on_types(V* v)
{
    check_get_on_types_impl<T, V>(v);
    check_get_on_types_impl<T, const V>(v);
}

inline void get_test()
{
    var_t v;
    check_get_on_types<int>(&v);

    var_t(base()).swap(v);
    check_get_on_types<base>(&v);

    var_t(derived1()).swap(v);
    check_get_on_types<derived1>(&v);

    var_t(derived2()).swap(v);
    check_get_on_types<derived2>(&v);

    var_t(std::string("Hello")).swap(v);
    check_get_on_types<std::string>(&v);

    var_t_shortened vs = derived2();
    check_polymorphic_get_on_types_impl_single_type<derived2, var_t_shortened, int>(&vs);
    check_polymorphic_get_on_types_impl_single_type<derived2, const var_t_shortened, int>(&vs);
    // Checking that Base is really determinated
    check_polymorphic_get_on_types_impl_single_type<base, var_t_shortened, base>(&vs);
    check_polymorphic_get_on_types_impl_single_type<base, const var_t_shortened, base>(&vs);

    vs = derived1();
    check_polymorphic_get_on_types_impl_single_type<derived2, var_t_shortened, int>(&vs);
    check_polymorphic_get_on_types_impl_single_type<derived2, const var_t_shortened, int>(&vs);
    // Checking that Base is really determinated
    check_polymorphic_get_on_types_impl_single_type<base, var_t_shortened, base>(&vs);
    check_polymorphic_get_on_types_impl_single_type<base, const var_t_shortened, base>(&vs);
}

inline void get_test_no_fallback()
{
    var_t_no_fallback v;
    var_t_no_fallback(base()).swap(v);
    check_polymorphic_get_on_types_impl_single_type<base, var_t_no_fallback, base>(&v);
    check_polymorphic_get_on_types_impl_single_type<base, const var_t_no_fallback, base>(&v);
    check_get_on_types_impl_single_type<base, var_t_no_fallback, base>(&v);
    check_get_on_types_impl_single_type<base, const var_t_no_fallback, base>(&v);

    var_t_no_fallback(derived1()).swap(v);
    check_polymorphic_get_on_types_impl_single_type<base, var_t_no_fallback, base>(&v);
    check_polymorphic_get_on_types_impl_single_type<base, const var_t_no_fallback, base>(&v);
    check_get_on_types_impl_single_type<derived1, var_t_no_fallback, derived1>(&v);
    check_get_on_types_impl_single_type<derived1, const var_t_no_fallback, derived1>(&v);

    var_t_no_fallback(derived2()).swap(v);
    check_polymorphic_get_on_types_impl_single_type<base, var_t_no_fallback, base>(&v);
    check_polymorphic_get_on_types_impl_single_type<base, const var_t_no_fallback, base>(&v);
    check_get_on_types_impl_single_type<derived2, var_t_no_fallback, derived2>(&v);
    check_get_on_types_impl_single_type<derived2, const var_t_no_fallback, derived2>(&v);
}

inline void get_ref_test()
{
    int i = 0;
    var_ref_t v(i);
    check_get_on_types<int>(&v);
    check_get_on_types<int&>(&v);

    base b;
    var_ref_t v1(b);
    check_get_on_types<base>(&v1);
    check_get_on_types<base&>(&v1);

    derived1 d1;
    var_ref_t v2(d1);
    check_get_on_types<derived1>(&v2);
    check_get_on_types<derived1&>(&v2);

    derived2 d2;
    var_ref_t v3(d2);
    check_get_on_types<derived2>(&v3);
    check_get_on_types<derived2&>(&v3);

    std::string s("Hello");
    var_ref_t v4(s);
    check_get_on_types<std::string>(&v4);
    check_get_on_types<std::string&>(&v4);
}


inline void get_cref_test()
{
    int i = 0;
    var_cref_t v(i);
    BOOST_TEST(boost::get<const int>(&v));
    BOOST_TEST(boost::get<const int&>(&v));
    BOOST_TEST(!boost::get<const base>(&v));

    base b;
    var_cref_t v1(b);
    BOOST_TEST(boost::get<const base>(&v1));
    BOOST_TEST(!boost::get<const derived1>(&v1));
    BOOST_TEST(!boost::get<const int>(&v1));

    std::string s("Hello");
    const var_cref_t v4 = s;
    BOOST_TEST(boost::get<const std::string>(&v4));
    BOOST_TEST(!boost::get<const int>(&v4));
}

inline void get_recursive_test()
{
    var_req_t v;
    check_get_on_types<int>(&v);

    var_req_t(base()).swap(v);
    check_get_on_types<base>(&v);

    var_req_t(derived1()).swap(v);
    check_get_on_types<derived1>(&v);

    var_req_t(derived2()).swap(v);
    check_get_on_types<derived2>(&v);

    var_req_t(std::string("Hello")).swap(v);
    check_get_on_types<std::string>(&v);

    recursive_structure s = { v }; // copying "v"
    v = s;
    check_get_on_types<recursive_structure>(&v);
}

template <class T>
inline void check_that_does_not_exist_impl()
{
    using namespace boost::detail::variant;

    BOOST_TEST((holds_element<T, const int>::value));
    BOOST_TEST((!holds_element<T, short>::value));
    BOOST_TEST((!holds_element<T, short>::value));
    BOOST_TEST((!holds_element<T, const short>::value));
    BOOST_TEST((!holds_element<T, char*>::value));
    BOOST_TEST((!holds_element<T, const char*>::value));
    BOOST_TEST((!holds_element<T, char[5]>::value));
    BOOST_TEST((!holds_element<T, const char[5]>::value));
    BOOST_TEST((!holds_element<T, bool>::value));
    BOOST_TEST((!holds_element<T, const bool>::value));

    BOOST_TEST((!holds_element<T, boost::recursive_wrapper<int> >::value));
    BOOST_TEST((!holds_element<T, boost::recursive_wrapper<short> >::value));
    BOOST_TEST((!holds_element<T, boost::detail::reference_content<short> >::value));


    BOOST_TEST((holds_element_polymorphic<T, const int>::value));
    BOOST_TEST((!holds_element_polymorphic<T, short>::value));
    BOOST_TEST((!holds_element_polymorphic<T, short>::value));
    BOOST_TEST((!holds_element_polymorphic<T, const short>::value));
    BOOST_TEST((!holds_element_polymorphic<T, char*>::value));
    BOOST_TEST((!holds_element_polymorphic<T, const char*>::value));
    BOOST_TEST((!holds_element_polymorphic<T, char[5]>::value));
    BOOST_TEST((!holds_element_polymorphic<T, const char[5]>::value));
    BOOST_TEST((!holds_element_polymorphic<T, bool>::value));
    BOOST_TEST((!holds_element_polymorphic<T, const bool>::value));

    BOOST_TEST((!holds_element_polymorphic<T, boost::recursive_wrapper<int> >::value));
    BOOST_TEST((!holds_element_polymorphic<T, boost::recursive_wrapper<short> >::value));
    BOOST_TEST((!holds_element_polymorphic<T, boost::detail::reference_content<short> >::value));
}

inline void check_that_does_not_exist()
{
    using namespace boost::detail::variant;

    BOOST_TEST((holds_element<var_t, int>::value));
    BOOST_TEST((holds_element<var_ref_t, int>::value));
    BOOST_TEST((!holds_element<var_cref_t, int>::value));

    check_that_does_not_exist_impl<var_t>();
    check_that_does_not_exist_impl<var_ref_t>();
    check_that_does_not_exist_impl<var_cref_t>();
    check_that_does_not_exist_impl<var_req_t>();
}

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
class MoveonlyType {
public:
    MoveonlyType() {}
    ~MoveonlyType() {}

    MoveonlyType(MoveonlyType&&) {}
    void operator=(MoveonlyType&&) {}

private:
    MoveonlyType(const MoveonlyType&);
    void operator=(const MoveonlyType&);
};

const boost::variant<int, std::string> foo1() { return ""; }
boost::variant<int, std::string> foo2() { return ""; }

inline void get_rvref_test()
{
  boost::get<std::string>(foo1());
  boost::get<std::string>(foo2());

  boost::variant<MoveonlyType, int> v;

  v = MoveonlyType();
  boost::get<MoveonlyType>(boost::move(v));

  v = 3;

  v = MoveonlyType();
  boost::get<MoveonlyType>(v);

  boost::relaxed_get<MoveonlyType&>(boost::variant<MoveonlyType, int>());

  v = MoveonlyType();
  MoveonlyType moved_from_variant(boost::get<MoveonlyType>(boost::move(v)));
}
#endif  // BOOST_NO_CXX11_RVALUE_REFERENCES

int main()
{
    get_test();
    get_test_no_fallback();
    get_ref_test();
    get_cref_test();
    get_recursive_test();
    check_that_does_not_exist();

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    get_rvref_test();
#endif

    return boost::report_errors();
}
