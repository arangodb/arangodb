////////////////////////////////////////////////////////////////////////////////
/// @brief tests for datafile structure sizes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesWalMarker.h"

template<typename T, typename U> size_t offsetOf (U T::*member) {
  return (char*) &((T*)nullptr->*member) - (char*) nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CStructureSizeTest", "[structure-size]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof some basic elements
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_basic_elements") {
  CHECK(4 == (int) sizeof(TRI_col_type_e));
  CHECK(1 == (int) sizeof(MMFilesMarkerType));
  CHECK(4 == (int) sizeof(MMFilesDatafileVersionType));
  CHECK(8 == (int) sizeof(TRI_voc_cid_t));
  CHECK(4 == (int) sizeof(TRI_voc_crc_t));
  CHECK(8 == (int) sizeof(TRI_voc_tid_t));
  CHECK(8 == (int) sizeof(TRI_voc_rid_t));
  CHECK(8 == (int) sizeof(TRI_voc_tick_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesMarker
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_df_marker") {
  size_t s = sizeof(MMFilesMarker);

  MMFilesMarker m; 
  CHECK(16 == (int) s);
  CHECK(s % 8 == 0); 

  CHECK(0 == (int) m.offsetOfSize());
  CHECK(4 == (int) m.offsetOfCrc());
  CHECK(8 == (int) m.offsetOfTypeAndTick());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesDatafileHeaderMarker
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_df_header_marker") {
  size_t s = sizeof(MMFilesDatafileHeaderMarker);

  CHECK(16 + 16 == (int) s);
  CHECK(s % 8 == 0); 

  CHECK(16 == (int) offsetOf(&MMFilesDatafileHeaderMarker::_version));
  CHECK(20 == (int) offsetOf(&MMFilesDatafileHeaderMarker::_maximalSize));
  CHECK(24 == (int) offsetOf(&MMFilesDatafileHeaderMarker::_fid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesDatafileFooterMarker
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_df_footer_marker") {
  size_t s = sizeof(MMFilesDatafileFooterMarker);

  CHECK(16 == (int) s);
  CHECK(s % 8 == 0); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesCollectionHeaderMarker
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_col_header_marker") {
  size_t s = sizeof(MMFilesCollectionHeaderMarker);

  CHECK(16 + 8 == (int) s); // base + own size
  CHECK(s % 8 == 0); 

  CHECK(16 == (int) offsetOf(&MMFilesCollectionHeaderMarker::_cid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesPrologueMarker
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_df_prologue_marker") {
  size_t s = sizeof(MMFilesPrologueMarker);

  CHECK(16 + 16 == (int) s);
  CHECK(s % 8 == 0); 

  CHECK(16 == (int) offsetOf(&MMFilesPrologueMarker::_databaseId));
  CHECK(24 == (int) offsetOf(&MMFilesPrologueMarker::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
