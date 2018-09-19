//
// Copyright (c) 2018 Christian Henning (chhenning at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/gil
//

#ifndef BOOST_GIL_VERSION_HPP
#define BOOST_GIL_VERSION_HPP

/** @def BOOST_GIL_API_VERSION 
    Identifies the API version of gil.
    This is a simple integer that is incremented by one every
    time a set of code changes is merged to the develop branch.
*/
#define BOOST_GIL_VERSION 22

/**
//  BOOST_GIL_LIB_VERSION must be defined to be the same as BOOST_GIL_VERSION
//  but as a *string* in the form "x_y[_z]" where x is the major version
//  number, y is the minor version number, and z is the patch level if not 0.
*/
#define BOOST_GIL_LIB_VERSION "2_2"

#endif