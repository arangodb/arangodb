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
#include "VocBase/primary-collection.h"
#include "BasicsC/utf8-helper.h"

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
  leftElement->fields     = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * leftElement->numFields, false);
  
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
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, hElement->fields);
    }  
  }  
}





// .............................................................................
// left < right  return -1
// left > right  return  1
// left == right return  0
// .............................................................................

typedef struct weighted_attribute_s {
  TRI_shape_aid_t _aid;
  int64_t _weight;  
  TRI_shaped_json_t _value;
  //const TRI_shape_t* _shape;
  //const char* _aname;
  const TRI_shaper_t* _shaper;
} weighted_attribute_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief Helper method to deal with freeing memory associated with
/// the weight comparisions
////////////////////////////////////////////////////////////////////////////////

static void freeShapeTypeJsonArrayHelper(weighted_attribute_t** leftWeightedList, 
                                         weighted_attribute_t** rightWeightedList) {
  if (*leftWeightedList != NULL) {                                         
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, *leftWeightedList);
    *leftWeightedList = NULL;
  }  
  if (*rightWeightedList != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, *rightWeightedList);
    *rightWeightedList = NULL;
  }                                           
}


// return the number of entries
static int compareShapeTypeJsonArrayHelper(const TRI_shape_t* shape, const TRI_shaper_t* shaper, 
                                           const TRI_shaped_json_t* shapedJson,
                                           weighted_attribute_t** attributeArray) {
  char* charShape = (char*)(shape);
  TRI_shape_size_t fixedEntries;     // the number of entries in the JSON array whose value is of a fixed size
  TRI_shape_size_t variableEntries;  // the number of entries in the JSON array whose value is not of a known fixed size
  TRI_shape_size_t j;
  int jj;
  const TRI_shape_aid_t* aids;
  const TRI_shape_sid_t* sids;
  const TRI_shape_size_t* offsets;
  
  // .............................................................................
  // Ensure we return an empty array - in case of funny business below
  // .............................................................................
  
  *attributeArray = NULL;
  
  
  // .............................................................................
  // Determine the number of fixed sized values
  // .............................................................................
  
  charShape = charShape + sizeof(TRI_shape_t);           
  fixedEntries = *((TRI_shape_size_t*)(charShape));

  
  // .............................................................................
  // Determine the number of variable sized values
  // .............................................................................
  
  charShape = charShape + sizeof(TRI_shape_size_t);
  variableEntries = *((TRI_shape_size_t*)(charShape));


  // .............................................................................
  // It may happen that the shaped_json_array is 'empty {}'
  // .............................................................................
  
  if ((fixedEntries + variableEntries) == 0) {
    return 0;
  }  
  

  // .............................................................................
  // Allocate memory to hold the attribute information required for comparison
  // .............................................................................
  
  *attributeArray = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (sizeof(weighted_attribute_t) * (fixedEntries + variableEntries)), false);   
  if (*attributeArray == NULL) {
    return -1;
  }


  // .............................................................................
  // Determine the list of shape identifiers
  // .............................................................................
  
  charShape = charShape + sizeof(TRI_shape_size_t);
  sids = (const TRI_shape_sid_t*)(charShape);
  
  charShape = charShape + (sizeof(TRI_shape_sid_t) * (fixedEntries + variableEntries));
  aids = (const TRI_shape_aid_t*)(charShape);

  charShape = charShape + (sizeof(TRI_shape_aid_t) * (fixedEntries + variableEntries));
  offsets = (const TRI_shape_size_t*)(charShape);
  
  for (j = 0; j < fixedEntries; ++j) {
    (*attributeArray)[j]._aid                = aids[j];
    (*attributeArray)[j]._weight             = shaper->lookupAttributeWeight((TRI_shaper_t*)(shaper),aids[j]);
    //(*attributeArray)[j]._shape              = shaper->lookupShapeId((TRI_shaper_t*)(shaper), sids[j]);
    (*attributeArray)[j]._value._sid         = sids[j];
    (*attributeArray)[j]._value._data.data   = shapedJson->_data.data + offsets[j];
    (*attributeArray)[j]._value._data.length = offsets[j + 1] - offsets[j];
    (*attributeArray)[j]._shaper             = shaper;
    //(*attributeArray)[j]._aname              = NULL;
  }
  
  offsets = (const TRI_shape_size_t*)(shapedJson->_data.data);
  for (j = 0; j < variableEntries; ++j) {
    jj = j + fixedEntries;
    (*attributeArray)[jj]._aid                = aids[jj];
    (*attributeArray)[jj]._weight             = shaper->lookupAttributeWeight((TRI_shaper_t*)(shaper),aids[jj]);
    //(*attributeArray)[jj]._shape              = shaper->lookupShapeId((TRI_shaper_t*)(shaper), sids[jj]);
    (*attributeArray)[jj]._value._sid         = sids[jj];
    (*attributeArray)[jj]._value._data.data   = shapedJson->_data.data + offsets[j];
    (*attributeArray)[jj]._value._data.length = offsets[j + 1] - offsets[j];
    (*attributeArray)[jj]._shaper             = shaper;
    //(*attributeArray)[jj]._aname              = NULL;
  }

  return (fixedEntries + variableEntries);  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Compares to weighted attributes
