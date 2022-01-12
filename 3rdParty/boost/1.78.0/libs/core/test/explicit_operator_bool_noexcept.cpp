/*
 *          Copyright Andrey Semashev 2007 - 2013.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   explicit_operator_bool_noexcept.cpp
 * \author Andrey Semashev
 * \date   26.04.2014
 *
 * \brief  This test checks that explicit operator bool is noexcept when possible.
 */

#define BOOST_TEST_MODULE explicit_operator_bool_noexcept

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_NOEXCEPT)

#include <boost/core/lightweight_test.hpp>
#include <boost/utility/explicit_operator_bool.hpp>

namespace {

    struct checkable1
    {
        BOOST_EXPLICIT_OPERATOR_BOOL()
        bool operator! () const
        {
            return false;
        }
    };

    struct checkable2
    {
        BOOST_CONSTEXPR_EXPLICIT_OPERATOR_BOOL()
        BOOST_CONSTEXPR bool operator! () const
        {
            return false;
        }
    };

    struct noexcept_checkable1
    {
        BOOST_EXPLICIT_OPERATOR_BOOL_NOEXCEPT()
        bool operator! () const noexcept
        {
            return false;
        }
    };

    struct noexcept_checkable2
    {
        BOOST_CONSTEXPR_EXPLICIT_OPERATOR_BOOL()
        BOOST_CONSTEXPR bool operator! () const noexcept
        {
            return false;
        }
    };

} // namespace

int main(int, char*[])
{
    checkable1 val1;
    checkable2 val2;

    noexcept_checkable1 noexcept_val1;
    noexcept_checkable2 noexcept_val2;

    BOOST_TEST(!noexcept(static_cast< bool >(val1)));
    // constexpr functions are implicitly noexcept
    BOOST_TEST(noexcept(static_cast< bool >(val2)));

    BOOST_TEST(noexcept(static_cast< bool >(noexcept_val1)));
    BOOST_TEST(noexcept(static_cast< bool >(noexcept_val2)));

    (void)val1;
    (void)val2;
    (void)noexcept_val1;
    (void)noexcept_val2;

    return boost::report_errors();
}

#else

int main(int, char*[])
{
    return 0;
}

#endif
