////////////////////////////////////////////////////////////////////////////////
/// @brief compare methods used for skiplist indexes
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_SKIPLIST_COMPARE_H
#define TRIAGENS_DURHAM_VOC_BASE_SKIPLIST_COMPARE_H 1

#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"

#define USE_STATIC_SKIPLIST_COMPARE 1

#define SKIPLIST_ELEMENT_TYPE(a,b) \
  struct a {                       \
    size_t numFields;              \
    TRI_shaped_json_t* fields;     \
    void* data;                    \
    void* collection;              \
  } b
  

#ifdef __cplusplus
extern "C" {
#endif


static int IndexStaticCopyElementElement (TRI_skiplist_base_t* skiplist, void* left, void* right) {
  typedef SKIPLIST_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  LocalElement_t* leftElement  = (LocalElement_t*)(left);
  LocalElement_t* rightElement = (LocalElement_t*)(right);

  if (leftElement == NULL || rightElement == NULL) {
    assert(0);
    return TRI_ERROR_INTERNAL;
  }
    
  leftElement->numFields  = rightElement->numFields;
  leftElement->data       = rightElement->data;
  leftElement->collection = rightElement->collection;
  leftElement->fields     = TRI_Allocate(sizeof(TRI_shaped_json_t) * leftElement->numFields);
  
  if (leftElement->fields == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  memcpy(leftElement->fields, rightElement->fields, sizeof(TRI_shaped_json_t) * leftElement->numFields);
  
  return TRI_ERROR_NO_ERROR;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an element -- removing any allocated memory within the structure
////////////////////////////////////////////////////////////////////////////////

static void IndexStaticDestroyElement(TRI_skiplist_base_t* skiplist, void* element) {
  typedef SKIPLIST_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

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
      TRI_Free(hElement->fields);
    }  
  }  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Helper method for recursion for CompareShapedJsonShapedJson
////////////////////////////////////////////////////////////////////////////////


// .............................................................................
// left < right  return -1
// left > right  return  1
// left == right return  0
// .............................................................................

static int CompareShapeTypes (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right, TRI_shaper_t* leftShaper, TRI_shaper_t* rightShaper) {
  
  int result;
  size_t j;
  TRI_shape_type_t leftType;
  TRI_shape_type_t rightType;
  const TRI_shape_t* leftShape;
  const TRI_shape_t* rightShape;
  size_t leftListLength;
  size_t rightListLength;
  size_t listLength;
  TRI_shaped_json_t leftElement;
  TRI_shaped_json_t rightElement;
  char* leftString;
  char* rightString;
  
  
  leftShape  = leftShaper->lookupShapeId(leftShaper, left->_sid);
  rightShape = rightShaper->lookupShapeId(rightShaper, right->_sid);
  leftType   = leftShape->_type;
  rightType  = rightShape->_type;
  
  switch (leftType) {
  
    case TRI_SHAPE_ILLEGAL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: {
          return 0;
        }
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } 

    case TRI_SHAPE_NULL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        {
          return 1;
        }
        case TRI_SHAPE_NULL:
        {
          return 0;
        }
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType) 
    } 

    case TRI_SHAPE_BOOLEAN: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        {
          return 1;
        }
        case TRI_SHAPE_BOOLEAN:
        {
          // check which is false and which is true!
          if ( *((TRI_shape_boolean_t*)(left->_data.data)) == *((TRI_shape_boolean_t*)(right->_data.data)) ) {
            return 0;          
          }  
          if ( *((TRI_shape_boolean_t*)(left->_data.data)) < *((TRI_shape_boolean_t*)(right->_data.data)) ) {
            return -1;          
          }  
          return 1;
        }
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType) 
    } 
    
    case TRI_SHAPE_NUMBER: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        {
          return 1;
        }
        case TRI_SHAPE_NUMBER:
        {
          // compare the numbers.
          if ( *((TRI_shape_number_t*)(left->_data.data)) == *((TRI_shape_number_t*)(right->_data.data)) ) {
            return 0;          
          }  
          if ( *((TRI_shape_number_t*)(left->_data.data)) < *((TRI_shape_number_t*)(right->_data.data)) ) {
            return -1;          
          }  
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType) 
    } 
    
    case TRI_SHAPE_SHORT_STRING: 
    case TRI_SHAPE_LONG_STRING: 
    {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        {
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        {
          // compare strings
          // extract the strings
          if (leftType == TRI_SHAPE_SHORT_STRING) {
            leftString = (char*)(sizeof(TRI_shape_length_short_string_t) + left->_data.data);
          }
          else {
            leftString = (char*)(sizeof(TRI_shape_length_long_string_t) + left->_data.data);
          }          
          
          if (rightType == TRI_SHAPE_SHORT_STRING) {
            rightString = (char*)(sizeof(TRI_shape_length_short_string_t) + right->_data.data);
          }
          else {
            rightString = (char*)(sizeof(TRI_shape_length_long_string_t) + right->_data.data);
          }         
          
          result = strcmp(leftString,rightString);          
          return result;
        }
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType) 
    } 
    
    case TRI_SHAPE_HOMOGENEOUS_LIST: 
    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
    case TRI_SHAPE_LIST:
    {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        {
          return 1;
        }
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
        case TRI_SHAPE_LIST:
        {
          // unfortunately recursion: check the types of all the entries
          leftListLength  = *((TRI_shape_length_list_t*)(left->_data.data));
          rightListLength = *((TRI_shape_length_list_t*)(right->_data.data));
          
          // determine the smallest list
          if (leftListLength > rightListLength) {
            listLength = rightListLength;
          }
          else {
            listLength = leftListLength;
          }
          
          for (j = 0; j < listLength; ++j) {
          
            if (leftType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*)(leftShape),
                                              left,j,&leftElement);
            }            
            else if (leftType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*)(leftShape),
                                                   left,j,&leftElement);
            }
            else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(leftShape),left,j,&leftElement);
            }
            
            
            if (rightType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*)(rightShape),
                                              right,j,&rightElement);
            }            
            else if (rightType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*)(rightShape),
                                                   right,j,&rightElement);
            }
            else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(rightShape),right,j,&rightElement);
            }
            
            result = CompareShapeTypes (&leftElement, &rightElement, leftShaper, rightShaper);
            if (result != 0) { 
              return result;
            }  
          }          
          
          // up to listLength everything matches
          if (leftListLength < rightListLength) {
            return -1;
          }
          else if (leftListLength > rightListLength) {
            return 1;
          }  
          return 0;
        }
        
        
        case TRI_SHAPE_ARRAY:
        {
          return -1;
        }
      } // end of switch (rightType) 
    } 
    
    case TRI_SHAPE_ARRAY:
    {
      /* start oreste: 
        char* shape = (char*)(leftShape);
        uint64_t fixedEntries;
        uint64_t variableEntries;
        uint64_t ssid;
        uint64_t aaid;
        char* name;
        TRI_shape_t* newShape;
        
        shape = shape + sizeof(TRI_shape_t);        
        fixedEntries = *((TRI_shape_size_t*)(shape));
        shape = shape + sizeof(TRI_shape_size_t);
        variableEntries = *((TRI_shape_size_t*)(shape));
        shape = shape + sizeof(TRI_shape_size_t);
        ssid = *((TRI_shape_sid_t*)(shape));
        shape = shape + (sizeof(TRI_shape_sid_t) * (fixedEntries + variableEntries));
        aaid = *((TRI_shape_aid_t*)(shape));
        shape = shape + (sizeof(TRI_shape_aid_t) * (fixedEntries + variableEntries));
        
        name      = leftShaper->lookupAttributeId(leftShaper,aaid);
        newShape  = leftShaper->lookupShapeId(leftShaper, ssid);

        
        printf("%s:%u:_fixedEntries:%u\n",__FILE__,__LINE__,fixedEntries);
        printf("%s:%u:_variableEntries:%u\n",__FILE__,__LINE__,variableEntries);
        printf("%s:%u:_sids[0]:%u\n",__FILE__,__LINE__,ssid);
        printf("%s:%u:_aids[0]:%u\n",__FILE__,__LINE__,aaid);
        printf("%s:%u:name:%s\n",__FILE__,__LINE__,name);
        printf("%s:%u:type:%d\n",__FILE__,__LINE__,newShape->_type);
                         
       end oreste */
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
        case TRI_SHAPE_LIST:
        {
          return 1;
        }
        case TRI_SHAPE_ARRAY:
        {
          
          assert(false);
          result = 0;
          return result;
        }
      } // end of switch (rightType) 
    } 
    
  }
  assert(false);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Compare a shapded json object recursively if necessary
