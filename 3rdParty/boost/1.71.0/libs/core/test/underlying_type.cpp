/*
 *             Copyright Andrey Semashev 2014.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   underlying_type.cpp
 * \author Andrey Semashev
 * \date   06.06.2014
 *
 * \brief  This test checks that underlying_type trait works.
 */

#include <boost/core/underlying_type.hpp>
#include <boost/core/scoped_enum.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/core/is_same.hpp>
#include <boost/config.hpp>

BOOST_SCOPED_ENUM_UT_DECLARE_BEGIN(emulated_enum, unsigned char)
{
    value0,
    value1,
    value2
}
BOOST_SCOPED_ENUM_DECLARE_END(emulated_enum)

#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)

enum class native_enum : unsigned short
{
    value0,
    value1,
    value2
};

#endif

#if defined(BOOST_NO_UNDERLYING_TYPE)
namespace boost {

template< >
struct underlying_type< emulated_enum >
{
    typedef unsigned char type;
};

#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)
template< >
struct underlying_type< native_enum >
{
    typedef unsigned short type;
};
#endif

} // namespace boost
#endif

int main(int, char*[])
{
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same< boost::underlying_type< emulated_enum >::type, unsigned char >));
#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same< boost::underlying_type< native_enum >::type, unsigned short >));
#endif

    return boost::report_errors();
}
