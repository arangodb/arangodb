//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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
