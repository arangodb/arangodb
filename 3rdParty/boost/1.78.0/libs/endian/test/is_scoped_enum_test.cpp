// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/detail/is_scoped_enum.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/config.hpp>

enum E1 {};

#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)

enum E2: long {};
enum class E3 {};
enum class E4: long {};

#endif

struct X
{
    operator int() const { return 0; }
};

struct Y;

template<class T> void test_true()
{
    using boost::endian::detail::is_scoped_enum;

    BOOST_TEST_TRAIT_TRUE((is_scoped_enum<T>));
    BOOST_TEST_TRAIT_TRUE((is_scoped_enum<T const>));
    BOOST_TEST_TRAIT_TRUE((is_scoped_enum<T volatile>));
    BOOST_TEST_TRAIT_TRUE((is_scoped_enum<T const volatile>));
}

template<class T> void test_false()
{
    using boost::endian::detail::is_scoped_enum;

    BOOST_TEST_TRAIT_FALSE((is_scoped_enum<T>));
    BOOST_TEST_TRAIT_FALSE((is_scoped_enum<T const>));
    BOOST_TEST_TRAIT_FALSE((is_scoped_enum<T volatile>));
    BOOST_TEST_TRAIT_FALSE((is_scoped_enum<T const volatile>));
}

template<class T> void test_false_()
{
    using boost::endian::detail::is_scoped_enum;

    BOOST_TEST_NOT((is_scoped_enum<T>::value));
    BOOST_TEST_NOT((is_scoped_enum<T const>::value));
    BOOST_TEST_NOT((is_scoped_enum<T volatile>::value));
    BOOST_TEST_NOT((is_scoped_enum<T const volatile>::value));
}

int main()
{
    test_false<int>();
    test_false<bool>();
    test_false<X>();
    test_false_<Y>();
    test_false<void>();
    test_false<int[]>();
    test_false<int[1]>();

    test_false<E1>();

#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)

    test_false<E2>();

    test_true<E3>();
    test_true<E4>();

#endif

    return boost::report_errors();
}
