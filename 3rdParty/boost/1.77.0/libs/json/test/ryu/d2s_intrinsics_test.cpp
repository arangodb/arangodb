// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

/*
    This is a derivative work
*/

#include <boost/json/detail/ryu/detail/d2s_intrinsics.hpp>
#include "gtest.hpp"
#include <math.h>

BOOST_JSON_NS_BEGIN
namespace detail {

namespace ryu {
namespace detail {

TEST(D2sIntrinsicsTest, mod1e9) {
  ASSERT_EQ(0, mod1e9(0));
  ASSERT_EQ(1, mod1e9(1));
  ASSERT_EQ(2, mod1e9(2));
  ASSERT_EQ(10, mod1e9(10));
  ASSERT_EQ(100, mod1e9(100));
  ASSERT_EQ(1000, mod1e9(1000));
  ASSERT_EQ(10000, mod1e9(10000));
  ASSERT_EQ(100000, mod1e9(100000));
  ASSERT_EQ(1000000, mod1e9(1000000));
  ASSERT_EQ(10000000, mod1e9(10000000));
  ASSERT_EQ(100000000, mod1e9(100000000));
  ASSERT_EQ(0, mod1e9(1000000000));
  ASSERT_EQ(0, mod1e9(2000000000));
  ASSERT_EQ(1, mod1e9(1000000001));
  ASSERT_EQ(1234, mod1e9(1000001234));
  ASSERT_EQ(123456789, mod1e9(12345123456789ull));
  ASSERT_EQ(123456789, mod1e9(123456789123456789ull));
}

} // detail
} // ryu

} // detail
BOOST_JSON_NS_END
