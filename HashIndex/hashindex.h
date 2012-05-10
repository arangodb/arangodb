////////////////////////////////////////////////////////////////////////////////
/// @brief unique hash index
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

#ifndef TRI_HASH_INDEX_H
#define TRI_HASH_INDEX_H 1

#include <BasicsC/common.h>
#include <BasicsC/associative.h>
#include <BasicsC/associative-multi.h>
#include <BasicsC/hashes.h>
#include "ShapedJson/shaped-json.h"
#include "ShapedJson/json-shaper.h"

#include "hasharray.h"

// ...............................................................................
// Define the structure of a unique or non-unique hashindex
// ...............................................................................

typedef struct {
  TRI_hasharray_t* hashArray;   
  bool unique; 
} HashIndex;


typedef struct {
  size_t numFields;          // the number of fields
  TRI_shaped_json_t* fields; // list of shaped json objects the blob of data within will be hashed
  void*  data;               // master document pointer
  void* collection;              
} HashIndexElement;

typedef struct {
  size_t _numElements;
  HashIndexElement* _elements; // simple list of elements
} TRI_hash_index_elements_t;  




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Unique hash indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


void HashIndex_destroy (HashIndex*);

HashIndex* HashIndex_new (void);

void HashIndex_free (HashIndex*);

int HashIndex_add (HashIndex*, HashIndexElement*);

TRI_hash_index_elements_t* HashIndex_find (HashIndex*, HashIndexElement*); 

int HashIndex_insert (HashIndex*, HashIndexElement*);

int HashIndex_remove (HashIndex*, HashIndexElement*); 

int HashIndex_update (HashIndex*, const HashIndexElement*, const HashIndexElement*);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-hash non-unique hash indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void MultiHashIndex_destroy (HashIndex*);

void MultiHashIndex_free (HashIndex*);

HashIndex* MultiHashIndex_new (void);

int MultiHashIndex_add (HashIndex*, HashIndexElement*);

TRI_hash_index_elements_t* MultiHashIndex_find (HashIndex*, HashIndexElement*); 

int MultiHashIndex_insert (HashIndex*, HashIndexElement*);

int MultiHashIndex_remove (HashIndex*, HashIndexElement*); 

int MultiHashIndex_update (HashIndex*, HashIndexElement*, HashIndexElement*);

void MultiHashIndex_free (HashIndex*);

#endif
