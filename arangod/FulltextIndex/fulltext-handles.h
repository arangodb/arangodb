////////////////////////////////////////////////////////////////////////////////
/// @brief full text search, handles
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_FULLTEXT_INDEX_FULLTEXT_HANDLES_H
#define TRIAGENS_FULLTEXT_INDEX_FULLTEXT_HANDLES_H 1

#include "fulltext-common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a fulltext handle entry
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_fulltext_handle_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief a slot containing _numUsed handles and has some statistics about
/// itself
///
/// the fulltext index will not store document ids in its nodes, because that
/// will be complicated in the case of deleting a document. in this case, all
/// nodes would need to be traversed to find where the document was referenced.
/// this would be too slow. instead of storing document ids, a node stores
/// handles. handles are increasing integer numbers that are each mapped to a
/// specific document. when a document is deleted from the index, its handle is
/// marked as deleted, but the handle value may remain stored in one or many
/// index nodes. handles of deleted documents are removed from result sets at
/// the end of each index query on-the-fly, so query results are still correct.
/// To finally get rid of handles of deleted documents, the index can perform
/// a compaction. The compaction rewrites a new, dense handle list consisting
/// with only handles that point to existing documents. The old handles used in
/// nodes become invalid by this, so the handles stores in the nodes have to
/// be rewritten. When the rewrite is done, the old handle list is freed and
/// the new one is put in place.
///
/// Inserting a new document will simply allocate a new handle, and the handle
/// will be stored for the node. We simply assign the next handle number for
/// the document. After that, we can quickly look up the document id for a
/// handle value. It's more tricky the other way around, because there is no
/// simple mapping from document ids to handles. To find the handle for a
/// document id, we have to check all handles already used.
/// As this would mean traversing over all handles used and comparing their
/// document values with the sought document id, there is some optimisation:
/// handles are stored in slots of fixed sizes. Each slot has some statistics
/// about the number of used and deleted documents/handles in it, as well as
/// its min and max document values.
/// When looking for a specific document id in all handles in the case of
/// deletion, the slot statistics are used to early prune non-relevant slots from
/// the further search. The simple min/max document id check implemented is
/// sufficient because normally document memory is contiguous so the pointers
/// to documents are just adjacent (second pointer is higher than first pointer).
/// This is only true for documents that are created on the same memory page
/// but this should be the common case to optimise for.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_fulltext_handle_slot_s {
  uint32_t                     _numUsed;     // number of handles used in slot
  uint32_t                     _numDeleted;  // number of deleted handles in slot
  TRI_fulltext_doc_t           _min;         // minimum handle value in slot
  TRI_fulltext_doc_t           _max;         // maximum handle value in slot
  TRI_fulltext_doc_t*          _documents;   // document ids for the slots
  uint8_t*                     _deleted;     // deleted flags for the slots
}
TRI_fulltext_handle_slot_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a fulltext handles instance
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_fulltext_handles_s {
  TRI_fulltext_handle_t        _next;        // next handle to use
  uint32_t                     _numSlots;    // current number of slots
  TRI_fulltext_handle_slot_t** _slots;       // pointers to slots
  uint32_t                     _slotSize;    // the size of each slot
  uint32_t                     _numDeleted;  // total number of deleted documents
  TRI_fulltext_handle_t*       _map;         // a temporary map for remapping existing
                                             // handles to new handles during compaction
}
TRI_fulltext_handles_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a handles instance
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_handles_t* TRI_CreateHandlesFulltextIndex (const uint32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a handles instance
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHandlesFulltextIndex (TRI_fulltext_handles_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get number of documents (including deleted)
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_NumHandlesHandleFulltextIndex (TRI_fulltext_handles_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief get number of deleted documents
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_NumDeletedHandleFulltextIndex (TRI_fulltext_handles_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief get handle list fill grade
////////////////////////////////////////////////////////////////////////////////

double TRI_DeletionGradeHandleFulltextIndex (TRI_fulltext_handles_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the handle list should be compacted
////////////////////////////////////////////////////////////////////////////////

bool TRI_ShouldCompactHandleFulltextIndex (TRI_fulltext_handles_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief compact the handle list
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_handles_t* TRI_CompactHandleFulltextIndex (TRI_fulltext_handles_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a document and return a handle for it
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_handle_t TRI_InsertHandleFulltextIndex (TRI_fulltext_handles_t* const,
                                                     const TRI_fulltext_doc_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a document as deleted in the handle list
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteDocumentHandleFulltextIndex (TRI_fulltext_handles_t* const,
                                            const TRI_fulltext_doc_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the document id for a handle
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_doc_t TRI_GetDocumentFulltextIndex (const TRI_fulltext_handles_t* const,
                                                 const TRI_fulltext_handle_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump all handles
////////////////////////////////////////////////////////////////////////////////

#if TRI_FULLTEXT_DEBUG
void TRI_DumpHandleFulltextIndex (TRI_fulltext_handles_t* const);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory usage for the handles
////////////////////////////////////////////////////////////////////////////////

size_t TRI_MemoryHandleFulltextIndex (const TRI_fulltext_handles_t* const);

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
