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
  BOOST_CHECK_EQUAL(8, sizeof(TRI_voc_cid_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_voc_rid_t));
  BOOST_CHECK_EQUAL(8, sizeof(TRI_voc_tick_t));
  
  BOOST_CHECK_EQUAL(4, sizeof(TRI_voc_crc_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_marker) {
  size_t s = sizeof(TRI_df_marker_t);

  BOOST_CHECK_EQUAL(24, s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 

    
  BOOST_CHECK_EQUAL(0, offsetof(struct TRI_df_marker_s, _size));
  BOOST_CHECK_EQUAL(4, offsetof(struct TRI_df_marker_s, _crc));
  BOOST_CHECK_EQUAL(8, offsetof(struct TRI_df_marker_s, _type));

  // TODO: fix alignment in struct TRI_df_marker_s. it is currently not portable!
  // BOOST_CHECK_EQUAL(16, offsetof(struct TRI_df_marker_s, _tick));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_header_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_header_marker) {
  size_t s = sizeof(TRI_df_header_marker_t);

  BOOST_CHECK_EQUAL(24 + 16, s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_df_footer_marker_t
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_df_footer_marker) {
  size_t s = sizeof(TRI_df_footer_marker_t);

  BOOST_CHECK_EQUAL(24 + 8, s);
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test sizeof TRI_doc_document_key_marker_t 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_document_key_marker) {
  size_t s = sizeof(TRI_doc_document_key_marker_t);

  BOOST_CHECK_EQUAL(24 + 32, s); // base + own size
  BOOST_CHECK_EQUAL(true, s % 8 == 0); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
