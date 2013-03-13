////////////////////////////////////////////////////////////////////////////////
/// @brief bitarray index
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BIT_INDEXES_BITARRAY_INDEX_H
#define TRIAGENS_BIT_INDEXES_BITARRAY_INDEX_H 1

#include "BasicsC/common.h"
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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

