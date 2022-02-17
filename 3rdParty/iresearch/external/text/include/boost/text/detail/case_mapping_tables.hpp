// Copyright (C) 2021 Andrey Abramov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_CASE_MAPPING_TABLES_HPP
#define BOOST_TEXT_DETAIL_CASE_MAPPING_TABLES_HPP

#include <frozen/set.h>

namespace boost { namespace text { namespace detail {

    static constexpr frozen::set<uint32_t, 46> SOFT_DOTTED_CPS {
      0x0069,
      0x6A,
      0x012F,
      0x0249,
      0x0268,
      0x029D,
      0x02B2,
      0x03F3,
      0x0456,
      0x0458,
      0x1D62,
      0x1D96,
      0x1DA4,
      0x1DA8,
      0x1E2D,
      0x1ECB,
      0x2071,
      0x2148,
      0x2149,
      0x2C7C,
      0x1D422,
      0x1D423,
      0x1D456,
      0x1D457,
      0x1D48A,
      0x1D48B,
      0x1D4BE,
      0x1D4BF,
      0x1D4F2,
      0x1D4F3,
      0x1D526,
      0x1D527,
      0x1D55A,
      0x1D55B,
      0x1D58E,
      0x1D58F,
      0x1D5C2,
      0x1D5C3,
      0x1D5F6,
      0x1D5F7,
      0x1D62A,
      0x1D62B,
      0x1D65E,
      0x1D65F,
      0x1D692,
      0x1D693
    };
}}}

#endif // BOOST_TEXT_DETAIL_CASE_MAPPING_TABLES_HPP