////////////////////////////////////////////////////////////////////////////////

static int CompareShapedJsonShapedJson (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right, TRI_shaper_t* leftShaper, TRI_shaper_t* rightShaper) {

  int result;
  
  // ............................................................................
  // the following order is currently defined for placing an order on documents
  // undef < null < boolean < number < strings < lists < hash arrays
  // note: undefined will be treated as NULL pointer not NULL JSON OBJECT
  // within each type class we have the following order
  // boolean: false < true
  // number: natural order
  // strings: lexicographical
  // lists: lexicorgraphically and within each slot according to these rules.
  // ............................................................................


  if (left == NULL && right == NULL) {
    return 0;
  }

  if (left == NULL && right != NULL) {
    return -1;
  }

  if (left != NULL && right == NULL) {
    return 1;
  }
    
  result = CompareShapeTypes (left, right, leftShaper, rightShaper);

  return result;
  
}  // end of function CompareShapedJsonShapedJson


////////////////////////////////////////////////////////////////////////////////
/// @brief compares two elements in a skip list
////////////////////////////////////////////////////////////////////////////////

static int IndexStaticCompareElementElement (struct TRI_skiplist_s* skiplist, void* leftElement, void* rightElement, int defaultEqual) {                                
  typedef SKIPLIST_ELEMENT_TYPE(LocalElement_s,LocalElement_t);
  // .............................................................................
  // Compare two elements and determines:
  // left < right   : return -1
  // left == right  : return 0
  // left > right   : return 1
  // .............................................................................
  int compareResult;                                    
  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;
  
  // ............................................................................
  // the following order is currently defined for placing an order on documents
  // undef < null < boolean < number < strings < lists < hash arrays
  // note: undefined will be treated as NULL pointer not NULL JSON OBJECT
  // within each type class we have the following order
  // boolean: false < true
  // number: natural order
  // strings: lexicographical
  // lists: lexicographically and within each slot according to these rules.
  // ............................................................................
  
  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }    
  
  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }    

  if (leftElement == rightElement) {
    return 0;
  }    
  
  // ............................................................................
  // This call back function is used when we insert and remove unique skip
  // list entries. 
  // ............................................................................
  
  if (hLeftElement->numFields != hRightElement->numFields) {
    assert(false);
  }
  
  
  // ............................................................................
  // The document could be the same -- so no further comparison is required.
  // ............................................................................
  if (hLeftElement->data == hRightElement->data) {
    return 0;
  }    

  
  leftShaper  = ((TRI_doc_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_doc_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < hLeftElement->numFields; j++) {
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields), leftShaper, rightShaper);
    if (compareResult != 0) {
      return compareResult;
    }
  }

  // ............................................................................
  // This is where the difference between CompareKeyElement (below) and 
  // CompareElementElement comes into play. Here if the 'keys' are the same,
  // but the doc ptr is different (which it is since we are here), then
  // we return what was requested to be returned: 0,-1 or 1. What is returned
  // depends on the purpose of calling this callback.
  // ............................................................................
   
  return defaultEqual;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key and an element
