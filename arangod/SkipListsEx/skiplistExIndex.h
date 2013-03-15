////////////////////////////////////////////////////////////////////////////////
/// @brief unique and non-unique skip lists which support transactions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. O
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_SKIPLIST_EX_INDEX_H
#define TRIAGENS_DURHAM_VOC_BASE_SKIPLIST_EX_INDEX_H 1

#include <BasicsC/common.h>
#include "SkipListsEx/skiplistEx.h"
#include "IndexIterators/index-iterator.h"
#include "IndexOperators/index-operator.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/transaction.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                      skiplistExIndex public types
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup skiplistExIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


typedef struct {
  union {
    TRI_skiplistEx_t* uniqueSkiplistEx;
    TRI_skiplistEx_multi_t* nonUniqueSkiplistEx;
  } _skiplistEx;   
  bool _unique; 
  TRI_transaction_context_t* _transactionContext;
} SkiplistExIndex;


typedef struct {
  size_t numFields;          // the number of fields
  TRI_shaped_json_t* fields; // list of shaped json objects which the collection should know about
  void* data;                // master document pointer
  void* collection;          // pointer to the collection;
} SkiplistExIndexElement;

typedef struct {
  size_t _numElements;
  SkiplistExIndexElement* _elements; // simple list of elements
} SkiplistExIndexElements;  


////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure for skip list. We require a start and stop node
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplistEx_iterator_interval_s {
  void*  _leftEndPoint;
  void*  _rightEndPoint;  
} TRI_skiplistEx_iterator_interval_t;

// ............................................................................
// The iterator essentially is reading a sequence of documents (which are 
// stored in a corresponding sequence of nodes), we require the transaction
// number to which this iterator belongs to, this will ensure that any
// modifications made after this transaction are not 'iterated over'
// ............................................................................

typedef struct TRI_skiplistEx_iterator_s {
  SkiplistExIndex* _index;
  TRI_vector_t _intervals;  
  size_t _currentInterval; // starts with 0
  void* _cursor; // initially null
  uint64_t _thisTransID; // the transaction id to which this iterator belongs to
  bool  (*_hasNext) (struct TRI_skiplistEx_iterator_s*);
  void* (*_next)    (struct TRI_skiplistEx_iterator_s*);
  void* (*_nexts)   (struct TRI_skiplistEx_iterator_s*, int64_t jumpSize);
  bool  (*_hasPrev) (struct TRI_skiplistEx_iterator_s*);
  void* (*_prev)    (struct TRI_skiplistEx_iterator_s*);
  void* (*_prevs)   (struct TRI_skiplistEx_iterator_s*, int64_t jumpSize);  
} TRI_skiplistEx_iterator_t;


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



// -----------------------------------------------------------------------------
// --SECTION--                                   skiplistExIndex  public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup skiplistExIndex
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a skiplistEx iterator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkiplistExIterator (TRI_skiplistEx_iterator_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistExIndex_destroy (SkiplistExIndex*);


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistExIndex_free (SkiplistExIndex*);



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

int SkiplistExIndex_assignMethod (void*, TRI_index_method_assignment_type_e);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Unique skiplist indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

SkiplistExIndex* SkiplistExIndex_new (TRI_transaction_context_t*);

int SkiplistExIndex_add (SkiplistExIndex*, SkiplistExIndexElement*, uint64_t);

TRI_skiplistEx_iterator_t* SkiplistExIndex_find (SkiplistExIndex*, TRI_vector_t*, TRI_index_operator_t*, uint64_t); 

int SkiplistExIndex_insert (SkiplistExIndex*, SkiplistExIndexElement*, uint64_t);

int SkiplistExIndex_remove (SkiplistExIndex*, SkiplistExIndexElement*, uint64_t); 

bool SkiplistExIndex_update (SkiplistExIndex*, const SkiplistExIndexElement*, const SkiplistExIndexElement*, uint64_t);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-skiplist non-unique skiplist indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


SkiplistExIndex* MultiSkiplistExIndex_new (TRI_transaction_context_t*);

int MultiSkiplistExIndex_add (SkiplistExIndex*, SkiplistExIndexElement*, uint64_t);

TRI_skiplistEx_iterator_t* MultiSkiplistExIndex_find (SkiplistExIndex*, TRI_vector_t*, TRI_index_operator_t*, uint64_t); 

int MultiSkiplistExIndex_insert (SkiplistExIndex*, SkiplistExIndexElement*, uint64_t);

int MultiSkiplistExIndex_remove (SkiplistExIndex*, SkiplistExIndexElement*, uint64_t); 

bool MultiSkiplistExIndex_update (SkiplistExIndex*, SkiplistExIndexElement*, SkiplistExIndexElement*, uint64_t);


#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

