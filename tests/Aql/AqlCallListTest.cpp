////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/AqlCallList.h"

using namespace arangodb::aql;

namespace arangodb::tests::aql {
class AqlCallListTest : public ::testing::Test {};

TEST_F(AqlCallListTest, only_single_call) {
  AqlCall myCall{};
  myCall.offset = 3u;
  myCall.softLimit = 9u;

  AqlCallList testee(myCall);
  EXPECT_TRUE(testee.hasMoreCalls());
  AqlCall popped = testee.popNextCall();
  EXPECT_EQ(myCall, popped);
  EXPECT_FALSE(testee.hasMoreCalls());

  // Make sure calls are non-referenced
  myCall.offset = 9;
  EXPECT_FALSE(myCall == popped);
}

TEST_F(AqlCallListTest, only_single_call_peek) {
  AqlCall myCall{};
  myCall.offset = 3;
  myCall.softLimit = 9u;

  AqlCallList testee(myCall);
  EXPECT_TRUE(testee.hasMoreCalls());
  AqlCall popped = testee.peekNextCall();
  EXPECT_EQ(myCall, popped);
  EXPECT_TRUE(testee.hasMoreCalls());

  // Make sure calls are non-referenced
  popped.offset = 9;
  EXPECT_FALSE(myCall == popped);

  AqlCall popped2 = testee.popNextCall();
  EXPECT_EQ(myCall, popped2);
  EXPECT_FALSE(testee.hasMoreCalls());
}

TEST_F(AqlCallListTest, multiple_calls) {
  AqlCall myFirstCall{};
  myFirstCall.offset = 3;
  myFirstCall.softLimit = 9u;

  AqlCall defaultCall{};
  defaultCall.hardLimit = 2u;
  defaultCall.fullCount = true;
  EXPECT_FALSE(myFirstCall == defaultCall);

  AqlCallList testee(myFirstCall, defaultCall);
  {
    // Test the First specific call
    EXPECT_TRUE(testee.hasMoreCalls());
    AqlCall popped = testee.popNextCall();
    // it is equal to myFirstCall
    EXPECT_EQ(popped, myFirstCall);
    EXPECT_FALSE(popped == defaultCall);
    EXPECT_TRUE(testee.hasMoreCalls());
  }
  // 3 is a random number, we should be able to loop here forever.
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_TRUE(testee.hasMoreCalls());
    AqlCall popped = testee.popNextCall();
    // it is equal to the defaultCall
    EXPECT_EQ(popped, defaultCall);
    EXPECT_FALSE(popped == myFirstCall);
    EXPECT_TRUE(testee.hasMoreCalls());
    // Modifying has no side effect on default call
    popped.didProduce(1);
    EXPECT_FALSE(popped == defaultCall);
    // Indirect test, modifing pop has no side-effect on the internal default call.
  }
}

TEST_F(AqlCallListTest, multiple_calls_peek) {
  AqlCall myFirstCall{};
  myFirstCall.offset = 3;
  myFirstCall.softLimit = 9u;

  AqlCall defaultCall{};
  defaultCall.hardLimit = 2u;
  defaultCall.fullCount = true;
  EXPECT_FALSE(myFirstCall == defaultCall);

  AqlCallList testee(myFirstCall, defaultCall);
  {
    // Test peek the first specific call
    EXPECT_TRUE(testee.hasMoreCalls());
    AqlCall peeked = testee.peekNextCall();
    // it is equal to myFirstCall
    EXPECT_EQ(peeked, myFirstCall);
    EXPECT_FALSE(peeked == defaultCall);
    EXPECT_TRUE(testee.hasMoreCalls());
  }
  {
    // Test pop the First specific call
    EXPECT_TRUE(testee.hasMoreCalls());
    AqlCall popped = testee.popNextCall();
    // it is equal to myFirstCall
    EXPECT_EQ(popped, myFirstCall);
    EXPECT_FALSE(popped == defaultCall);
    EXPECT_TRUE(testee.hasMoreCalls());
  }
  // 3 is a random number, we should be able to loop here forever.
  for (size_t i = 0; i < 3; ++i) {
    {
      // Test peek
      EXPECT_TRUE(testee.hasMoreCalls());
      AqlCall peeked = testee.peekNextCall();
      // it is equal to myFirstCall
      EXPECT_EQ(peeked, defaultCall);
      EXPECT_FALSE(peeked == myFirstCall);
      EXPECT_TRUE(testee.hasMoreCalls());
    }
    {
      EXPECT_TRUE(testee.hasMoreCalls());
      AqlCall popped = testee.popNextCall();
      // it is equal to the defaultCall
      EXPECT_EQ(popped, defaultCall);
      EXPECT_FALSE(popped == myFirstCall);
      EXPECT_TRUE(testee.hasMoreCalls());
      // Modifying has no side effect on default call
      popped.didProduce(1);
      EXPECT_FALSE(popped == defaultCall);
    }
    // Indirect test, modifing pop has no side-effect on the internal default call.
  }
}

}  // namespace arangodb::tests::aql
