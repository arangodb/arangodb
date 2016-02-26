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

#include <boost/test/unit_test.hpp>

#include "arangod/VocBase/datafile.h"
#include "arangod/VocBase/document-collection.h"
#include "arangod/VocBase/VocShaper.h"
#include "arangod/Wal/Marker.h"

template<typename T, typename U> size_t offsetOf (U T::*member) {
  return (char*) &((T*)nullptr->*member) - (char*) nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CStructureSizeSetup {
  CStructureSizeSetup () {
  }

  ~CStructureSizeSetup () {
  }
};


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CStructureSizeTest, CStructureSizeSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof some basic elements
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_basic_elements) {
  BOOST_CHECK_EQUAL(4, (int) sizeof(TRI_col_type_t));
  BOOST_CHECK_EQUAL(4, (int) sizeof(TRI_df_marker_type_t));
  BOOST_CHECK_EQUAL(4, (int) sizeof(TRI_df_version_t));
  BOOST_CHECK_EQUAL(8, (int) sizeof(TRI_shape_aid_t));
  BOOST_CHECK_EQUAL(8, (int) sizeof(TRI_shape_sid_t));
  BOOST_CHECK_EQUAL(8, (int) sizeof(TRI_shape_size_t));
  BOOST_CHECK_EQUAL(8, (int) sizeof(TRI_voc_cid_t));
  BOOST_CHECK_EQUAL(4, (int) sizeof(TRI_voc_crc_t));
  BOOST_CHECK_EQUAL(8, (int) sizeof(TRI_voc_tid_t));
  BOOST_CHECK_EQUAL(8, (int) sizeof(TRI_voc_rid_t));
  BOOST_CHECK_EQUAL(4, (int) sizeof(TRI_voc_size_t));
  BOOST_CHECK_EQUAL(8, (int) sizeof(TRI_voc_tick_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_marker) {
  size_t s = sizeof(TRI_df_marker_t);

  BOOST_CHECK_EQUAL(24, (int) s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, (int) offsetOf(&TRI_df_marker_t::_size));
  BOOST_CHECK_EQUAL( 4, (int) offsetOf(&TRI_df_marker_t::_crc));
  BOOST_CHECK_EQUAL( 8, (int) offsetOf(&TRI_df_marker_t::_type));
  BOOST_CHECK_EQUAL(16, (int) offsetOf(&TRI_df_marker_t::_tick));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_header_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_header_marker) {
  size_t s = sizeof(TRI_df_header_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&TRI_df_header_marker_t::_version));
  BOOST_CHECK_EQUAL(28, (int) offsetOf(&TRI_df_header_marker_t::_maximalSize));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&TRI_df_header_marker_t::_fid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_footer_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_footer_marker) {
  size_t s = sizeof(TRI_df_footer_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, (int) s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&TRI_df_footer_marker_t::_maximalSize));
  BOOST_CHECK_EQUAL(28, (int) offsetOf(&TRI_df_footer_marker_t::_totalSize));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_col_header_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_col_header_marker) {
  size_t s = sizeof(TRI_col_header_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&TRI_col_header_marker_t::_type));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&TRI_col_header_marker_t::_cid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof database_create_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_database_create_marker) {
  size_t s = sizeof(arangodb::wal::database_create_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::database_create_marker_t::_databaseId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof database_drop_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_database_drop_marker) {
  size_t s = sizeof(arangodb::wal::database_drop_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::database_drop_marker_t::_databaseId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof collection_create_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_collection_create_marker) {
  size_t s = sizeof(arangodb::wal::collection_create_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::collection_create_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&arangodb::wal::collection_create_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof collection_drop_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_collection_drop_marker) {
  size_t s = sizeof(arangodb::wal::collection_drop_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::collection_drop_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&arangodb::wal::collection_drop_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof collection_rename_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_collection_rename_marker) {
  size_t s = sizeof(arangodb::wal::collection_rename_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::collection_rename_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&arangodb::wal::collection_rename_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof collection_change_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_collection_change_marker) {
  size_t s = sizeof(arangodb::wal::collection_change_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::collection_change_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&arangodb::wal::collection_change_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof transaction_begin_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_transaction_begin_marker) {
  size_t s = sizeof(arangodb::wal::transaction_begin_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::transaction_begin_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&arangodb::wal::transaction_begin_marker_t::_transactionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof transaction_abort_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_transaction_abort_marker) {
  size_t s = sizeof(arangodb::wal::transaction_abort_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::transaction_abort_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&arangodb::wal::transaction_abort_marker_t::_transactionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof transaction_commit_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_transaction_commit_marker) {
  size_t s = sizeof(arangodb::wal::transaction_commit_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, (int) s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, (int) offsetOf(&arangodb::wal::transaction_commit_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, (int) offsetOf(&arangodb::wal::transaction_commit_marker_t::_transactionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
