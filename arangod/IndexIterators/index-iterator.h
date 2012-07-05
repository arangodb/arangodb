////////////////////////////////////////////////////////////////////////////////
/// @brief index iterator
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

#ifndef TRIAGENS_DURHAM_INDEX_ITERATORS_INDEX_ITERATOR_H
#define TRIAGENS_DURHAM_INDEX_ITERATORS_INDEX_ITERATOR_H 1

#include <BasicsC/common.h>
#include "IndexOperators/index-operator.h"
#include "ShapedJson/shaped-json.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup indexIterator
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure used to store results for indexes
////////////////////////////////////////////////////////////////////////////////


// .............................................................................
// Essentially an interator consists of a sequence of 'intervals'. The iterator
// iterators over each 'interval'. The exact meaning of an 'interval' will 
// vary with the type of index that the iterator is used for. For example,
// in a Skiplist Index, 'intervals' are actual intervals with end points. For
// a Bitarray index there is only one 'interval' which consists of a vector list
// of document handles.
// .............................................................................

typedef struct TRI_index_iterator_interval_s {
  void*  _leftEndPoint;   // typecast to whatever the index requires
  void*  _rightEndPoint;  // typecast to whatever the index requires -- can be NULL
} TRI_index_iterator_interval_t;



// .............................................................................
// The structure of an index iterator
// TODO: for safety sakes whenever we define a structure which we will pass
//       around as an object, add an identifier to the object as the first field
//       within the structure -- this will let everyone one whether or not you
//       have a valid object.
// .............................................................................

typedef struct TRI_index_iterator_s {
  void*        _index;      // the actual index which uses this iterator -- typecast back to the appropriate index structure
  TRI_vector_t _intervals;  // zero or more intervals of the type TRI_index_iterator_interval_t
  size_t _currentInterval;  // the current interval we are operating with
  void* _cursor;            // initially null -- the position within an interval -- typecast to appropriate structure
  void* _currentDocument;   // the result of a call to _next or _prev is stored here.
  
  // ...........................................................................
  // Iteration callback functions:
  //
  // _filter := a function which performs a final filter on a document before
  //            releasing it as available. This function is provided by the 
  //            client using the index. If null, then not filter is performed.
  //
  // _hasNext := returns true if there is a next document WITHOUT advancing the 
  //             iterator.
  // 
  // _next := increments the iterator by 1 and returns a document handle if available.
  //          Will return NULL if no document is available or if the _filter function
  //          above excludes the document.  
  //
  // _nexts := similar to _next, except it advances the iterator by jumpSize
  //
  // _hasPrev := returns true if there is a previous document WITHOUT advancing 
  //             the iterator.
  // 
  // _prev := decrements the iterator by 1 and returns a document handle if available.
  //          Will return NULL if no document is available or if the _filter function
  //          above excludes the document.  
  //
  // _prevs := similar to _prev., except it retreats the iterator by jumpSize.
  //
  // Note that if jumpSize is negative in the functions _nexts and _prevs, then
  // the role of these functions is reversed.
  //
  // Note that these functions are assigned when an interator is returned for
  // an index lookup request. There is NO globally accessible create index 
  // iterator function
  // ...........................................................................

  bool  (*_filter)  (struct TRI_index_iterator_s*);  
  bool  (*_hasNext) (struct TRI_index_iterator_s*);
  void* (*_next)    (struct TRI_index_iterator_s*);
  void* (*_nexts)   (struct TRI_index_iterator_s*, int64_t jumpSize);
  bool  (*_hasPrev) (struct TRI_index_iterator_s*);
  void* (*_prev)    (struct TRI_index_iterator_s*);
  void* (*_prevs)   (struct TRI_index_iterator_s*, int64_t jumpSize);  
  void  (*_reset)   (struct TRI_index_iterator_s*, bool beginning);  
  
  
  // ...........................................................................
  // Index related callback functions:
  //
  // _destroyIterator := since the structure of the 'intervals' is dependent 
  //                     on the type of index used, then this callback will
  //                     correctly deallocate memory to all of the interval
  //                     structures and the cursor structure.
  //
  // Note that these functions are assigned when an interator is returned for
  // an index lookup request. There is NO globally accessible create index 
  // iterator function
  // ...........................................................................

  void  (*_destroyIterator) (struct TRI_index_iterator_s*);
  
} TRI_index_iterator_t;


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup indexIterator
/// @{
////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an index iterator  but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyIndexIterator (TRI_index_iterator_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief Free an index  iterator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndexIterator (TRI_index_iterator_t*);




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

