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

#include "gtest/gtest.h"

#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesWalMarker.h"

template<typename T, typename U> size_t offsetOf (U T::*member) {
  return (char*) &((T*)nullptr->*member) - (char*) nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof some basic elements
////////////////////////////////////////////////////////////////////////////////

TEST(CStructureSizeTest, tst_basic_elements) {
  EXPECT_TRUE(4 == (int) sizeof(TRI_col_type_e));
  EXPECT_TRUE(1 == (int) sizeof(MMFilesMarkerType));
  EXPECT_TRUE(4 == (int) sizeof(MMFilesDatafileVersionType));
  EXPECT_TRUE(8 == (int) sizeof(TRI_voc_cid_t));
  EXPECT_TRUE(4 == (int) sizeof(TRI_voc_crc_t));
  EXPECT_TRUE(8 == (int) sizeof(TRI_voc_tid_t));
  EXPECT_TRUE(8 == (int) sizeof(TRI_voc_rid_t));
  EXPECT_TRUE(8 == (int) sizeof(TRI_voc_tick_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesMarker
////////////////////////////////////////////////////////////////////////////////

TEST(CStructureSizeTest, tst_df_marker) {
  size_t s = sizeof(MMFilesMarker);

  MMFilesMarker m; 
  EXPECT_TRUE(16 == (int) s);
  EXPECT_TRUE(s % 8 == 0); 

  EXPECT_TRUE(0 == (int) m.offsetOfSize());
  EXPECT_TRUE(4 == (int) m.offsetOfCrc());
  EXPECT_TRUE(8 == (int) m.offsetOfTypeAndTick());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesDatafileHeaderMarker
////////////////////////////////////////////////////////////////////////////////

TEST(CStructureSizeTest, tst_df_header_marker) {
  size_t s = sizeof(MMFilesDatafileHeaderMarker);

  EXPECT_TRUE(16 + 16 == (int) s);
  EXPECT_TRUE(s % 8 == 0); 

  EXPECT_TRUE(16 == (int) offsetOf(&MMFilesDatafileHeaderMarker::_version));
  EXPECT_TRUE(20 == (int) offsetOf(&MMFilesDatafileHeaderMarker::_maximalSize));
  EXPECT_TRUE(24 == (int) offsetOf(&MMFilesDatafileHeaderMarker::_fid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesDatafileFooterMarker
////////////////////////////////////////////////////////////////////////////////

TEST(CStructureSizeTest, tst_df_footer_marker) {
  size_t s = sizeof(MMFilesDatafileFooterMarker);

  EXPECT_TRUE(16 == (int) s);
  EXPECT_TRUE(s % 8 == 0); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesCollectionHeaderMarker
////////////////////////////////////////////////////////////////////////////////

TEST(CStructureSizeTest, tst_col_header_marker) {
  size_t s = sizeof(MMFilesCollectionHeaderMarker);

  EXPECT_TRUE(16 + 8 == (int) s); // base + own size
  EXPECT_TRUE(s % 8 == 0); 

  EXPECT_TRUE(16 == (int) offsetOf(&MMFilesCollectionHeaderMarker::_cid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof MMFilesPrologueMarker
////////////////////////////////////////////////////////////////////////////////

TEST(CStructureSizeTest, tst_df_prologue_marker) {
  size_t s = sizeof(MMFilesPrologueMarker);

  EXPECT_TRUE(16 + 16 == (int) s);
  EXPECT_TRUE(s % 8 == 0); 

  EXPECT_TRUE(16 == (int) offsetOf(&MMFilesPrologueMarker::_databaseId));
  EXPECT_TRUE(24 == (int) offsetOf(&MMFilesPrologueMarker::_collectionId));
}
