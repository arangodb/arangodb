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
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "result-set.h"

#include <Basics/hashes.h>
#include <Basics/json.h>

#include <VocBase/document-collection.h>

// -----------------------------------------------------------------------------
// --SECTION--                                     private types for result sets
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result set storing a single result
////////////////////////////////////////////////////////////////////////////////

typedef struct doc_rs_single_s {
  TRI_result_set_t base;

  bool _empty;

  TRI_shaped_json_t _element;
  TRI_voc_did_t _did;

  TRI_voc_size_t _total;
}
doc_rs_single_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief result set stored as a vector of document
////////////////////////////////////////////////////////////////////////////////

typedef struct doc_rs_vector_s {
  TRI_result_set_t base;

  TRI_shaped_json_t* _elements;
  TRI_voc_did_t* _dids;

  TRI_voc_size_t _length;
  TRI_voc_size_t _current;
  TRI_voc_size_t _total;
}
doc_rs_vector_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                          private functions for single result sets
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief empty check for single result sets
////////////////////////////////////////////////////////////////////////////////

static bool HasNextRSSingle (TRI_result_set_t* rs) {
  doc_rs_single_t* rss = (doc_rs_single_t*) rs;

  return ! rss->_empty;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current element of a single result set
////////////////////////////////////////////////////////////////////////////////

static TRI_shaped_json_t* NextRSSingle (TRI_result_set_t* rs, TRI_voc_did_t* did) {
  doc_rs_single_t* rss = (doc_rs_single_t*) rs;

  if (rss->_empty) {
    *did = 0;

    return NULL;
  }
  else {
    rss->_empty = true;
    *did = rss->_did;

    return &rss->_element;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of elements
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t CountRSSingle (TRI_result_set_t* rs, bool current) {
  doc_rs_single_t* rss = (doc_rs_single_t*) rs;

  return rss->_total;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the single result set
////////////////////////////////////////////////////////////////////////////////

static void FreeRSSingle (TRI_result_set_t* rs) {
  void* f;

  f = TRI_RemoveKeyAssociativeSynced(&rs->_container->_resultSets, &rs->_id);
  assert(f != NULL);

  TRI_Free(rs->_info._cursor);
  TRI_Free(rs);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                          private functions for vector result sets
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief empty check for vector result sets
////////////////////////////////////////////////////////////////////////////////

static bool HasNextRSVector (TRI_result_set_t* rs) {
  doc_rs_vector_t* rss = (doc_rs_vector_t*) rs;

  return rss->_current < rss->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current element of a vector result set
////////////////////////////////////////////////////////////////////////////////

static TRI_shaped_json_t* NextRSVector (TRI_result_set_t* rs, TRI_voc_did_t* did) {
  doc_rs_vector_t* rss = (doc_rs_vector_t*) rs;

  if (rss->_current < rss->_length) {
    *did = rss->_dids[rss->_current];

    return &rss->_elements[rss->_current++];
  }
  else {
    *did = 0;

    return NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of elements
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t CountRSVector (TRI_result_set_t* rs, bool current) {
  doc_rs_vector_t* rss = (doc_rs_vector_t*) rs;

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
  doc_rs_vector_t* rss = (doc_rs_vector_t*) rs;

  void* f;

  if (rss->_elements != NULL) {
    TRI_Free(rss->_dids);
    TRI_Free(rss->_elements);
  }

  f = TRI_RemoveKeyAssociativeSynced(&rs->_container->_resultSets, &rs->_id);
  assert(f != NULL);

  TRI_Free(rs->_info._cursor);
  TRI_Free(rs);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                 private functions for result sets
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the result set id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyRS (TRI_associative_synced_t* array, void const* key) {
  TRI_rs_id_t const* k = key;

  return TRI_FnvHashPointer(k, sizeof(TRI_rs_id_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the result set id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementRS (TRI_associative_synced_t* array, void const* element) {
  TRI_result_set_t const* e = element;

  return TRI_FnvHashPointer(&e->_id, sizeof(TRI_rs_id_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection id and a collection
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyRS (TRI_associative_synced_t* array, void const* key, void const* element) {
  TRI_rs_id_t const* k = key;
  TRI_result_set_t const* e = element;

  return *k == e->_id;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a result set container
////////////////////////////////////////////////////////////////////////////////

void TRI_InitRSContainer (TRI_rs_container_t* container, TRI_doc_collection_t* collection) {
  container->_collection = collection;

  TRI_InitAssociativeSynced(&container->_resultSets,
                            HashKeyRS,
                            HashElementRS,
                            EqualKeyRS,
                            NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a result set container
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyRSContainer (TRI_rs_container_t* container) {
  TRI_DestroyAssociativeSynced(&container->_resultSets);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a single result set
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_CreateRSSingle (TRI_doc_collection_t* collection,
                                      TRI_doc_mptr_t const* header,
                                      TRI_voc_size_t total) {
  doc_rs_single_t* rs;

  rs = TRI_Allocate(sizeof(doc_rs_single_t));

  rs->base.hasNext = HasNextRSSingle;
  rs->base.next = NextRSSingle;
  rs->base.count = CountRSSingle;
  rs->base.free = FreeRSSingle;

  rs->base._id = TRI_NewTickVocBase();
  rs->base._container = &collection->_resultSets;

  if (header == NULL) {
    rs->_empty = true;
  }
  else {
    rs->_empty = false;
    rs->_did = header->_did;
    rs->_element = header->_document;
  }

  rs->_total = total;

  TRI_InsertKeyAssociativeSynced(&collection->_resultSets._resultSets, &rs->base._id, rs);

  return &rs->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a full result set
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_CreateRSVector (TRI_doc_collection_t* collection,
                                      TRI_doc_mptr_t const** header,
                                      TRI_voc_size_t length,
                                      TRI_voc_size_t total) {
  doc_rs_vector_t* rs;
  TRI_doc_mptr_t const** qtr;
  TRI_shaped_json_t* ptr;
  TRI_shaped_json_t* end;
  TRI_voc_did_t* wtr;

  rs = TRI_Allocate(sizeof(doc_rs_vector_t));

  rs->base.hasNext = HasNextRSVector;
  rs->base.next = NextRSVector;
  rs->base.count = CountRSVector;
  rs->base.free = FreeRSVector;

  rs->base._id = TRI_NewTickVocBase();
  rs->base._container = &collection->_resultSets;

  if (length == 0) {
    rs->_length = 0;
    rs->_current = 0;
    rs->_dids = NULL;
    rs->_elements = NULL;
  }
  else {
    rs->_length = length;
    rs->_current = 0;
    rs->_elements = TRI_Allocate(length * sizeof(TRI_shaped_json_t));
    rs->_dids = TRI_Allocate(length * sizeof(TRI_voc_did_t));

    qtr = header;
    ptr = rs->_elements;
    wtr = rs->_dids;
    end = ptr + length;

    for (;  ptr < end;  ++ptr, ++qtr, ++wtr) {
      *wtr = (*qtr)->_did;
      *ptr = (*qtr)->_document;
    }
  }

  rs->_total = total;

  TRI_InsertKeyAssociativeSynced(&collection->_resultSets._resultSets, &rs->base._id, rs);

  return &rs->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
