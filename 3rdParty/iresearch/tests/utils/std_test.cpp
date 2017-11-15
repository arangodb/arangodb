////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/std.hpp"

using namespace iresearch;

TEST(std_test, head_for_each_if) {
  /* usual case */
  {
    std::vector<int> src = {
      0, 7, 28, 1, 2, 5, 7, 9, 1, 3, 65, 0, 0, 0, 1, 2, 3, 0, 4, 0, 0, 1
    };

    /* create min heap */
    std::make_heap( src.begin(), src.end(), std::greater<int>() );
    const std::vector<int> expected( 7, 0 );

    std::vector<int> result;
    std::vector<const int*> refs;
    irstd::heap::for_each_if(
     src.begin(), src.end(),
     [](int i) { return 0 == i; },
     [&result,&refs] (const int& i) {
       result.push_back(i);
       refs.push_back(&i);
    });

    refs.erase(std::unique(refs.begin(),refs.end()), refs.end());
    ASSERT_EQ(expected, result);
    /* check that elements are different */
    ASSERT_EQ(result.size(),refs.size());
  }
}

TEST(std_test, heap_for_each_top) {
  /* usual case */
  {
    std::vector<int> src = {
      0, 7, 28, 1, 2, 5, 7, 9, 1, 3, 65, 0, 0, 0, 1, 2, 3, 0, 4, 0, 0, 1
    };

    /* create min heap */
    std::make_heap( src.begin(), src.end(), std::greater<int>() );

    const std::vector<int> expected( 7, 0 );

    std::vector<int> result;
    std::vector<const int*> refs;
    irstd::heap::for_each_top(
      src.begin(), src.end(),
      [&result,&refs] (const int& i) {
      result.push_back(i);
      refs.push_back(&i);
    });

    refs.erase(std::unique(refs.begin(),refs.end()), refs.end());
    ASSERT_EQ(expected, result);
    /* check that elements are different */
    ASSERT_EQ(result.size(),refs.size());
  }

  /* empty */
  {
    const std::vector<int> src;

    std::vector<int> result;
    std::vector<const int*> refs;
    irstd::heap::for_each_top(
      src.begin(), src.end(),
      [&result,&refs] ( const int& i ) {
        result.push_back( i );
        refs.push_back(&i);
    } );

    refs.erase(std::unique(refs.begin(),refs.end()), refs.end());
    ASSERT_EQ(src, result);
    /* check that elements are different */
    ASSERT_EQ(result.size(),refs.size());
  }

  /* all equals */
  {
    const std::vector<int> src( 15, 1 );

    std::vector<int> result;
    std::vector<const int*> refs;
    irstd::heap::for_each_top(
      src.begin(), src.end(),
      [&result,&refs] (const int& i ) {
      result.push_back( i );
      refs.push_back(&i);
    } );

    refs.erase(std::unique(refs.begin(),refs.end()), refs.end());
    ASSERT_EQ(src, result);
    /* check that elements are different */
    ASSERT_EQ(result.size(),refs.size());
  }
}

TEST(std_test, ALL_EQUAL) {
  /* equals */
  {
    const std::vector<int> src( 10, 0 );
    ASSERT_TRUE( irstd::all_equal( src.begin(), src.end() ) );
  }

  /* empty */
  {
    const std::vector<int> src;
    ASSERT_TRUE( irstd::all_equal( src.begin(), src.end() ) );
  }

  /* not equals */
  {
    const std::vector<int> src{ 0, 4, 1, 3, 4, 1, 2, 51, 21, 32 };
    ASSERT_FALSE( irstd::all_equal( src.begin(), src.end() ) );
  }
}
