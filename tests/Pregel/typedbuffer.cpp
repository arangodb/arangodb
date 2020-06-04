////////////////////////////////////////////////////////////////////////
/// @brief geo index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by ArangoDB GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
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