////////////////////////////////////////////////////////////////////////////////

static int IndexStaticCompareKeyElement (struct TRI_skiplist_s* skiplist, void* leftElement, void* rightElement, int defaultEqual) {
  typedef SKIPLIST_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  // .............................................................................
  // Compare two elements and determines:
  // left < right   : return -1
  // left == right  : return 0
  // left > right   : return 1
  // .............................................................................
  int compareResult;                               
  size_t numFields;
  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;
  
  // ............................................................................
  // the following order is currently defined for placing an order on documents
  // undef < null < boolean < number < strings < lists < hash arrays
  // note: undefined will be treated as NULL pointer not NULL JSON OBJECT
  // within each type class we have the following order
  // boolean: false < true
  // number: natural order
  // strings: lexicographical
  // lists: lexicorgraphically and within each slot according to these rules.
  // associative array: ordered keys followed by value of key
  // ............................................................................
  
  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }    
  
  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }    

  if (leftElement == rightElement) {
    return 0;
  }    
    
  // ............................................................................
  // The document could be the same -- so no further comparison is required.
  // ............................................................................
  if (hLeftElement->data == hRightElement->data) {
    return 0;
  }    
  
  // ............................................................................
  // This call back function is used when we query the index, as such
  // the number of fields which we are using for the query may be less than
  // the number of fields that the index is defined with.
  // ............................................................................

  
  if (hLeftElement->numFields < hRightElement->numFields) {
    numFields = hLeftElement->numFields;
  }
  else {
    numFields = hRightElement->numFields;
  }
  
  
  leftShaper  = ((TRI_doc_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_doc_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
  /*
  printf("%s:%u:%f:%f,%u:%u\n",__FILE__,__LINE__,
    *((double*)((j + hLeftElement->fields)->_data.data)),
    *((double*)((j + hRightElement->fields)->_data.data)),
    (uint64_t)(hLeftElement->data),
    (uint64_t)(hRightElement->data)
  );
  */
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), 
                                                (j + hRightElement->fields),
                                                leftShaper,
                                                rightShaper);
    if (compareResult != 0) {
      return compareResult;
    }
  }
  
  // ............................................................................
  // The 'keys' match -- however, we may only have a partial match in reality 
  // if not all keys comprising index have been used.
  // ............................................................................
  return defaultEqual;
}



