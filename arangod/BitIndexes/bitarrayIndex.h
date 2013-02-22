////////////////////////////////////////////////////////////////////////////////
/// @brief bitarray index
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

#ifndef TRIAGENS_DURHAM_BITINDEXES_BITARRAY_INDEX_H
#define TRIAGENS_DURHAM_BITINDEXES_BITARRAY_INDEX_H 1

#include <BasicsC/common.h>
#include "BitIndexes/bitarray.h"
#include "IndexIterators/index-iterator.h"
#include "IndexOperators/index-operator.h"
#include "ShapedJson/shaped-json.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                        bitarrayIndex public types
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarrayIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


typedef struct {
  TRI_bitarray_t* _bitarray;
  TRI_vector_t _values; // list of json value which are allowed
  bool _supportUndef;
} BitarrayIndex;


typedef struct {
  size_t numFields;          // the number of fields
  TRI_shaped_json_t* fields; // list of shaped json objects which the collection should know about
  void* data;                // master document pointer
  void* collection;          // pointer to the collection;
} BitarrayIndexElement;

typedef struct {
  size_t _numElements;
  BitarrayIndexElement* _elements; // simple list of elements
} BitarrayIndexElements;  




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



// -----------------------------------------------------------------------------
// --SECTION--                                     bitarrayIndex  public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarrayIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a bitarray index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void BitarrayIndex_destroy (BitarrayIndex*);


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a bitarray index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void BitarrayIndex_free (BitarrayIndex*);



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

int BittarrayIndex_assignMethod (void*, TRI_index_method_assignment_type_e);

int BitarrayIndex_new (BitarrayIndex**, TRI_memory_zone_t*, size_t, TRI_vector_t*, bool, void*);

int BitarrayIndex_add (BitarrayIndex*, BitarrayIndexElement*);

TRI_index_iterator_t* BitarrayIndex_find (BitarrayIndex*,  
                                          TRI_index_operator_t*, 
                                          TRI_vector_t*,
                                          void*,
                                          bool (*filter) (TRI_index_iterator_t*) );

                                         
int BitarrayIndex_insert (BitarrayIndex*, BitarrayIndexElement*);

int BitarrayIndex_remove (BitarrayIndex*, BitarrayIndexElement*); 

int BitarrayIndex_update (BitarrayIndex*, const BitarrayIndexElement*, const BitarrayIndexElement*);



#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

