/* Copyright 2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_WORKAROUND_DR1467_HPP
#define BOOST_POLY_COLLECTION_DETAIL_WORKAROUND_DR1467_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>

#if BOOST_WORKAROUND(BOOST_GCC_VERSION,<50000)||\
    defined(BOOST_CLANG)&&\
      (__clang_major__<3||(__clang_major__==3&&__clang_minor__<=6))
/* Defect report 1467 denounces that for aggregate types an intended {}-style
 * copy construction will be mistaken for aggregate initialization:
 *   http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#1467
 *
 * A solution is to define a default constructor for the offending class so
 * that it is no longer an aggregate.
 */

#define BOOST_POLY_COLLECTION_WORKAROUND_DR1467(name) name(){}
#else
#define BOOST_POLY_COLLECTION_WORKAROUND_DR1467(name)
#endif

#endif
