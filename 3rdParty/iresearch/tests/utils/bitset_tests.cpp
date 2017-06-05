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
#include "utils/bitset.hpp"

using namespace iresearch;

TEST(bitset_tests, ctor) {
  /* zero size bitset*/
  {
    const bitset::index_t words = 0;
    const bitset::index_t size = 0;
    const bitset bs( size );
    ASSERT_EQ(nullptr, bs.data());
    ASSERT_EQ( size, bs.size() );
    ASSERT_EQ( words, bs.words() );
    ASSERT_EQ(words, bs.capacity());
    ASSERT_TRUE( bs.none() );
    ASSERT_FALSE( bs.any() );
    ASSERT_TRUE( bs.all() );
  }

  /* less that 1 word size bitset*/
  {
    const bitset::index_t words = 1;
    const bitset::index_t size = 32;
    const bitset bs( size );
    ASSERT_NE(nullptr, bs.data());
    ASSERT_EQ( size, bs.size() );
    ASSERT_EQ( words, bs.words() );
    ASSERT_EQ(words, bs.capacity());
    ASSERT_TRUE( bs.none() );
    ASSERT_FALSE( bs.any() );
    ASSERT_FALSE( bs.all() );
  }

  /* uint64_t size bitset */
  {
    const bitset::index_t size = 64;
    const bitset::index_t words = 1;
    const bitset bs( size );
    ASSERT_NE(nullptr,  bs.data());
    ASSERT_EQ( size, bs.size() );
    ASSERT_EQ( words, bs.words() );
    ASSERT_EQ(words, bs.capacity());
    ASSERT_TRUE( bs.none() );
    ASSERT_FALSE( bs.any() );
    ASSERT_FALSE( bs.all() );
  }
  
  /* nonzero size bitset */
  {
    const bitset::index_t words = 2;
    const bitset::index_t size = 78;
    const bitset bs( size );
    ASSERT_NE(nullptr, bs.data());
    ASSERT_EQ( size, bs.size() );
    ASSERT_EQ( words, bs.words() );
    ASSERT_EQ(words, bs.capacity());
    ASSERT_EQ( 0, bs.count() );
    ASSERT_TRUE( bs.none() );
    ASSERT_FALSE( bs.any() );
    ASSERT_FALSE( bs.all() );
  }  
}

TEST(bitset_tests, set_unset) {
  const bitset::index_t words = 3;
  const bitset::index_t size = 155;
  bitset bs(size);
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_EQ(words, bs.capacity());
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none());
  ASSERT_FALSE(bs.any());
  ASSERT_FALSE(bs.all());

  /* set */
  const bitset::index_t i = 43;
  ASSERT_FALSE(bs.test(i));
  bs.set(i);
  ASSERT_TRUE(bs.test(i));

  /* unset */
  bs.unset(i);
  ASSERT_FALSE(bs.test(i));

  /* reset */
  bs.reset(i, true );
  ASSERT_TRUE(bs.test(i));
  bs.reset(i, false );
  ASSERT_FALSE(bs.test(i));
}

TEST(bitset_tests, reset) {
  bitset bs;
  ASSERT_EQ(nullptr, bs.data());
  ASSERT_EQ(0, bs.size());
  ASSERT_EQ(0, bs.words());
  ASSERT_EQ(0, bs.capacity());
  ASSERT_TRUE(bs.none());
  ASSERT_FALSE(bs.any());
  ASSERT_TRUE(bs.all());

  // reset to bigger size
  bitset::index_t capacity = 3;
  bitset::index_t words = 3;
  bitset::index_t size = 155;

  bs.reset(size);
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_EQ(capacity, bs.capacity());
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none());
  ASSERT_FALSE(bs.any());
  ASSERT_FALSE(bs.all());
  bs.set(42);
  ASSERT(1, bs.count());
  bs.set(42);
  ASSERT(1, bs.count());
  bs.set(73); 
  ASSERT(2, bs.count());
  ASSERT_FALSE(bs.none() );
  ASSERT_TRUE(bs.any());
  ASSERT_FALSE(bs.all());
  const auto* prev_data = bs.data();
  
  // reset to smaller size
  words = 2;
  size = 89;

  bs.reset(size); // reset to smaller size
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_EQ(capacity, bs.capacity()); // capacity haven't changed
  ASSERT_EQ(prev_data, bs.data()); // storage haven't changed
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none()); // content cleared
  ASSERT_FALSE(bs.any()); // content cleared
  ASSERT_FALSE(bs.all()); // content cleared
  bs.set(42);
  ASSERT(1, bs.count());
  bs.set(42);
  ASSERT(1, bs.count());
  bs.set(73); 
  ASSERT(2, bs.count());
  ASSERT_FALSE(bs.none() );
  ASSERT_TRUE(bs.any());
  ASSERT_FALSE(bs.all());
  
  // reset to bigger size
  words = 5;
  capacity = 5;
  size = 319;

  bs.reset(size); // reset to smaller size
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ(size, bs.size());
  ASSERT_EQ(words, bs.words());
  ASSERT_EQ(capacity, bs.capacity()); // capacity changed
  ASSERT_NE(prev_data, bs.data()); // storage was reallocated
  ASSERT_EQ(0, bs.count());
  ASSERT_TRUE(bs.none()); // content cleared
  ASSERT_FALSE(bs.any()); // content cleared
  ASSERT_FALSE(bs.all()); // content cleared
  bs.set(42);
  ASSERT(1, bs.count());
  bs.set(42);
  ASSERT(1, bs.count());
  bs.set(73); 
  ASSERT(2, bs.count());
  ASSERT_FALSE(bs.none() );
  ASSERT_TRUE(bs.any());
  ASSERT_FALSE(bs.all());
}

TEST(bitset_tests, clear_count) {
  const bitset::index_t words = 3;
  const bitset::index_t size = 155;
  bitset bs( size );
  ASSERT_NE(nullptr, bs.data());
  ASSERT_EQ( size, bs.size() );
  ASSERT_EQ( words, bs.words() );
  ASSERT_EQ(words, bs.capacity());
  ASSERT_EQ( 0, bs.count() );
  ASSERT_TRUE( bs.none() );
  ASSERT_FALSE( bs.any() );
  ASSERT_FALSE( bs.all() );

  bs.set( 42 );
  ASSERT( 1, bs.count() );
  bs.set( 42 );
  ASSERT( 1, bs.count() );
  bs.set( 73 ); 
  ASSERT( 2, bs.count() );
  ASSERT_FALSE( bs.none() );
  ASSERT_TRUE( bs.any() );
  ASSERT_FALSE( bs.all() );

  /* set almost all bits */
  const bitset::index_t end = 100;
  for ( bitset::index_t i = 0; i < end; ++i ) {
    bs.set( i );
  }
  ASSERT_EQ( end, bs.count() );
  ASSERT_FALSE( bs.none() );
  ASSERT_TRUE( bs.any() );
  ASSERT_FALSE( bs.all() );

  /* set almost all */
  for ( bitset::index_t i = 0; i < bs.size(); ++i ) {
    bs.set( i );
  }
  ASSERT_EQ( bs.size(), bs.count() );
  ASSERT_FALSE( bs.none() );
  ASSERT_TRUE( bs.any() );
  ASSERT_TRUE( bs.all() );

  /* clear all bits */
  bs.clear();
  ASSERT_TRUE( bs.none() );
  ASSERT_FALSE( bs.any() );
  ASSERT_FALSE( bs.all() );
}
