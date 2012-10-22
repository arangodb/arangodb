////////////////////////////////////////////////////////////////////////////////
/// @brief static hash and comparison functions
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


#include <BasicsC/hashes.h>
#include <ShapedJson/json-shaper.h>
#include <ShapedJson/shaped-json.h>

#define USE_STATIC_HASHARRAY_COMPARE 1

#define HASHARRAY_ELEMENT_TYPE(a,b) \
  struct a {                       \
    TRI_shaped_json_t* fields;     \
    void* data;                    \
  } b
  

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------

static void IndexStaticClearElement (TRI_hasharray_t*, void*);
static bool IndexStaticCopyElementElement (TRI_hasharray_t*, void*, void*);
static void IndexStaticDestroyElement (TRI_hasharray_t*, void*);
static uint64_t IndexStaticHashElement (TRI_hasharray_t*, void*);
static uint64_t IndexStaticHashKey (TRI_hasharray_t*, void*);
static uint64_t IndexStaticHashShapedJson (const uint64_t, const TRI_shaped_json_t*); 
static bool IndexStaticIsEmptyElement (TRI_hasharray_t*, void*);
static bool IndexStaticIsEqualElementElement (TRI_hasharray_t*, void*, void*);
static bool IndexStaticIsEqualElementElementMulti (TRI_hasharray_t*, void*, void*);
static bool IndexStaticIsEqualKeyElement (TRI_hasharray_t*, void*, void*);
static bool IndexStaticIsEqualKeyElementMulti (TRI_hasharray_t*, void*, void*);
static bool IndexStaticIsEqualShapedJsonShapedJson (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right); 



// -----------------------------------------------------------------------------
// Implementation 
// -----------------------------------------------------------------------------



////////////////////////////////////////////////////////////////////////////////
/// @brief marks an element in the hash array as being 'cleared' or 'empty'
////////////////////////////////////////////////////////////////////////////////

static void IndexStaticClearElement(TRI_hasharray_t* array, void* element) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  LocalElement_t* hElement = (LocalElement_t*)(element);
  if (element != NULL) {
    hElement->fields     = 0;
    hElement->data       = 0;
  }  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief deep copies the content of the right item into the left item
////////////////////////////////////////////////////////////////////////////////

static bool IndexStaticCopyElementElement (TRI_hasharray_t* array, void* left, void* right) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  LocalElement_t* leftElement  = (LocalElement_t*)(left);
  LocalElement_t* rightElement = (LocalElement_t*)(right);

  if (leftElement == NULL || rightElement == NULL) {
    assert(0);
    return false;
  }
    
  leftElement->fields     = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * array->_numFields, false);
  if (leftElement->fields == NULL) {
    return false;
  }
  
  leftElement->data       = rightElement->data;

  memcpy(leftElement->fields, rightElement->fields, sizeof(TRI_shaped_json_t) * array->_numFields);
  
  return true;  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an element -- removing any allocated memory within the structure
////////////////////////////////////////////////////////////////////////////////

static void IndexStaticDestroyElement(TRI_hasharray_t* array, void* element) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  // ...........................................................................
  // Each 'field' in the hElement->fields is a TRI_shaped_json_t object, this
  // structure has internal structure of its own -- which also has memory
  // allocated -- HOWEVER we DO NOT deallocate this memory here since it 
  // is actually part of the document structure. This memory should be deallocated
  // when the document has been destroyed.
  // ...........................................................................
  
  LocalElement_t* hElement = (LocalElement_t*)(element);
  if (element != NULL) {
    if (hElement->fields != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, hElement->fields);
    }  
  }  
  IndexStaticClearElement(array, element);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Given an element generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t IndexStaticHashElement (TRI_hasharray_t* array, void* element) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  LocalElement_t* hElement = (LocalElement_t*)(element);
  uint64_t hash = TRI_FnvHashBlockInitial();
  size_t j;

  for (j = 0; j < array->_numFields; j++) {
    hash = IndexStaticHashShapedJson(hash, (j + hElement->fields) );
  }
  return  hash;
}


////////////////////////////////////////////////////////////////////////////////
// Given an element generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t IndexStaticHashKey (TRI_hasharray_t* array, void* element) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);
  
  LocalElement_t* hElement = (LocalElement_t*)(element);
  uint64_t hash = TRI_FnvHashBlockInitial();
  size_t j;

  for (j = 0; j < array->_numFields; j++) {
    hash = IndexStaticHashShapedJson(hash, (j + hElement->fields) );
  }
  return  hash;
}



static uint64_t IndexStaticHashShapedJson (const uint64_t hash, const TRI_shaped_json_t* shapedJson) {
  return TRI_FnvHashBlock(hash, shapedJson->_data.data, shapedJson->_data.length); 
}  // end of function hashShapedJson



// .............................................................................
// returns true if an element in the unique assoc array is 'empty'
// .............................................................................

static bool IndexStaticIsEmptyElement (TRI_hasharray_t* array, void* element) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  LocalElement_t* hElement = (LocalElement_t*)(element);
  if (element != NULL) {
    if (hElement->data == 0) {
      return true;
    }
  }  
  return false;
}



// .............................................................................
// Determines if two elements of the unique assoc array are equal
// Two elements are 'equal' if the document pointer is the same. 
// .............................................................................
static bool IndexStaticIsEqualElementElement (TRI_hasharray_t* array, void* leftElement, void* rightElement) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  return (hLeftElement->data == hRightElement->data);
}



// .............................................................................
// Returns true if document pointers are the same, otherwise returns false
// .............................................................................
static bool IndexStaticIsEqualElementElementMulti (TRI_hasharray_t* array, void* leftElement, void* rightElement) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  return (hLeftElement->data == hRightElement->data); 
}


// .............................................................................
// Determines if two elements of the unique assoc array are equal
// Two elements are 'equal' if the shaped json content is equal. 
// .............................................................................
static bool IndexStaticIsEqualKeyElement (TRI_hasharray_t* array, void* leftElement, void* rightElement) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);
  
  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);
  size_t j;
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  for (j = 0; j < array->_numFields; j++) {
    if (!IndexStaticIsEqualShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields))) {
      return false;
    }
  }
  
  return true;
}


// .............................................................................
// Returns true if the "key" matches that of the element
// .............................................................................
static bool IndexStaticIsEqualKeyElementMulti (TRI_hasharray_t* array, void* leftElement, void* rightElement) {
  typedef HASHARRAY_ELEMENT_TYPE(LocalElement_s,LocalElement_t);
  
  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);
  size_t j;
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  for (j = 0; j < array->_numFields; j++) {
    TRI_shaped_json_t* left   = (j + hLeftElement->fields);
    TRI_shaped_json_t* right  = (j + hRightElement->fields);
    if (!IndexStaticIsEqualShapedJsonShapedJson(left, right)) {
      return false;
    }
  }
  
  return true;
}


static bool IndexStaticIsEqualShapedJsonShapedJson (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right) {
  // if (left == NULL && right == NULL) {
  //   return true;
  // }

  // if (left == NULL && right != NULL) {
  //   return false;
  // }

  // if (left != NULL && right == NULL) {
  //   return false;
  // }
 
  // simplified the above checks to 
  if (left == NULL) {
    return (right == NULL);
  }

  if (right == NULL) {
    return false;
  }

  if (left->_data.length != right->_data.length) {
    return false;
  }

  return (memcmp(left->_data.data, right->_data.data, left->_data.length) == 0);   
}  // end of function isEqualShapedJsonShapedJson




