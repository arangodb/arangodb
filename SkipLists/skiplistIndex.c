////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist index
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

#include "skiplistIndex.h"

// ............................................................................
// For now our comparison function simple compares if the memory allocated
// for the data storage is the same.
// Note that this will not necessarily place the two json objects in 
// 'human' order, e.g. may not be in alpha order.
// TODO: 
// (i) Determine the type of the shaped json object
// (ii) If the types match, compare as those types (e.g. integers, strings)
// (iii) If types differ, convert to string and them make the comparision
// ............................................................................

static int CompareShapedJsonShapedJson (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right) {

  int result;
  
  if (left == NULL && right == NULL) {
    return 0;
  }

  if (left == NULL && right != NULL) {
    return -1;
  }

  if (left != NULL && right == NULL) {
    return 1;
  }

  if (left->_data.length < right->_data.length) {
    return -1;
  }
  
  if (left->_data.length > right->_data.length) {
    return 1;
  }
  
  return memcmp(left->_data.data,right->_data.data, left->_data.length);   
}  // end of function CompareShapedJsonShapedJson


// .............................................................................
// Compare two elements and determines:
// left < right   : return -1
// left == right  : return 0
// left > right   : return 1
// .............................................................................
static int CompareElementElement (struct TRI_skiplist_s* skiplist, 
                                  void* leftElement, void* rightElement) {
                                  
  int compareResult;                                    
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
  
  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }    
  
  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }    
  
  if (hLeftElement->numFields < hRightElement->numFields) {
    return -1; // should never happen
  }

  if (hLeftElement->numFields > hRightElement->numFields) {
    return 1; // should never happen
  }

  if (leftElement == rightElement) {
    return 0;
  }    
  
  for (size_t j = 0; j < hLeftElement->numFields; j++) {
  /*
    printf("%s:%u:%f:%f\n",__FILE__,__LINE__,
      *((double*)((j + hLeftElement->fields)->_data.data)),
      *((double*)((j + hRightElement->fields)->_data.data))
    );
    */
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields));
    if (compareResult != 0) {
      return compareResult;
    }
  }
  
  return 0;
}


// .............................................................................
// Compare two elements and determines:
// left < right   : return -1
// left == right  : return 0
// left > right   : return 1
// .............................................................................
static int CompareKeyElement (struct TRI_skiplist_s* skiplist, 
                               void* leftElement, void* rightElement) {
  int compareResult;                               
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
  
  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }    
  
  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }    
  
  if (hLeftElement->numFields < hRightElement->numFields) {
    return -1; // should never happen
  }
  
  if (hLeftElement->numFields > hRightElement->numFields) {
    return 1; // should never happen
  }

  for (size_t j = 0; j < hLeftElement->numFields; j++) {
  /*
    printf("%s:%u:%f:%f\n",__FILE__,__LINE__,
      *((double*)((j + hLeftElement->fields)->_data.data)),
      *((double*)((j + hRightElement->fields)->_data.data))
    );
    */
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields));
    if (compareResult != 0) {
      return compareResult;
    }
  }
  
  return 0;
}



// .............................................................................
// Creates a new skiplist
// .............................................................................

SkiplistIndex* SkiplistIndex_new() {
  SkiplistIndex* skiplistIndex;

  skiplistIndex = TRI_Allocate(sizeof(SkiplistIndex));
  if (skiplistIndex == NULL) {
    return NULL;
  }

  skiplistIndex->unique = true;
  skiplistIndex->skiplist.uniqueSkiplist = TRI_Allocate(sizeof(TRI_skiplist_t));
  if (skiplistIndex->skiplist.uniqueSkiplist == NULL) {
    TRI_Free(skiplistIndex);
    return NULL;
  }    
    
  TRI_InitSkipList(skiplistIndex->skiplist.uniqueSkiplist,
                   sizeof(SkiplistIndexElement),
                   CompareElementElement, 
                   CompareKeyElement,
                   TRI_SKIPLIST_PROB_HALF, 40);
  
  return skiplistIndex;
}


// ...............................................................................
// Adds (inserts) a data element into the skip list
// ...............................................................................

int SkiplistIndex_add(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_InsertElementSkipList(skiplistIndex->skiplist.uniqueSkiplist, element, false);
  if (result) {
    return 0;
  }    
  return -1;
}


// ...............................................................................
// Locates an entry within the unique skiplist
// ...............................................................................

SkiplistIndexElements* SkiplistIndex_find(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  SkiplistIndexElement* result;
  SkiplistIndexElements* results;

  results = TRI_Allocate(sizeof(SkiplistIndexElements));    
  
  result = (SkiplistIndexElement*) (TRI_LookupByElementSkipList(skiplistIndex->skiplist.uniqueSkiplist, element)); 

  if (result != NULL) {
    results->_elements    = TRI_Allocate(sizeof(SkiplistIndexElement) * 1); // unique skiplist index maximum number is 1
    results->_elements[0] = *result;
    results->_numElements = 1;
  }
  else {
    results->_elements    = NULL;
    results->_numElements = 0;
  }
  return results;
}


// ...............................................................................
// An alias for addIndex 
// ...............................................................................

int SkiplistIndex_insert(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  return SkiplistIndex_add(skiplistIndex,element);
} 


// ...............................................................................
// Removes an entry from the skip list
// ...............................................................................

