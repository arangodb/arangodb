////////////////////////////////////////////////////////////////////////////////
/// @brief result set
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "result-set.h"

#include <BasicsC/hashes.h>
#include <BasicsC/json.h>

#include <VocBase/document-collection.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                SINGLE RESULT SETS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result set storing a single result
////////////////////////////////////////////////////////////////////////////////

typedef struct doc_rs_single_s {
  TRI_result_set_t base;

  bool _empty;

  TRI_rs_entry_t _entry;
  TRI_voc_size_t _total;
}
doc_rs_single_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief empty check for single result sets
////////////////////////////////////////////////////////////////////////////////

static bool HasNextRSSingle (TRI_result_set_t* rs) {
  doc_rs_single_t* rss;

  rss = (doc_rs_single_t*) rs;
  return ! rss->_empty;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current element of a single result set
////////////////////////////////////////////////////////////////////////////////

static TRI_rs_entry_t const* NextRSSingle (TRI_result_set_t* rs) {
  doc_rs_single_t* rss;

  rss = (doc_rs_single_t*) rs;

  if (rss->_empty) {
    return NULL;
  }
  else {
    rss->_empty = true;
    return &rss->_entry;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of elements
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t CountRSSingle (TRI_result_set_t* rs, bool current) {
  doc_rs_single_t* rss;

  rss = (doc_rs_single_t*) rs;
  return rss->_total;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the single result set
////////////////////////////////////////////////////////////////////////////////

static void FreeRSSingle (TRI_result_set_t* rs) {
  TRI_FreeBarrier(rs->_barrier);

  TRI_Free(rs->_info._cursor);
  TRI_Free(rs);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a single result set
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_CreateRSSingle (TRI_doc_collection_t* collection,
                                      TRI_barrier_t* barrier,
                                      TRI_doc_mptr_t const* header,
                                      TRI_voc_size_t total) {
  doc_rs_single_t* rs;

  rs = TRI_Allocate(sizeof(doc_rs_single_t));

  rs->base._error = NULL;

  rs->base.hasNext = HasNextRSSingle;
  rs->base.next = NextRSSingle;
  rs->base.count = CountRSSingle;
  rs->base.free = FreeRSSingle;

  rs->base._id = TRI_NewTickVocBase();
  rs->base._barrier = barrier;

  if (header == NULL) {
    rs->_empty = true;
  }
  else {
    rs->_empty = false;

    rs->_entry._document = header->_document;
    rs->_entry._augmented._type = TRI_JSON_UNUSED;
    rs->_entry._type = ((TRI_df_marker_t*) header->_data)->_type;

    rs->_entry._did = header->_did;
    rs->_entry._rid = header->_rid;

    if (rs->_entry._type == TRI_DOC_MARKER_EDGE) {
      TRI_doc_edge_marker_t* marker = (TRI_doc_edge_marker_t*) header->_data;

      rs->_entry._fromCid = marker->_fromCid;
      rs->_entry._fromDid = marker->_fromDid;
      rs->_entry._toCid = marker->_toCid;
      rs->_entry._toDid = marker->_toDid;
    }
    else {
      rs->_entry._fromCid = 0;
      rs->_entry._fromDid = 0;
      rs->_entry._toCid = 0;
      rs->_entry._toDid = 0;
    }
  }

  rs->_total = total;

  return &rs->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                VECTOR RESULT SETS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result set stored as a vector of document
////////////////////////////////////////////////////////////////////////////////

typedef struct doc_rs_vector_s {
  TRI_result_set_t base;

  TRI_rs_entry_t* _entries;

  TRI_voc_size_t _length;
  TRI_voc_size_t _current;
  TRI_voc_size_t _total;
}
doc_rs_vector_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief empty check for vector result sets
////////////////////////////////////////////////////////////////////////////////

static bool HasNextRSVector (TRI_result_set_t* rs) {
  doc_rs_vector_t* rss;

  rss = (doc_rs_vector_t*) rs;
  return rss->_current < rss->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current element of a vector result set
////////////////////////////////////////////////////////////////////////////////

static TRI_rs_entry_t const* NextRSVector (TRI_result_set_t* rs) {
  doc_rs_vector_t* rss;

  rss = (doc_rs_vector_t*) rs;

  if (rss->_current < rss->_length) {
    return &rss->_entries[rss->_current++];
  }
  else {
    return NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of elements
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t CountRSVector (TRI_result_set_t* rs, bool current) {
  doc_rs_vector_t* rss;

  rss = (doc_rs_vector_t*) rs;

  if (current) {
    return rss->_length;
  }
  else {
    return rss->_total;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the vector result set
////////////////////////////////////////////////////////////////////////////////

static void FreeRSVector (TRI_result_set_t* rs) {
  doc_rs_vector_t* rss;
  TRI_rs_entry_t* ptr;
  TRI_rs_entry_t* end;

  rss = (doc_rs_vector_t*) rs;

  TRI_FreeBarrier(rs->_barrier);

  if (rss->_entries != NULL) {
    ptr = rss->_entries;
    end = rss->_entries + rss->_length;

    for (;  ptr < end;  ++ptr) {
      TRI_DestroyJson(&ptr->_augmented);
    }

    TRI_Free(rss->_entries);
  }

  TRI_Free(rs->_info._cursor);
  TRI_Free(rs);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a full result set
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_CreateRSVector (TRI_doc_collection_t* collection,
                                      TRI_barrier_t* barrier,
                                      TRI_doc_mptr_t const** header,
                                      TRI_json_t const* augmented,
                                      TRI_voc_size_t length,
                                      TRI_voc_size_t total) {
  TRI_doc_mptr_t const** qtr;
  TRI_json_t const* atr;
  TRI_rs_entry_t* end;
  TRI_rs_entry_t* ptr;
  doc_rs_vector_t* rs;

  rs = TRI_Allocate(sizeof(doc_rs_vector_t));

  rs->base._error = NULL;

  rs->base.hasNext = HasNextRSVector;
  rs->base.next = NextRSVector;
  rs->base.count = CountRSVector;
  rs->base.free = FreeRSVector;

  rs->base._id = TRI_NewTickVocBase();
  rs->base._barrier = barrier;

  if (length == 0) {
    rs->_length = 0;
    rs->_current = 0;
    rs->_entries = NULL;
  }
  else {
    TRI_doc_edge_marker_t const* edge;

    rs->_length = length;
    rs->_current = 0;
    rs->_entries = TRI_Allocate(length * sizeof(TRI_rs_entry_t));

    qtr = header;
    atr = augmented;

    ptr = rs->_entries;
    end = ptr + length;

    for (;  ptr < end;  ++ptr, ++qtr, ++atr) {
      edge = (*qtr)->_data;

      (*ptr)._document = (*qtr)->_document;
      (*ptr)._type = edge->base.base._type;

      if (augmented == NULL) {
        (*ptr)._augmented._type = TRI_JSON_UNUSED;
      }
      else {
        (*ptr)._augmented = *atr;
      }

      (*ptr)._did = (*qtr)->_did;
      (*ptr)._rid = (*qtr)->_rid;

      if ((*ptr)._type == TRI_DOC_MARKER_EDGE) {
        (*ptr)._fromCid = edge->_fromCid;
        (*ptr)._fromDid = edge->_fromDid;
        (*ptr)._toCid = edge->_toCid;
        (*ptr)._toDid = edge->_toDid;
      }
    }
  }

  rs->_total = total;

  return &rs->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
