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
#include "arangod/VocBase/voc-shaper.h"
#include "arangod/Wal/Marker.h"

template<typename T, typename U> constexpr size_t offsetOf (U T::*member) {
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
  BOOST_CHECK_EQUAL(4, sizeof(TRI_col_type_t));
  BOOST_CHECK_EQUAL(4, sizeof(TRI_df_marker_type_t));
  BOOST_CHECK_EQUAL(4, sizeof(TRI_df_version_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_shape_aid_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_shape_sid_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_shape_size_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_voc_cid_t));
  BOOST_CHECK_EQUAL(4, sizeof(TRI_voc_crc_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_voc_tid_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_voc_rid_t));
  BOOST_CHECK_EQUAL(4, sizeof(TRI_voc_size_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_voc_tick_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_marker) {
  size_t s = sizeof(TRI_df_marker_t);

  BOOST_CHECK_EQUAL(24, s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetOf(&TRI_df_marker_t::_size));
  BOOST_CHECK_EQUAL( 4, offsetOf(&TRI_df_marker_t::_crc));
  BOOST_CHECK_EQUAL( 8, offsetOf(&TRI_df_marker_t::_type));
  BOOST_CHECK_EQUAL(16, offsetOf(&TRI_df_marker_t::_tick));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_header_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_header_marker) {
  size_t s = sizeof(TRI_df_header_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&TRI_df_header_marker_t::_version));
  BOOST_CHECK_EQUAL(28, offsetOf(&TRI_df_header_marker_t::_maximalSize));
  BOOST_CHECK_EQUAL(32, offsetOf(&TRI_df_header_marker_t::_fid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_footer_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_footer_marker) {
  size_t s = sizeof(TRI_df_footer_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&TRI_df_footer_marker_t::_maximalSize));
  BOOST_CHECK_EQUAL(28, offsetOf(&TRI_df_footer_marker_t::_totalSize));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_col_header_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_col_header_marker) {
  size_t s = sizeof(TRI_col_header_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&TRI_col_header_marker_t::_type));
  BOOST_CHECK_EQUAL(32, offsetOf(&TRI_col_header_marker_t::_cid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof attribute_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_attribute_marker) {
  size_t s = sizeof(triagens::wal::attribute_marker_t);

  BOOST_CHECK_EQUAL(24 + 24, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::attribute_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::attribute_marker_t::_collectionId));
  BOOST_CHECK_EQUAL(40, offsetOf(&triagens::wal::attribute_marker_t::_attributeId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof shape_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_shape_marker) {
  size_t s = sizeof(triagens::wal::shape_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::shape_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::shape_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof database_create_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_database_create_marker) {
  size_t s = sizeof(triagens::wal::database_create_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::database_create_marker_t::_databaseId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof database_drop_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_database_drop_marker) {
  size_t s = sizeof(triagens::wal::database_drop_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::database_drop_marker_t::_databaseId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof collection_create_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_collection_create_marker) {
  size_t s = sizeof(triagens::wal::collection_create_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::collection_create_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::collection_create_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof collection_drop_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_collection_drop_marker) {
  size_t s = sizeof(triagens::wal::collection_drop_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::collection_drop_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::collection_drop_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof collection_rename_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_collection_rename_marker) {
  size_t s = sizeof(triagens::wal::collection_rename_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::collection_rename_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::collection_rename_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof collection_change_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_collection_change_marker) {
  size_t s = sizeof(triagens::wal::collection_change_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::collection_change_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::collection_change_marker_t::_collectionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof transaction_begin_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_transaction_begin_marker) {
  size_t s = sizeof(triagens::wal::transaction_begin_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::transaction_begin_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::transaction_begin_marker_t::_transactionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof transaction_abort_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_transaction_abort_marker) {
  size_t s = sizeof(triagens::wal::transaction_abort_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::transaction_abort_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::transaction_abort_marker_t::_transactionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof transaction_commit_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_transaction_commit_marker) {
  size_t s = sizeof(triagens::wal::transaction_commit_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::transaction_commit_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::transaction_commit_marker_t::_transactionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof document_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_document_marker) {
  size_t s = sizeof(triagens::wal::document_marker_t);

  BOOST_CHECK_EQUAL(24 + 48, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::document_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::document_marker_t::_collectionId));
  BOOST_CHECK_EQUAL(40, offsetOf(&triagens::wal::document_marker_t::_revisionId));
  BOOST_CHECK_EQUAL(48, offsetOf(&triagens::wal::document_marker_t::_transactionId));
  BOOST_CHECK_EQUAL(56, offsetOf(&triagens::wal::document_marker_t::_shape));
  BOOST_CHECK_EQUAL(64, offsetOf(&triagens::wal::document_marker_t::_offsetKey));
  BOOST_CHECK_EQUAL(66, offsetOf(&triagens::wal::document_marker_t::_offsetLegend));
  BOOST_CHECK_EQUAL(68, offsetOf(&triagens::wal::document_marker_t::_offsetJson));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof edge_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_edge_marker) {
  size_t s = sizeof(triagens::wal::edge_marker_t);

  BOOST_CHECK_EQUAL(24 + 48 + 24, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 
  
  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::edge_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::edge_marker_t::_collectionId));
  BOOST_CHECK_EQUAL(40, offsetOf(&triagens::wal::edge_marker_t::_revisionId));
  BOOST_CHECK_EQUAL(48, offsetOf(&triagens::wal::edge_marker_t::_transactionId));
  BOOST_CHECK_EQUAL(56, offsetOf(&triagens::wal::edge_marker_t::_shape));
  BOOST_CHECK_EQUAL(64, offsetOf(&triagens::wal::edge_marker_t::_offsetKey));
  BOOST_CHECK_EQUAL(66, offsetOf(&triagens::wal::edge_marker_t::_offsetLegend));
  BOOST_CHECK_EQUAL(68, offsetOf(&triagens::wal::edge_marker_t::_offsetJson));

  BOOST_CHECK_EQUAL(72, offsetOf(&triagens::wal::edge_marker_t::_toCid));
  BOOST_CHECK_EQUAL(80, offsetOf(&triagens::wal::edge_marker_t::_fromCid));
  BOOST_CHECK_EQUAL(88, offsetOf(&triagens::wal::edge_marker_t::_offsetToKey));
  BOOST_CHECK_EQUAL(90, offsetOf(&triagens::wal::edge_marker_t::_offsetFromKey));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof remove_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_wal_remove_marker) {
  size_t s = sizeof(triagens::wal::remove_marker_t);

  BOOST_CHECK_EQUAL(24 + 32, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&triagens::wal::remove_marker_t::_databaseId));
  BOOST_CHECK_EQUAL(32, offsetOf(&triagens::wal::remove_marker_t::_collectionId));
  BOOST_CHECK_EQUAL(40, offsetOf(&triagens::wal::remove_marker_t::_revisionId));
  BOOST_CHECK_EQUAL(48, offsetOf(&triagens::wal::remove_marker_t::_transactionId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_document_key_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_document_key_marker) {
  size_t s = sizeof(TRI_doc_document_key_marker_t);

  BOOST_CHECK_EQUAL(24 + 32, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&TRI_doc_document_key_marker_t::_rid));
  BOOST_CHECK_EQUAL(32, offsetOf(&TRI_doc_document_key_marker_t::_tid));
  BOOST_CHECK_EQUAL(40, offsetOf(&TRI_doc_document_key_marker_t::_shape));
  BOOST_CHECK_EQUAL(48, offsetOf(&TRI_doc_document_key_marker_t::_offsetKey));
  BOOST_CHECK_EQUAL(50, offsetOf(&TRI_doc_document_key_marker_t::_offsetJson));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_edge_key_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_edge_key_marker) {
  size_t s = sizeof(TRI_doc_edge_key_marker_t);

  BOOST_CHECK_EQUAL(24 + 56, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(56, offsetOf(&TRI_doc_edge_key_marker_t::_toCid));
  BOOST_CHECK_EQUAL(64, offsetOf(&TRI_doc_edge_key_marker_t::_fromCid));
  BOOST_CHECK_EQUAL(72, offsetOf(&TRI_doc_edge_key_marker_t::_offsetToKey));
  BOOST_CHECK_EQUAL(74, offsetOf(&TRI_doc_edge_key_marker_t::_offsetFromKey));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_begin_transaction_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_begin_transaction_marker) {
  size_t s = sizeof(TRI_doc_begin_transaction_marker_t);

  BOOST_CHECK_EQUAL(24 + 8 + 8, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&TRI_doc_begin_transaction_marker_t::_tid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_commit_transaction_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_commit_transaction_marker) {
  size_t s = sizeof(TRI_doc_commit_transaction_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&TRI_doc_commit_transaction_marker_t::_tid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_abort_transaction_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_abort_transaction_marker) {
  size_t s = sizeof(TRI_doc_abort_transaction_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&TRI_doc_abort_transaction_marker_t::_tid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_attribute_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_attribute_marker) {
  size_t s = sizeof(TRI_df_attribute_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL(24, offsetOf(&TRI_df_attribute_marker_t::_aid));
  BOOST_CHECK_EQUAL(32, offsetOf(&TRI_df_attribute_marker_t::_size));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_shape_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_shape_marker) {
  size_t s = sizeof(TRI_df_shape_marker_t);

  BOOST_CHECK_EQUAL(24, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 
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