bool SkiplistIndex_remove(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;

  result = TRI_RemoveElementSkipList(skiplistIndex->skiplist.uniqueSkiplist, element, NULL); 
  return result;
}


// ...............................................................................
// updates an entry in the skip list, first removes beforeElement, 
//  then adds the afterElement -- should never be called here
// ...............................................................................

bool SkiplistIndex_update(SkiplistIndex* skiplistIndex, const SkiplistIndexElement* beforeElement, 
                          const SkiplistIndexElement* afterElement) {
  assert(false);
  return false;                      
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-skiplist non-unique skiplist indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Private methods
//------------------------------------------------------------------------------



// .............................................................................
// This is used to determine the equality of elements - not the order.
// .............................................................................
static int CompareMultiElementElement (TRI_skiplist_multi_t* multiSkiplist, 
                                       void* leftElement, void* rightElement) {
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
  int compareResult;                                    
  
  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }    
  
  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }    

  if (hLeftElement->numFields < hRightElement->numFields) {
    return -1; // should never happen
  }
  
  if (hLeftElement->numFields > hRightElement->numFields) {
    return 1; // should never happen
  }
  
  if (hLeftElement->data == hRightElement->data) {
    return 0;
  }    

  /*
    printf("%s:%u:%f:%f\n",__FILE__,__LINE__,
      *((double*)((j + hLeftElement->fields)->_data.data)),
      *((double*)((j + hRightElement->fields)->_data.data))
    );
    */
  
  return -1;  
}


// .............................................................................
// Returns true if the "key" matches that of the element
// .............................................................................
static int  CompareMultiKeyElement (TRI_skiplist_multi_t* multiSkiplist, 
                                    void* leftElement, void* rightElement) {
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
  int compareResult;                                    

  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }    
  
  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }    

  if (hLeftElement->numFields < hRightElement->numFields) {
    return -1; // should never happen
  }
  
  if (hLeftElement->numFields > hRightElement->numFields) {
    return 1; // should never happen
  }
  
  if (hLeftElement->data == hRightElement->data) {
    return 0;
  }    
  
  for (size_t j = 0; j < hLeftElement->numFields; j++) {
  /*
    printf("%s:%u:%f:%f\n",__FILE__,__LINE__,
      *((double*)((j + hLeftElement->fields)->_data.data)),
      *((double*)((j + hRightElement->fields)->_data.data))
    );
    */
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields));
    if (compareResult != 0) {
      return compareResult;
    }
  }
  
  return 0;
}




// .............................................................................
// Creates a new non-uniqe skip list
// .............................................................................

SkiplistIndex* MultiSkiplistIndex_new() {
  SkiplistIndex* skiplistIndex;

  skiplistIndex = TRI_Allocate(sizeof(SkiplistIndex));
  if (skiplistIndex == NULL) {
    return NULL;
  }

  skiplistIndex->unique = false;
  skiplistIndex->skiplist.nonUniqueSkiplist = TRI_Allocate(sizeof(TRI_skiplist_t));
  if (skiplistIndex->skiplist.nonUniqueSkiplist == NULL) {
    TRI_Free(skiplistIndex);
    return NULL;
  }    
    
  TRI_InitSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist,
                        sizeof(SkiplistIndexElement),
                        CompareMultiElementElement,
                        CompareMultiKeyElement, 
                        TRI_SKIPLIST_PROB_HALF, 40);
                   
  return skiplistIndex;
}




// ...............................................................................
// Adds (inserts) a data element into a skip list
// ...............................................................................

int MultiSkiplistIndex_add(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_InsertKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, element, element, false);
  if (result) {
    return 0;
  }    
  return -1;
}


// ...............................................................................
// Locates one or more entries within the non-unique skip list
// ...............................................................................

SkiplistIndexElements* MultiSkiplistIndex_find(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  TRI_vector_pointer_t result;
  SkiplistIndexElements* results;
  size_t j;
  
  results = TRI_Allocate(sizeof(SkiplistIndexElements));    
  
  result  = TRI_LookupByKeySkipListMulti (skiplistIndex->skiplist.nonUniqueSkiplist, element); 

  
  if (result._length == 0) {
    results->_elements    = NULL;
    results->_numElements = 0;
  }
  else {  
    results->_numElements = result._length;
    results->_elements = TRI_Allocate(sizeof(SkiplistIndexElement) * result._length); 
    for (j = 0; j < result._length; ++j) {  
      results->_elements[j] = *((SkiplistIndexElement*)(result._buffer[j]));
    }  
  }
  TRI_DestroyVectorPointer(&result);
  return results;
}


// ...............................................................................
// An alias for addIndex 
// ...............................................................................

int MultiSkiplistIndex_insert(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  return MultiSkiplistIndex_add(skiplistIndex,element);
} 


// ...............................................................................
// Removes an entry from the skiplist
// ...............................................................................

bool MultiSkiplistIndex_remove(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_RemoveElementSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, element, NULL); 
  return result;
}


// ...............................................................................
// updates and entry in the skiplist, first removes beforeElement, 
//  then adds the afterElement. Should never be called from here
// ...............................................................................

bool MultiSkiplistIndex_update(SkiplistIndex* skiplistIndex, SkiplistIndexElement* beforeElement, 
                               SkiplistIndexElement* afterElement) {
  assert(false);
  return false;                      
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