////////////////////////////////////////////////////////////////////////////////

static int attributeWeightCompareFunction(const void* leftItem, const void* rightItem) {
  const weighted_attribute_t* l = (const weighted_attribute_t*)(leftItem);
  const weighted_attribute_t* r = (const weighted_attribute_t*)(rightItem);

  if (l->_weight < r->_weight) { return -1; }
  if (l->_weight > r->_weight) { return  1; }
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Helper method for recursion for CompareShapedJsonShapedJson
////////////////////////////////////////////////////////////////////////////////


static int CompareShapeTypes (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right, TRI_shaper_t* leftShaper, TRI_shaper_t* rightShaper) {
  
  int result;
  int i;
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
  weighted_attribute_t* leftWeightedList;
  weighted_attribute_t* rightWeightedList;
  int leftNumWeightedList;
  int rightNumWeightedList;
  int numWeightedList;
  
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
    } // end of case TRI_SHAPE_ILLEGAL

    
    case TRI_SHAPE_NULL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: {
          return 1;
        }
        case TRI_SHAPE_NULL: {
          return 0;
        }
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
    } // end of case TRI_SHAPE_NULL
    

    case TRI_SHAPE_BOOLEAN: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL: {
          return 1;
        }
        case TRI_SHAPE_BOOLEAN: {
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
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_BOOLEAN

    
    case TRI_SHAPE_NUMBER: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN: {
          return 1;
        }
        case TRI_SHAPE_NUMBER: {
          // compare the numbers
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
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_NUMBER
    
    
    
    case TRI_SHAPE_SHORT_STRING: 
    case TRI_SHAPE_LONG_STRING: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER: {
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING: {
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
          
#ifdef TRI_HAVE_ICU  
          result = TRI_compare_utf8(leftString,rightString);
#else
          result = strcmp(leftString,rightString);
#endif
          return result;
        }
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_LONG/SHORT_STRING 

    
    case TRI_SHAPE_HOMOGENEOUS_LIST: 
    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
    case TRI_SHAPE_LIST: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING: {
          return 1;
        }
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
        case TRI_SHAPE_LIST: {
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
    } // end of case TRI_SHAPE_LIST ... 
    
    
    
    case TRI_SHAPE_ARRAY: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
        case TRI_SHAPE_LIST: {
          return 1;
        }
        
        case TRI_SHAPE_ARRAY: {
          assert(0);
          // ............................................................................  
          // We are comparing a left JSON array with another JSON array on the right
          // The comparison works as follows:
          //
          //   Suppose that leftShape has m key/value pairs and that the 
          //   rightShape has n key/value pairs
          //
          //   Extract the m key aids (attribute identifiers) from the leftShape
          //   Extract the n key aids (attribute identifiers) from the rightShape
          //
          //   Sort the key aids for both the left and right shape
          //   according to the weight of the key (attribute) 
          //
          //   Let lw_j denote the weight of the jth key from the sorted leftShape key list
          //   and rw_j the corresponding rightShape.
          //
          //   If lw_j < rw_j return -1
          //   If lw_j > rw_j return 1
          //   If lw_j == rw_j, then we extract the values and compare the values
          //   using recursion. 
          //   
          //   If lv_j < rv_j return -1
          //   If lv_j > rv_j return 1
          //   If lv_j == rv_j, then repeat the process with j+1.
          // ............................................................................  

          
          // ............................................................................
          // generate the left and right lists.
          // ............................................................................

          leftNumWeightedList  = compareShapeTypeJsonArrayHelper(leftShape, leftShaper, left, &leftWeightedList);
          rightNumWeightedList = compareShapeTypeJsonArrayHelper(rightShape, rightShaper, right, &rightWeightedList);


          // ............................................................................
          // If the left and right both resulted in errors, we return equality for want
          // of something better.
          // ............................................................................
          
          if ( (leftNumWeightedList < 0) && (rightNumWeightedList < 0) )  { // probably out of memory error        
            freeShapeTypeJsonArrayHelper(&leftWeightedList, &rightWeightedList);
            return 0;            
          }
          
          // ............................................................................
          // If the left had an error, we rank the left as the smallest item in the order
          // ............................................................................
          
          if (leftNumWeightedList < 0) { // probably out of memory error        
            freeShapeTypeJsonArrayHelper(&leftWeightedList, &rightWeightedList);
            return -1; // attempt to compare as low as possible
          }
          
          
          // ............................................................................
          // If the right had an error, we rank the right as the largest item in the order
          // ............................................................................
          
          if (rightNumWeightedList < 0) {
            freeShapeTypeJsonArrayHelper(&leftWeightedList, &rightWeightedList);
            return 1;
          }


          // ............................................................................
          // Are we comparing two empty shaped_json_arrays?
          // ............................................................................
          
          if ( (leftNumWeightedList == 0) && (rightNumWeightedList == 0) ) {
            freeShapeTypeJsonArrayHelper(&leftWeightedList, &rightWeightedList);
            return 0; 
          }
          

          // ............................................................................
          // If the left is empty, then it is smaller than the right, right?
          // ............................................................................

          if  (leftNumWeightedList == 0) {
            freeShapeTypeJsonArrayHelper(&leftWeightedList, &rightWeightedList);
            return -1; 
          }

          
          // ............................................................................
          // ...and the opposite of the above.
          // ............................................................................

          if  (rightNumWeightedList == 0) {
            freeShapeTypeJsonArrayHelper(&leftWeightedList, &rightWeightedList);
            return 1; 
          }
          
          

          // ..............................................................................
          // We now have to sort the left and right weighted list according to attribute weight
          // ..............................................................................

          qsort(leftWeightedList, leftNumWeightedList, sizeof(weighted_attribute_t), attributeWeightCompareFunction);
          qsort(rightWeightedList, rightNumWeightedList, sizeof(weighted_attribute_t), attributeWeightCompareFunction);                         


          // ..............................................................................
          // check the weight and if equal check the values. Notice that numWeightedList
          // below MUST be greater or equal to 1.
          // ..............................................................................
          
          numWeightedList = (leftNumWeightedList < rightNumWeightedList ? leftNumWeightedList: rightNumWeightedList);          
          
          for (i = 0; i < numWeightedList; ++i) {
          
            if (leftWeightedList[i]._weight != rightWeightedList[i]._weight) {
              result = (leftWeightedList[i]._weight < rightWeightedList[i]._weight ? -1: 1);
              break;
            }
            
            result = CompareShapeTypes (&(leftWeightedList[i]._value), &(rightWeightedList[i]._value), leftShaper, rightShaper);
            if (result != 0) { 
              break;
            }  
            
            // the attributes are equal now check for the values 
            /* start oreste debug
            const char* name = leftShaper->lookupAttributeId(leftShaper,leftWeightedList[i]._aid);
            printf("%s:%u:w=%ld:%s\n",__FILE__,__LINE__,leftWeightedList[i]._weight,name);
            const char* name = rightShaper->lookupAttributeId(rightShaper,rightWeightedList[i]._aid);
            printf("%s:%u:w=%ld:%s\n",__FILE__,__LINE__,rightWeightedList[i]._weight,name);
            end oreste debug */
          }        
          
          
          if (result == 0) {
            // ............................................................................
            // The comparisions above indicate that the shaped_json_arrays are equal, 
            // however one more check to determine if the number of elements in the arrays
            // are equal.
            // ............................................................................
            if (leftNumWeightedList < rightNumWeightedList) {
              result = -1;
            }
            else if (leftNumWeightedList > rightNumWeightedList) {
              result = 1;
            }
          }
          
          
          // ..............................................................................
          // Deallocate any memory for the comparisions and return the result
          // ..............................................................................
          
          freeShapeTypeJsonArrayHelper(&leftWeightedList, &rightWeightedList);
          return result;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_ARRAY
    
  } // end of switch (leftType)
  
  assert(false);

  return 0; //shut the vc++ up
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
  
  
  // ............................................................................
  // In the above function CompareShaeTypes we use strcmp which may return
  // an integer greater than 1 or less than -1. From this function we only
  // need to know whether we have equality (0), less than (-1)  or greater than (1)
  // ............................................................................
  
  if (result < 0) { 
    result = -1; 
  }
  else if (result > 0) { 
    result = 1; 
  }
  
  
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

  
  leftShaper  = ((TRI_primary_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(hRightElement->collection))->_shaper;
  
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
  
  
  leftShaper  = ((TRI_primary_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
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


  leftShaper  = ((TRI_primary_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < hLeftElement->numFields; j++) {
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields), leftShaper, rightShaper);
    if (compareResult != 0) {
    
      // ......................................................................
      // The function CompareShaedJsonShapedJson can only return 0, -1, or 1
      // that is, TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL (0)
      // TRI_SKIPLIST_COMPARE_STRICTLY_LESS (-1)
      // TRI_SKIPLIST_COMPARE_STRICTLY_GREATER (1)
      // ......................................................................
      
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
  
  leftShaper  = ((TRI_primary_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(hRightElement->collection))->_shaper;
  
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

