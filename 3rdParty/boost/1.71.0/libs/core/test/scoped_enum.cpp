/*
 *             Copyright Andrey Semashev 2014.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   scoped_enum.cpp
 * \author Andrey Semashev
 * \date   06.06.2014
 *
 * \brief  This test checks that scoped enum emulation works similar to C++11 scoped enums.
 */

#include <boost/core/scoped_enum.hpp>
#include <boost/core/lightweight_test.hpp>

BOOST_SCOPED_ENUM_DECLARE_BEGIN(namespace_enum1)
{
    value0,
    value1,
    value2
}
BOOST_SCOPED_ENUM_DECLARE_END(namespace_enum1)

BOOST_SCOPED_ENUM_UT_DECLARE_BEGIN(namespace_enum2, unsigned char)
{
    // Checks that enum value names do not clash
    value0 = 10,
    value1 = 20,
    value2 = 30
}
BOOST_SCOPED_ENUM_DECLARE_END(namespace_enum2)

struct my_struct
{
    // Checks that declarations are valid in class scope
    BOOST_SCOPED_ENUM_DECLARE_BEGIN(color)
    {
        red,
        green,
        blue
    }
    BOOST_SCOPED_ENUM_DECLARE_END(color)

    color m_color;

    explicit my_struct(color col) : m_color(col)
    {
    }

    color get_color() const
    {
        return m_color;
    }
};

void check_operators()
{
    namespace_enum1 enum1 = namespace_enum1::value0;
    BOOST_TEST(enum1 == namespace_enum1::value0);
    BOOST_TEST(enum1 != namespace_enum1::value1);
    BOOST_TEST(enum1 != namespace_enum1::value2);

    enum1 = namespace_enum1::value1;
    BOOST_TEST(enum1 != namespace_enum1::value0);
    BOOST_TEST(enum1 == namespace_enum1::value1);
    BOOST_TEST(enum1 != namespace_enum1::value2);

    BOOST_TEST(!(enum1 < namespace_enum1::value0));
    BOOST_TEST(!(enum1 <= namespace_enum1::value0));
    BOOST_TEST(enum1 >= namespace_enum1::value0);
    BOOST_TEST(enum1 > namespace_enum1::value0);

    BOOST_TEST(!(enum1 < namespace_enum1::value1));
    BOOST_TEST(enum1 <= namespace_enum1::value1);
    BOOST_TEST(enum1 >= namespace_enum1::value1);
    BOOST_TEST(!(enum1 > namespace_enum1::value1));

    namespace_enum1 enum2 = namespace_enum1::value0;
    BOOST_TEST(enum1 != enum2);

    enum2 = enum1;
    BOOST_TEST(enum1 == enum2);
}

void check_argument_passing()
{
    my_struct str(my_struct::color::green);
    BOOST_TEST(str.get_color() == my_struct::color::green);
}

void check_switch_case()
{
    my_struct str(my_struct::color::blue);

    switch (boost::native_value(str.get_color()))
    {
    case my_struct::color::blue:
        break;
    default:
        BOOST_ERROR("Unexpected color value in switch/case");
    }
}

template< typename T >
struct my_trait
{
    enum _ { value = 0 };
};

template< >
struct my_trait< BOOST_SCOPED_ENUM_NATIVE(namespace_enum2) >
{
    enum _ { value = 1 };
};

template< typename T >
void native_type_helper(T)
{
    BOOST_TEST(my_trait< T >::value != 0);
}

void check_native_type()
{
    BOOST_TEST(my_trait< int >::value == 0);
    BOOST_TEST(my_trait< BOOST_SCOPED_ENUM_NATIVE(namespace_enum2) >::value != 0);
    BOOST_TEST(my_trait< boost::native_type< namespace_enum2 >::type >::value != 0);

    namespace_enum2 enum1 = namespace_enum2::value0;
    native_type_helper(boost::native_value(enum1));
}

void check_underlying_cast()
{
    namespace_enum2 enum1 = namespace_enum2::value1;
    BOOST_TEST(boost::underlying_cast< unsigned char >(enum1) == 20);
}

void check_underlying_type()
{
    // The real check for the type is in the underlying_type trait test.
    namespace_enum2 enum1 = namespace_enum2::value1;
    BOOST_TEST(sizeof(enum1) == sizeof(unsigned char));
}

int main(int, char*[])
{
    check_operators();
    check_argument_passing();
    check_switch_case();
    check_native_type();
    check_underlying_cast();
    check_underlying_type();

    return boost::report_errors();
}