//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Non-unique skiplist 
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------




////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two elements
////////////////////////////////////////////////////////////////////////////////

static int IndexStaticMultiCompareElementElement (TRI_skiplist_multi_t* multiSkiplist, void* leftElement, void* rightElement, int defaultEqual) {
  typedef SKIPLIST_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  int compareResult;                                    
  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;

  
  if (leftElement == NULL && rightElement == NULL) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_GREATER;
  }    

  if (leftElement == NULL && rightElement != NULL) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_LESS;
  }      
  
  if (leftElement == rightElement) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL;
  }    

  if (hLeftElement->numFields != hRightElement->numFields) {
    assert(false);
  }
  
  if (hLeftElement->data == hRightElement->data) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL;
  }    


  leftShaper  = ((TRI_doc_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_doc_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < hLeftElement->numFields; j++) {
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields), leftShaper, rightShaper);
    if (compareResult != 0) {
      return compareResult;
    }
  }
  
  return defaultEqual;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two keys
////////////////////////////////////////////////////////////////////////////////

static int  IndexStaticMultiCompareKeyElement (TRI_skiplist_multi_t* multiSkiplist, void* leftElement, void* rightElement, int defaultEqual) {
  typedef SKIPLIST_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  int compareResult;                                    
  size_t numFields;
  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;
  
  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }    

  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }    
  
  
  // ............................................................................
  // The document could be the same -- so no further comparison is required.
  // ............................................................................

  if (hLeftElement->data == hRightElement->data) {
    return 0;
  }    

  
  // ............................................................................
  // This call back function is used when we query the index, as such
  // the number of fields which we are using for the query may be less than
  // the number of fields that the index is defined with.
  // ............................................................................
  
  if (hLeftElement->numFields < hRightElement->numFields) {
    numFields = hLeftElement->numFields;
  }
  else {
    numFields = hRightElement->numFields;
  }
  
  leftShaper  = ((TRI_doc_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_doc_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields), leftShaper, rightShaper);
    if (compareResult != 0) {
      return compareResult;
    }
  }
  
  return defaultEqual;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two keys
////////////////////////////////////////////////////////////////////////////////

static bool IndexStaticMultiEqualElementElement (TRI_skiplist_multi_t* multiSkiplist, void* leftElement, void* rightElement) {

  typedef SKIPLIST_ELEMENT_TYPE(LocalElement_s,LocalElement_t);

  LocalElement_t* hLeftElement  = (LocalElement_t*)(leftElement);
  LocalElement_t* hRightElement = (LocalElement_t*)(rightElement);

  if (leftElement == rightElement) {
    return true;
  }    

  /*
  printf("%s:%u:%f:%f,%u:%u\n",__FILE__,__LINE__,
    *((double*)((hLeftElement->fields)->_data.data)),
    *((double*)((hRightElement->fields)->_data.data)),
    (uint64_t)(hLeftElement->data),
    (uint64_t)(hRightElement->data)
  );
  */
  return (hLeftElement->data == hRightElement->data);
}








#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

