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
#include "arangod/VocBase/primary-collection.h"
#include "arangod/VocBase/voc-shaper.h"

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

    
  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_df_marker_s, _size));
  BOOST_CHECK_EQUAL( 4, offsetof(struct TRI_df_marker_s, _crc));
  BOOST_CHECK_EQUAL( 8, offsetof(struct TRI_df_marker_s, _type));
  BOOST_CHECK_EQUAL(16, offsetof(struct TRI_df_marker_s, _tick));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_header_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_header_marker) {
  size_t s = sizeof(TRI_df_header_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_df_header_marker_s, base));
  BOOST_CHECK_EQUAL(24, offsetof(struct TRI_df_header_marker_s, _version));
  BOOST_CHECK_EQUAL(28, offsetof(struct TRI_df_header_marker_s, _maximalSize));
  BOOST_CHECK_EQUAL(32, offsetof(struct TRI_df_header_marker_s, _fid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_footer_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_footer_marker) {
  size_t s = sizeof(TRI_df_footer_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_df_footer_marker_s, base));
  BOOST_CHECK_EQUAL(24, offsetof(struct TRI_df_footer_marker_s, _maximalSize));
  BOOST_CHECK_EQUAL(28, offsetof(struct TRI_df_footer_marker_s, _totalSize));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_document_marker_t 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_document_marker) {
  size_t s = sizeof(TRI_df_document_marker_t);

  BOOST_CHECK_EQUAL(24, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_df_document_marker_s, base));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_skip_marker_t 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_skip_marker) {
  size_t s = sizeof(TRI_df_skip_marker_t);

  BOOST_CHECK_EQUAL(24, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_df_skip_marker_s, base));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_col_header_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_col_header_marker) {
  size_t s = sizeof(TRI_col_header_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_col_header_marker_s, base));
  BOOST_CHECK_EQUAL(24, offsetof(struct TRI_col_header_marker_s, _type));
  BOOST_CHECK_EQUAL(32, offsetof(struct TRI_col_header_marker_s, _cid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_document_key_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_document_key_marker) {
  size_t s = sizeof(TRI_doc_document_key_marker_t);

  BOOST_CHECK_EQUAL(24 + 32, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_doc_document_key_marker_s, base));
  BOOST_CHECK_EQUAL(24, offsetof(struct TRI_doc_document_key_marker_s, _rid));
  BOOST_CHECK_EQUAL(32, offsetof(struct TRI_doc_document_key_marker_s, _tid));
  BOOST_CHECK_EQUAL(40, offsetof(struct TRI_doc_document_key_marker_s, _shape));
  BOOST_CHECK_EQUAL(48, offsetof(struct TRI_doc_document_key_marker_s, _offsetKey));
  BOOST_CHECK_EQUAL(50, offsetof(struct TRI_doc_document_key_marker_s, _offsetJson));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_edge_key_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_edge_key_marker) {
  size_t s = sizeof(TRI_doc_edge_key_marker_t);

  BOOST_CHECK_EQUAL(24 + 56, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_doc_edge_key_marker_s, base));
  BOOST_CHECK_EQUAL(56, offsetof(struct TRI_doc_edge_key_marker_s, _toCid));
  BOOST_CHECK_EQUAL(64, offsetof(struct TRI_doc_edge_key_marker_s, _fromCid));
  BOOST_CHECK_EQUAL(72, offsetof(struct TRI_doc_edge_key_marker_s, _offsetToKey));
  BOOST_CHECK_EQUAL(74, offsetof(struct TRI_doc_edge_key_marker_s, _offsetFromKey));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_begin_transaction_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_begin_transaction_marker) {
  size_t s = sizeof(TRI_doc_begin_transaction_marker_t);

  BOOST_CHECK_EQUAL(24 + 8 + 8, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_doc_begin_transaction_marker_s, base));
  BOOST_CHECK_EQUAL(24, offsetof(struct TRI_doc_begin_transaction_marker_s, _tid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_commit_transaction_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_commit_transaction_marker) {
  size_t s = sizeof(TRI_doc_commit_transaction_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_doc_commit_transaction_marker_s, base));
  BOOST_CHECK_EQUAL(24, offsetof(struct TRI_doc_commit_transaction_marker_s, _tid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_abort_transaction_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_doc_abort_transaction_marker) {
  size_t s = sizeof(TRI_doc_abort_transaction_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_doc_abort_transaction_marker_s, base));
  BOOST_CHECK_EQUAL(24, offsetof(struct TRI_doc_abort_transaction_marker_s, _tid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_attribute_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_attribute_marker) {
  size_t s = sizeof(TRI_df_attribute_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_df_attribute_marker_s, base));
  BOOST_CHECK_EQUAL(24, offsetof(struct TRI_df_attribute_marker_s, _aid));
  BOOST_CHECK_EQUAL(32, offsetof(struct TRI_df_attribute_marker_s, _size));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_shape_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_shape_marker) {
  size_t s = sizeof(TRI_df_shape_marker_t);

  BOOST_CHECK_EQUAL(24, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

  BOOST_CHECK_EQUAL( 0, offsetof(struct TRI_df_shape_marker_s, base));
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
