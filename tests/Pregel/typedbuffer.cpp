////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Simon Grätzer
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


#include "ApplicationFeatures/ApplicationServer.h"
#include "Pregel/TypedBuffer.h"

#include "gtest/gtest.h"


using namespace arangodb::pregel;

/***************************************/
TEST(PregelTypedBufferTest, test_with_malloc) {
  
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  
  VectorTypedBuffer<int> mapped(1024);
  ASSERT_EQ(mapped.size(), 0);
  ASSERT_EQ(mapped.capacity(), 1024);
  ASSERT_EQ(mapped.remainingCapacity(), 1024);
  
  mapped.advance(1024);
  ASSERT_EQ(mapped.size(), 1024);
  ASSERT_EQ(mapped.capacity(), 1024);
  ASSERT_EQ(mapped.remainingCapacity(), 0);
  
  int *ptr = mapped.begin();
  for (int i = 0; i < 1024; i++) {
    *(ptr+i) = i;
  }
  
  for (int i = 0; i < 1024; i++) {
    ASSERT_EQ(*(ptr+i), i);
  }
}

TEST(PregelTypedBufferTest, test_with_mmap) {
  
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  
  MappedFileBuffer<int64_t> mapped(1024);
  ASSERT_EQ(mapped.size(), 0);
  ASSERT_EQ(mapped.capacity(), 1024);
  ASSERT_EQ(mapped.remainingCapacity(), 1024);
  
  mapped.advance(1024);
  ASSERT_EQ(mapped.size(), 1024);
  ASSERT_EQ(mapped.capacity(), 1024);
  ASSERT_EQ(mapped.remainingCapacity(), 0);
  
  int64_t *ptr = mapped.begin();
  for (int i = 0; i < 1024; i++) {
    *(ptr+i) = i;
  }
  
  for (int i = 0; i < 1024; i++) {
    ASSERT_EQ(*(ptr+i), i);
  }
  
  mapped.close();
  ASSERT_EQ(mapped.begin(), nullptr);
}
