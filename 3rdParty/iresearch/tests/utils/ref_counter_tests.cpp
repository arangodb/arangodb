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

#include "gtest/gtest.h"
#include "utils/ref_counter.hpp"

namespace tests {
  class ref_counter_tests: public ::testing::Test {
    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };
}

using namespace tests;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(ref_counter_tests, test_ref_counter_add) {
  {
    iresearch::ref_counter<int> refs;

    ASSERT_EQ(0, refs.find(1));
    ASSERT_EQ(0, refs.find(2));

    auto ref1a = refs.add(1);
    auto* ref1a_val = ref1a.get();

    ASSERT_FALSE(ref1a.unique());
    ASSERT_EQ(2, ref1a.use_count());
    ASSERT_EQ(1, *(ref1a.get()));
    ASSERT_EQ(1, refs.find(1));

    auto ref2 = refs.add(2);

    ASSERT_FALSE(ref2.unique());
    ASSERT_EQ(2, ref2.use_count());
    ASSERT_EQ(2, *(ref2.get()));
    ASSERT_EQ(1, refs.find(2));

    auto ref1b = refs.add(1);

    ASSERT_FALSE(ref1b.unique());
    ASSERT_EQ(3, ref1b.use_count());
    ASSERT_EQ(1, *(ref1b.get()));
    ASSERT_EQ(ref1a_val, ref1b.get());
    ASSERT_EQ(2, refs.find(1));

    ref1a.reset();
    ASSERT_EQ(2, ref1b.use_count());
    ASSERT_EQ(1, *(ref1b.get()));
    ASSERT_EQ(1, refs.find(1));

    ref1b.reset();
    ASSERT_EQ(0, refs.find(1));

    auto ref1c = refs.add(1);

    ASSERT_FALSE(ref1c.unique());
    ASSERT_EQ(2, ref1c.use_count());
    ASSERT_EQ(1, *(ref1c.get()));
    ASSERT_EQ(ref1a_val, ref1c.get());
    ASSERT_EQ(1, refs.find(1));
  }

  {
    iresearch::ref_counter<std::string> refs;

    ASSERT_EQ(0, refs.find("abc"));
    ASSERT_EQ(0, refs.find("def"));

    auto ref1a = refs.add("abc");
    auto* ref1a_val = ref1a.get();

    ASSERT_FALSE(ref1a.unique());
    ASSERT_EQ(2, ref1a.use_count());
    ASSERT_EQ(std::string("abc"), *(ref1a.get()));
    ASSERT_EQ(1, refs.find("abc"));

    auto ref2 = refs.add("def");

    ASSERT_FALSE(ref2.unique());
    ASSERT_EQ(2, ref2.use_count());
    ASSERT_EQ(std::string("def"), *(ref2.get()));
    ASSERT_EQ(1, refs.find("def"));

    auto ref1b = refs.add("abc");

    ASSERT_FALSE(ref1b.unique());
    ASSERT_EQ(3, ref1b.use_count());
    ASSERT_EQ(std::string("abc"), *(ref1b.get()));
    ASSERT_EQ(ref1a_val, ref1b.get());
    ASSERT_EQ(2, refs.find("abc"));

    ref1a.reset();
    ASSERT_EQ(2, ref1b.use_count());
    ASSERT_EQ(std::string("abc"), *(ref1b.get()));
    ASSERT_EQ(1, refs.find("abc"));

    ref1b.reset();
    ASSERT_EQ(0, refs.find("abc"));

    auto ref1c = refs.add("abc");

    ASSERT_FALSE(ref1c.unique());
    ASSERT_EQ(2, ref1c.use_count());
    ASSERT_EQ(std::string("abc"), *(ref1c.get()));
    ASSERT_EQ(ref1a_val, ref1c.get());
    ASSERT_EQ(1, refs.find("abc"));
  }
}

TEST_F(ref_counter_tests, test_ref_counter_contains) {
  iresearch::ref_counter<int> refs;

  ASSERT_FALSE(refs.contains(1));
  ASSERT_FALSE(refs.contains(2));

  auto ref = refs.add(1);

  ASSERT_TRUE(refs.contains(1));
  ref.reset();
  ASSERT_TRUE(refs.contains(1));
}

TEST_F(ref_counter_tests, test_ref_counter_visit) {
  iresearch::ref_counter<int> refs;

  auto ref1a = refs.add(1);
  auto ref1b = refs.add(1);
  auto ref2a = refs.add(2);
  auto ref2b = refs.add(2);
  auto ref2c = refs.add(2);

  // test full visitation
  {
    std::unordered_map<int, size_t> expected = { {1, 2}, {2, 3} };
    auto visitor = [&expected](const int& key, size_t count)->bool {
      auto itr = expected.find(key);
      EXPECT_EQ(itr->second, count);
      expected.erase(itr);
      return true;
    };

    ASSERT_TRUE(refs.visit(visitor));
    ASSERT_TRUE(expected.empty());
  }

  // test early terminate
  {
    std::unordered_map<int, size_t> expected = { {1, 2}, {2, 3} };
    auto visitor = [&expected](const int& key, size_t count)->bool {
      auto itr = expected.find(key);
      EXPECT_EQ(itr->second, count);
      expected.erase(itr);
      return false;
    };

    ASSERT_FALSE(refs.visit(visitor));
    ASSERT_EQ(1, expected.size()); // only 1 element removed out of 2
  }

  // test remove unused
  {
    std::unordered_map<int, size_t> expected = { {1, 0}, {2, 2} };
    auto visitor = [&expected](const int& key, size_t count)->bool {
      auto itr = expected.find(key);
      EXPECT_EQ(itr->second, count);
      expected.erase(itr);
      return true;
    };

    ref1a.reset();
    ref1b.reset();
    ref2a.reset();
    ASSERT_FALSE(expected.empty());
    ASSERT_TRUE(refs.visit(visitor, true));
    ASSERT_TRUE(expected.empty());
  }

  // test empty
  {
    auto visitor = [](const int& key, size_t count)->bool { return true; };

    ASSERT_FALSE(refs.empty());
    ASSERT_TRUE(refs.visit(visitor, true));
    ASSERT_FALSE(refs.empty());
    ref2b.reset();
    ref2c.reset();
    ASSERT_FALSE(refs.empty());
    ASSERT_TRUE(refs.visit(visitor, true));
    ASSERT_TRUE(refs.empty());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
