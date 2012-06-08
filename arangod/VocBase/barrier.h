////////////////////////////////////////////////////////////////////////////////
/// @brief deletion barriers for datafiles
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_BARRIER_H
#define TRIAGENS_DURHAM_VOC_BASE_BARRIER_H 1

#include "BasicsC/locks.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_collection_s;
struct TRI_collection_s;
struct TRI_datafile_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief barrier element type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_BARRIER_ELEMENT,
  TRI_BARRIER_DATAFILE_CALLBACK,
  TRI_BARRIER_COLLECTION_CALLBACK,
}
TRI_barrier_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief barrier element
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_barrier_s {
  struct TRI_barrier_s* _prev;
  struct TRI_barrier_s* _next;

  struct TRI_barrier_list_s* _container;

  TRI_barrier_type_e _type;

  struct TRI_datafile_s* _datafile;
  void* _datafileData;
  void (*datafileCallback) (struct TRI_datafile_s*, void*);
}
TRI_barrier_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief barrier element datafile callback
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_barrier_datafile_cb_s {
  TRI_barrier_t base;

  struct TRI_datafile_s* _datafile;
  void* _data;
  void (*callback) (struct TRI_datafile_s*, void*);
}
TRI_barrier_datafile_cb_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief barrier element collection callback
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_barrier_collection_cb_s {
  TRI_barrier_t base;

  struct TRI_collection_s* _collection;
  void* _data;
  bool (*callback) (struct TRI_collection_s*, void*);
}
TRI_barrier_collection_cb_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief doubly linked list of barriers
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_barrier_list_s {
  struct TRI_doc_collection_s* _collection;

  TRI_spin_t _lock;

  TRI_barrier_t* _begin;
  TRI_barrier_t* _end;
}
TRI_barrier_list_t;

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
/// @brief initialises a barrier list
////////////////////////////////////////////////////////////////////////////////

void TRI_InitBarrierList (TRI_barrier_list_t* container, struct TRI_doc_collection_s* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a barrier list
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBarrierList (TRI_barrier_list_t* container);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new barrier element
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierElement (TRI_barrier_list_t* container);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new datafile deletion barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierDatafile (TRI_barrier_list_t* container,
                                          struct TRI_datafile_s* datafile,
                                          void (*callback) (struct TRI_datafile_s*, void*),
                                          void* data);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection deletion barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierCollection (TRI_barrier_list_t* container,
                                            struct TRI_collection_s* collection,
                                            bool (*callback) (struct TRI_collection_s*, void*),
                                            void* data);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes and frees a barrier element or datafile deletion marker
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBarrier (TRI_barrier_t* element);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
