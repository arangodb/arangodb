////////////////////////////////////////////////////////////////////////////////
/// @brief index garbage collector
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Anonymous
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


#ifndef TRIAGENS_VOC_BASE_INDEX_GARBAGE_COLLECTOR_H
#define TRIAGENS_VOC_BASE_INDEX_GARBAGE_COLLECTOR_H 1

#include "BasicsC/common.h"
#include "VocBase/vocbase.h"

#ifdef __cplusplus
extern "C" {
#endif


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s;
struct TRI_transaction_context_s;

typedef struct TRI_index_gc_s {
  struct TRI_index_s* _index; // index which requires rubbish collection
  uint8_t _passes;            // the number of passes to complete the rubbish collection
  uint8_t _lastPass;          // the last pass performed (_lastPass = 0, implies no passes performed)
  uint64_t _transID;          // the transaction id which must have completed before the current pass can come into effect
  void* _data;                // storage of data which may be required by the index
  int (*_collectGarbage) (struct TRI_index_gc_s*); // callback which actually does the work (defined where the index is defined)
} TRI_index_gc_t;


int TRI_AddToIndexGC (TRI_index_gc_t*); // adds an item to the rubbish collection linked list

void TRI_IndexGCVocBase (void*); // essentially a loop called by the thread and runs 'forever'

////////////////////////////////////////////////////////////////////////////////
/// @brief index garbage collector event loop
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
