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

#include "Basics/Common.h"

#include "catch.hpp"

#include "Pregel/TypedBuffer.h"

using namespace arangodb::pregel;

/***************************************/
TEST_CASE("tst_pregel1", "[pregel][mmap]") {
#if 0
  MappedFileBuffer<int> mapped(1024);
  int *ptr = mapped.data();
  for (int i = 0; i < 1024; i++) {
    *(ptr+i) = i;
  }
  for (int i = 0; i < 1024; i++) {
    REQUIRE(*(ptr+i) == i);
  }
  
#ifdef __linux__
  mapped.resize(2048);
  REQUIRE(mapped.size() == 2048);
  ptr = mapped.data();
  for (int i = 0; i < 1024; i++) {
    REQUIRE(*(ptr+i) == i);
  }
#endif
  
  mapped.resize(512);
  ptr = mapped.data();
  REQUIRE(mapped.size() == 512);
  for (int i = 0; i < 512; i++) {
    REQUIRE(*(ptr+i) == i);
  }
  
  mapped.close();
  REQUIRE(mapped.data() == nullptr);
#endif
}

