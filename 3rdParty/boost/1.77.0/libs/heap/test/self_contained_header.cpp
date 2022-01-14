/*
 *            Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   self_contained_header.cpp
 * \author Andrey Semashev
 * \date   28.10.2018
 *
 * \brief  This file contains a test boilerplate for checking that every public header is self-contained and does not have any missing #includes.
 */

#define BOOST_HEAP_TEST_INCLUDE_HEADER() <boost/heap/BOOST_HEAP_TEST_HEADER>

#include BOOST_HEAP_TEST_INCLUDE_HEADER()

int main(int, char*[])
{
    return 0;
}
