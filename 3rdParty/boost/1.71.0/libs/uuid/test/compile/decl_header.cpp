/*
 *             Copyright Andrey Semashev 2015.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          https://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   decl_header.cpp
 * \author Andrey Semashev
 * \date   21.06.2015
 *
 * \brief  This file contains a test boilerplate for checking that every
 *         public header is self-contained and does not have any missing
 *         #includes.
 */

#define BOOST_UUID_TEST_INCLUDE_HEADER() <boost/uuid/BOOST_UUID_TEST_HEADER>

#include BOOST_UUID_TEST_INCLUDE_HEADER()

int main(int, char*[])
{
    return 0;
}
