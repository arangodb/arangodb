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

#include <Basics/hashes.h>
#include <Basics/json.h>

#include <VocBase/document-collection.h>

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static void RemoveContainerElement (TRI_result_set_t* rs);

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

  TRI_shaped_json_t _element;
  TRI_voc_did_t _did;
  TRI_voc_rid_t _rid;

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
  TRI_voc_rid_t* _rids;
  TRI_json_t* _augmented;

  TRI_voc_size_t _length;
  TRI_voc_size_t _current;
  TRI_voc_size_t _total;
}
doc_rs_vector_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                SINGLE RESULT SETS
// -----------------------------------------------------------------------------

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

static TRI_shaped_json_t* NextRSSingle (TRI_result_set_t* rs,
                                        TRI_voc_did_t* did,
                                        TRI_voc_rid_t* rid,
                                        TRI_json_t const** augmented) {
  doc_rs_single_t* rss;

  rss = (doc_rs_single_t*) rs;

  if (augmented != NULL) {
    *augmented = NULL;
  }

  if (rss->_empty) {
    *did = 0;
    *rid = 0;

    return NULL;
  }
  else {
    rss->_empty = true;
    *did = rss->_did;
    *rid = rss->_rid;

    return &rss->_element;
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
  RemoveContainerElement(rs);

  TRI_Free(rs->_info._cursor);
  TRI_Free(rs);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                VECTOR RESULT SETS
// -----------------------------------------------------------------------------

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

static TRI_shaped_json_t* NextRSVector (TRI_result_set_t* rs,
                                        TRI_voc_did_t* did,
                                        TRI_voc_rid_t* rid,
                                        TRI_json_t const** augmented) {
  doc_rs_vector_t* rss;

  rss = (doc_rs_vector_t*) rs;

  if (rss->_current < rss->_length) {
    *did = rss->_dids[rss->_current];
    *rid = rss->_rids[rss->_current];

    if (augmented != NULL) {
      if (rss->_augmented == NULL) {
        *augmented = NULL;
      }
      else {
        *augmented = &rss->_augmented[rss->_current];
      }
    }

    return &rss->_elements[rss->_current++];
  }
  else {
    *did = 0;
    *rid = 0;

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

  rss = (doc_rs_vector_t*) rs;

  RemoveContainerElement(rs);

  if (rss->_elements != NULL) {
    TRI_Free(rss->_dids);
    TRI_Free(rss->_rids);
    TRI_Free(rss->_elements);
  }

  if (rss->_augmented != NULL) {
    TRI_Free(rss->_augmented);
  }

  TRI_Free(rs->_info._cursor);
  TRI_Free(rs);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       RESULT SETS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a result set from the doubly linked list
////////////////////////////////////////////////////////////////////////////////

static void RemoveContainerElement (TRI_result_set_t* rs) {
  TRI_rs_container_t* container;
  TRI_rs_container_element_t* element;

  element = rs->_containerElement;
  container = element->_container;

  TRI_LockSpin(&container->_lock);

  // element is at the beginning of the chain
  if (element->_prev == NULL) {
    container->_begin = element->_next;
  }
  else {
    element->_prev->_next = element->_next;
  }

  // element is at the end of the chain
  if (element->_next == NULL) {
    container->_end = element->_prev;
  }
  else {
    element->_next->_prev = element->_prev;
  }

  TRI_UnlockSpin(&container->_lock);

  // free the element
  TRI_Free(element);
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
/// @brief initialises a result set container
////////////////////////////////////////////////////////////////////////////////

void TRI_InitRSContainer (TRI_rs_container_t* container, TRI_doc_collection_t* collection) {
  container->_collection = collection;

  TRI_InitSpin(&container->_lock);

  container->_begin = NULL;
  container->_end = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a result set container
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyRSContainer (TRI_rs_container_t* container) {
  TRI_rs_container_element_t* ptr;
  TRI_rs_container_element_t* next;

  ptr = container->_begin;

  while (ptr != NULL) {
    next = ptr->_next;
    ptr->_container = NULL;
    ptr = next;
  }

  TRI_DestroySpin(&container->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a single result set
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_CreateRSSingle (TRI_doc_collection_t* collection,
                                      TRI_rs_container_element_t* containerElement,
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
  rs->base._containerElement = containerElement;

  if (header == NULL) {
    rs->_empty = true;
  }
  else {
    rs->_empty = false;
    rs->_did = header->_did;
    rs->_rid = header->_rid;
    rs->_element = header->_document;
  }

  rs->_total = total;

  return &rs->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a full result set
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_CreateRSVector (TRI_doc_collection_t* collection,
                                      TRI_rs_container_element_t* containerElement,
                                      TRI_doc_mptr_t const** header,
                                      TRI_json_t const* augmented,
                                      TRI_voc_size_t length,
                                      TRI_voc_size_t total) {
  TRI_doc_mptr_t const** qtr;
  TRI_shaped_json_t* end;
  TRI_shaped_json_t* ptr;
  TRI_voc_did_t* wtr;
  TRI_voc_rid_t* ztr;
  doc_rs_vector_t* rs;

  rs = TRI_Allocate(sizeof(doc_rs_vector_t));

  rs->base._error = NULL;

  rs->base.hasNext = HasNextRSVector;
  rs->base.next = NextRSVector;
  rs->base.count = CountRSVector;
  rs->base.free = FreeRSVector;

  rs->base._id = TRI_NewTickVocBase();
  rs->base._containerElement = containerElement;

  if (length == 0) {
    rs->_length = 0;
    rs->_current = 0;
    rs->_dids = NULL;
    rs->_elements = NULL;
    rs->_augmented = NULL;
  }
  else {
    rs->_length = length;
    rs->_current = 0;
    rs->_elements = TRI_Allocate(length * sizeof(TRI_shaped_json_t));
    rs->_dids = TRI_Allocate(length * sizeof(TRI_voc_did_t));
    rs->_rids = TRI_Allocate(length * sizeof(TRI_voc_rid_t));

    qtr = header;
    ptr = rs->_elements;
    wtr = rs->_dids;
    ztr = rs->_rids;
    end = ptr + length;

    for (;  ptr < end;  ++ptr, ++qtr, ++wtr, ++ztr) {
      *wtr = (*qtr)->_did;
      *ztr = (*qtr)->_rid;
      *ptr = (*qtr)->_document;
    }

    if (augmented == NULL) {
      rs->_augmented = NULL;
    }
    else {
      rs->_augmented = TRI_Allocate(length * sizeof(TRI_json_t));
      memcpy(rs->_augmented, augmented, length * sizeof(TRI_json_t));
    }
  }

  rs->_total = total;

  return &rs->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a result set to the end of the doubly linked list
////////////////////////////////////////////////////////////////////////////////

TRI_rs_container_element_t* TRI_AddResultSetRSContainer (TRI_rs_container_t* container) {
  TRI_rs_container_element_t* element;

  element = TRI_Allocate(sizeof(TRI_rs_container_element_t));

  element->_type = TRI_RSCE_RESULT_SET;
  element->_container = container;
  element->_resultSet = NULL;
  element->_datafile = NULL;
  element->datafileCallback = NULL;

  TRI_LockSpin(&container->_lock);

  // empty list
  if (container->_end == NULL) {
    element->_next = NULL;
    element->_prev = NULL;

    container->_begin = element;
    container->_end = element;
  }

  // add to the end
  else {
    element->_next = NULL;
    element->_prev = container->_end;

    container->_end->_next = element;
    container->_end = element;
  }

  TRI_UnlockSpin(&container->_lock);

  return element;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an callback to the result set container
////////////////////////////////////////////////////////////////////////////////

TRI_rs_container_element_t* TRI_AddDatafileRSContainer (TRI_rs_container_t* container,
                                                        TRI_datafile_t* datafile,
                                                        void (*callback) (struct TRI_datafile_s*, void*),
                                                        void* data) {
  TRI_rs_container_element_t* element;

  element = TRI_Allocate(sizeof(TRI_rs_container_element_t));

  element->_type = TRI_RSCE_DATAFILE;
  element->_container = container;
  element->_resultSet = NULL;
  element->_datafile = datafile;
  element->_datafileData = data;
  element->datafileCallback = callback;

  TRI_LockSpin(&container->_lock);

  // empty list
  if (container->_end == NULL) {
    element->_next = NULL;
    element->_prev = NULL;

    container->_begin = element;
    container->_end = element;
  }

  // add to the end
  else {
    element->_next = NULL;
    element->_prev = container->_end;

    container->_end->_next = element;
    container->_end = element;
  }

  TRI_UnlockSpin(&container->_lock);

  return element;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
