/*
 *             Copyright Andrey Semashev 2014.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   scoped_enum_compile_fail_conv_from_int.cpp
 * \author Andrey Semashev
 * \date   06.06.2014
 *
 * \brief  This test checks that scoped enum emulation prohibits implicit conversions from int
 */

#include <boost/core/scoped_enum.hpp>

BOOST_SCOPED_ENUM_DECLARE_BEGIN(color)
{
    red,
    green,
    blue
}
BOOST_SCOPED_ENUM_DECLARE_END(color)

int main(int, char*[])
{
    color col = 2;

    return boost::native_value(col);
}
