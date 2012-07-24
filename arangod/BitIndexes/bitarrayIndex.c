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

#include "bitarrayIndex.h"
#include "BasicsC/string-buffer.h"
#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"
#include "BasicsC/logging.h"


// .............................................................................
// forward declaration of static functions used for iterator callbacks
// .............................................................................

static void  BitarrayIndexDestroyIterator          (TRI_index_iterator_t*);
static bool  BitarrayIndexHasNextIterationCallback (TRI_index_iterator_t*);
static void* BitarrayIndexNextIterationCallback    (TRI_index_iterator_t*);
static void* BitarrayIndexNextsIterationCallback   (TRI_index_iterator_t*, int64_t);
static bool  BitarrayIndexHasPrevIterationCallback (TRI_index_iterator_t*);
static void* BitarrayIndexPrevIterationCallback    (TRI_index_iterator_t*);
static void* BitarrayIndexPrevsIterationCallback   (TRI_index_iterator_t*, int64_t);
static void  BitarrayIndexResetIterator            (TRI_index_iterator_t*, bool);
// .............................................................................
// forward declaration of static functions used here
// .............................................................................

static int  BitarrayIndex_findHelper (BitarrayIndex*, TRI_vector_t*, TRI_index_operator_t*, TRI_index_iterator_t*);
static int  generateInsertBitMask    (BitarrayIndex*, const BitarrayIndexElement*, TRI_bitarray_mask_t*);
static int  generateEqualBitMask     (BitarrayIndex*, const TRI_relation_index_operator_t*, TRI_bitarray_mask_t*);
//static void debugPrintMaskFooter     (const char*); 
//static void debugPrintMaskHeader     (const char*);
//static void debugPrintMask           (BitarrayIndex*, uint64_t);



// -----------------------------------------------------------------------------
// --SECTION--                           bitarrayIndex     common public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarrayIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a bitarray index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void BitarrayIndex_destroy(BitarrayIndex* baIndex) {
  size_t j;
  if (baIndex == NULL) {
    return;
  } 
  for (j = 0;  j < baIndex->_values._length;  ++j) {
    TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j)));
  }
  TRI_DestroyVector(&baIndex->_values);
  
  TRI_FreeBitarray(baIndex->_bitarray);
  baIndex->_bitarray = NULL;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a bitarray index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void BitarrayIndex_free(BitarrayIndex* baIndex) {
  if (baIndex == NULL) {
    return;
  }  
  BitarrayIndex_destroy(baIndex);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, baIndex);
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



//------------------------------------------------------------------------------
// Public Methods 
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a new bitarray index. Failure will return an appropriate error code
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_new(BitarrayIndex** baIndex, 
                      TRI_memory_zone_t* memoryZone, 
                      size_t cardinality,
                      TRI_vector_t* values,
                      bool supportUndef,
                      void* context) {
  int     result;
  size_t  numArrays;
  int j;
  
  
  // ...........................................................................  
  // Sime simple checks
  // ...........................................................................  
  
  if (baIndex == NULL) {
    assert(false);
    return TRI_ERROR_INTERNAL;
  }  
  
  
  
  // ...........................................................................  
  // If the bit array index has arealdy been created, return internal error
  // ...........................................................................  
  
  if (*baIndex != NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................  
  // If the memory zone is invalid, then return an internal error
  // ...........................................................................  
    
  if (memoryZone == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................  
  // Create the bit array index structure
  // ...........................................................................  
  
  *baIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(BitarrayIndex), true);
  if (*baIndex == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  

  // ...........................................................................  
  // Copy the values into this index
  // ...........................................................................  
 
   
  TRI_InitVector(&((*baIndex)->_values), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_json_t));

  for (j = 0;  j < values->_length;  ++j) {
    TRI_json_t value;
    TRI_CopyToJson(TRI_UNKNOWN_MEM_ZONE, &value, (TRI_json_t*)(TRI_AtVector(values,j)));
    TRI_PushBackVector(&((*baIndex)->_values), &value);
  }
 
 
  // ...........................................................................  
  // Store whether or not the index supports 'undefined' documents (that is
  // documents with attributes which do not match those of the index
  // ...........................................................................  
  
  (*baIndex)->_supportUndef = supportUndef;
  
  // ...........................................................................  
  // Determine the number of bit columns which will comprise the bit array index.
  // ...........................................................................  
  
  numArrays = cardinality;
  
  // ...........................................................................  
  // Create the bit arrays
  // ...........................................................................  
  
  result = TRI_InitBitarray(&((*baIndex)->_bitarray), memoryZone, numArrays, NULL);


  // ...........................................................................  
  // return the result of creating the bit  arrays
  // ...........................................................................  
  
  return result;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into one or more bit arrays
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_add(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  int result;
  TRI_bitarray_mask_t mask;

  // ..........................................................................
  // At the current time we have no way in which to store undefined documents
  // need some sort of parameter passed here.
  // ..........................................................................
  
  // ..........................................................................
  // generate bit mask
  // ..........................................................................
  mask._mask = 0;
  mask._ignoreMask = 0;
  result = generateInsertBitMask(baIndex, element, &mask);
  //debugPrintMask(baIndex, mask._mask); 
 
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }  
  
  
  // ..........................................................................
  // insert the bit mask into the bit array
  // ..........................................................................
  
  result = TRI_InsertBitMaskElementBitarray(baIndex->_bitarray, &mask, element->data);  
  return result;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate one or more documents which match an index operator
////////////////////////////////////////////////////////////////////////////////

TRI_index_iterator_t* BitarrayIndex_find(BitarrayIndex* baIndex, 
                                         TRI_index_operator_t* indexOperator,
                                         TRI_vector_t* shapeList,
                                         void* collectionIndex,
                                         bool (*filter) (TRI_index_iterator_t*) ) {

  TRI_index_iterator_t* iterator;
  int result;
  
  // ...........................................................................  
  // Attempt to allocate memory for the index iterator which stores the results
  // if any of the lookup.
  // ...........................................................................  
  
  iterator = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_index_iterator_t), true);
  if (iterator == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL; // calling procedure needs to care when the iterator is null
  }  
  
  
  // ...........................................................................  
  // We now initialise the index iterator with all the call back functions.
  // ...........................................................................  

  TRI_InitVector(&(iterator->_intervals), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_index_iterator_interval_t));
  
  iterator->_index           = collectionIndex;  
  iterator->_currentInterval = 0;  
  iterator->_cursor          = NULL;
  
  iterator->_filter          = filter; 
  iterator->_hasNext         = BitarrayIndexHasNextIterationCallback;
  iterator->_next            = BitarrayIndexNextIterationCallback;
  iterator->_nexts           = BitarrayIndexNextsIterationCallback;
  iterator->_hasPrev         = BitarrayIndexHasPrevIterationCallback;
  iterator->_prev            = BitarrayIndexPrevIterationCallback;
  iterator->_prevs           = BitarrayIndexPrevsIterationCallback;
  iterator->_destroyIterator = BitarrayIndexDestroyIterator;
  iterator->_reset           = BitarrayIndexResetIterator;
  
  result = BitarrayIndex_findHelper(baIndex, shapeList, indexOperator, iterator);

  return iterator;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for add Index item
//////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_insert(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  return BitarrayIndex_add(baIndex,element);
} 



//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the bit arrays and master table
//////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_remove(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  int result;
  result = TRI_RemoveElementBitarray(baIndex->_bitarray, element->data); 
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates a bit array index  entry
//////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_update(BitarrayIndex* baIndex, 
                          const BitarrayIndexElement* oldElement, 
                          const BitarrayIndexElement* newElement) {
                          
  assert(false);                          
  return TRI_ERROR_INTERNAL;
}





// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Implementation of static functions forward declared above
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


// .............................................................................
// forward declaration of static functions used for iterator callbacks
// .............................................................................

void BitarrayIndexDestroyIterator(TRI_index_iterator_t* iterator) {
  TRI_DestroyVector(&(iterator->_intervals));
}  
 

bool  BitarrayIndexHasNextIterationCallback(TRI_index_iterator_t* iterator) {

  if (iterator->_intervals._length == 0) {
    return false;
  }
    
  if (iterator->_currentInterval >= iterator->_intervals._length) {
    return false;
  }  
  
  return true;  
}

  
void* BitarrayIndexNextIterationCallback(TRI_index_iterator_t* iterator) {
  TRI_index_iterator_interval_t* interval;

  iterator->_currentDocument = NULL;

  if (iterator->_cursor == NULL) {
    iterator->_currentInterval = 0;
  }
  
  
  if (iterator->_intervals._length == 0) {
    return NULL;
  }
  
  
  if (iterator->_currentInterval >= iterator->_intervals._length) {
    return NULL;
  }  

 
  interval = (TRI_index_iterator_interval_t*)(TRI_AtVector(&(iterator->_intervals),iterator->_currentInterval));
  
  if (interval == NULL) { // should not occur -- something is wrong
    LOG_WARNING("internal error in BitarrayIndexNextIterationCallback");
    return NULL;
  }
  
  iterator->_cursor          = interval->_leftEndPoint;
  iterator->_currentDocument = interval->_leftEndPoint;      
  
  ++iterator->_currentInterval;
  return iterator->_currentDocument;
}


void* BitarrayIndexNextsIterationCallback(TRI_index_iterator_t* iterator, int64_t jumpSize) {
  int64_t j;
  void* result = NULL;
  void* lastValidResult = NULL;
  
  for (j = 0; j < jumpSize; ++j) {
    result = BitarrayIndexNextIterationCallback(iterator);  
    if (result != NULL) {
      lastValidResult = result;
    }  
    if (result == NULL) {
      break;
    }  
  }
  return lastValidResult;
}

  
bool BitarrayIndexHasPrevIterationCallback(TRI_index_iterator_t* iterator) {

  if (iterator->_intervals._length == 0) {
    return false;
  }
    
  if (iterator->_currentInterval >= iterator->_intervals._length) {
    return false;
  }  
  
  return true;  
}

  
void* BitarrayIndexPrevIterationCallback(TRI_index_iterator_t* iterator) {
  TRI_index_iterator_interval_t* interval;

  iterator->_currentDocument = NULL;

  if (iterator->_cursor == NULL) {
    iterator->_currentInterval = iterator->_intervals._length - 1;
  }
  
  if (iterator->_currentInterval >= iterator->_intervals._length) {
    return false;
  }  
  
  if (iterator->_intervals._length == 0) {
    return NULL;
  }
  
 
  interval = (TRI_index_iterator_interval_t*)(TRI_AtVector(&(iterator->_intervals),iterator->_currentInterval));
  
  if (interval == NULL) { // should not occur -- something is wrong
    LOG_WARNING("internal error in BitarrayIndexPrevIterationCallback");
    return NULL;
  }
  
  iterator->_cursor          = interval->_leftEndPoint;
  iterator->_currentDocument = interval->_leftEndPoint;      
  
  if (iterator->_currentInterval == 0) {
    iterator->_currentInterval = iterator->_intervals._length;
  }
  else {
    --iterator->_currentInterval;
  }  
  return iterator->_currentDocument;
}

  
void* BitarrayIndexPrevsIterationCallback(TRI_index_iterator_t* iterator, int64_t jumpSize) {
  int64_t j;
  void* result = NULL;
  void* lastValidResult = NULL;
  
  for (j = 0; j < jumpSize; ++j) {
    result = BitarrayIndexPrevIterationCallback(iterator);  
    if (result != NULL) {
      lastValidResult = result;
    }  
    if (result == NULL) {
      break;
    }  
  }
  return lastValidResult;
}

  
void BitarrayIndexResetIterator(TRI_index_iterator_t* iterator, bool beginning) {
  if (beginning) {
    iterator->_cursor = NULL;
    iterator->_currentInterval = 0;
    iterator->_currentDocument = NULL;
    return;
  }

  iterator->_cursor = NULL;
  iterator->_currentInterval = 0;
  iterator->_currentDocument = NULL;
  if (iterator->_intervals._length > 0) {
    iterator->_currentInterval = iterator->_intervals._length - 1;
  }
}  
 
 
// .............................................................................
// forward declaration of static functions used internally here
// .............................................................................

int BitarrayIndex_findHelper(BitarrayIndex* baIndex, 
                              TRI_vector_t* shapeList, 
                              TRI_index_operator_t* indexOperator, 
                              TRI_index_iterator_t* iterator) {

  // BitarrayIndexElement element;
  TRI_bitarray_mask_t mask;
  int result;
  
  /*
  element.fields      = NULL;
  element.numFields   = 0;
  element.collection  = NULL;
  element.data        = NULL;
  */
  
  mask._mask       = 0;
  mask._ignoreMask = 0;
  
  // ............................................................................
  // Prepare the values variable with details about the fields and collection
  // these may be required later. These values make sense only for relational
  // type operators.
  // ............................................................................
  
  switch (indexOperator->_type) {
    case TRI_EQ_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR: 
    case TRI_LT_INDEX_OPERATOR: 
    case TRI_GE_INDEX_OPERATOR: 
    case TRI_GT_INDEX_OPERATOR: {
      /* TRI_relation_index_operator_t* relationOperator = (TRI_relation_index_operator_t*) (indexOperator); */
      /*
      element.fields     = relationOperator->_fields;
      element.numFields  = relationOperator->_numFields;
      element.collection = relationOperator->_collection;
      */
      break;
    }
    
    default: {
      // ..........................................................................
      // If it is not a relational index operator we may not have access
      // access _fields and _collection
      // ..........................................................................
      break;       
    }
  }


  // ............................................................................
  // Process the indexOperator recursively
  // ............................................................................
  
  switch (indexOperator->_type) {

    case TRI_AND_INDEX_OPERATOR: {
      assert(false);
      break;    
    }

  
    case TRI_OR_INDEX_OPERATOR: {
      assert(false);
      break;    
    }
    
    
    case TRI_EQ_INDEX_OPERATOR: {
      TRI_relation_index_operator_t* relationOperator = (TRI_relation_index_operator_t*)(indexOperator);

      // ............................................................................
      // for bitarray indexes, the number of attribute values ALWAYS matches the
      // the number of parameters for a TRI_EQ_INDEX_OPERATOR. However, the client
      // may wish some attributes to be ignored, so some values will be '{}'.
      // ............................................................................
      
      if (relationOperator->_numFields != shapeList->_length) {
        assert(false);
      } 


      // ............................................................................
      // generate the bit mask
      // ............................................................................
      
      result = generateEqualBitMask(baIndex, relationOperator, &mask); 
      //debugPrintMask(baIndex, mask._mask);      
      //debugPrintMask(baIndex, mask._ignoreMask);      
      
      result = TRI_LookupBitMaskBitarray(baIndex->_bitarray, &mask, iterator);

      break;    
    }
    
    case TRI_LE_INDEX_OPERATOR: {
      assert(false);
      break;    
    }  
    
    
    case TRI_LT_INDEX_OPERATOR: {
      assert(false);
      break;    
    }  
    

    case TRI_GE_INDEX_OPERATOR: {
      assert(false);
      break;    
    }  
  
  
    case TRI_GT_INDEX_OPERATOR: {
      assert(false);
      break;    
    }  
    
    default: {
      assert(0);
    }
    
  } // end of switch statement
   
  return result;   
}
            


////////////////////////////////////////////////////////////////////////////////
// Given the index structure and the list of shaped json values which came from
// some document we generate a bit mask.
////////////////////////////////////////////////////////////////////////////////

static bool isEqualJson(TRI_json_t* left, TRI_json_t* right) {

  if (left == NULL && right == NULL) {
    return true;
  }

  if (left == NULL || right == NULL) {
    return false;
  }

  if (left->_type != right->_type) {
    return false;
  }

  switch (left->_type) {
  
    case TRI_JSON_UNUSED: {
      return true;
    }
    
    case TRI_JSON_NULL: {
      return true;
    }
    
    case TRI_JSON_BOOLEAN: {
      return (left->_value._boolean == right->_value._boolean);
    }    
    
    case TRI_JSON_NUMBER: {
      return (left->_value._number == right->_value._number);
    }    
    
    case TRI_JSON_STRING: {
      return (strcmp(left->_value._string.data, right->_value._string.data) == 0);
    }    
    
    case TRI_JSON_ARRAY: {
      int j;
      
      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }
      
      for (j = 0; j < (left->_value._objects._length / 2); ++j) {
        TRI_json_t* leftName;
        TRI_json_t* leftValue;
        TRI_json_t* rightValue;

        leftName = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),2*j));
        if (leftName == NULL) {
          return false;
        }
        
        leftValue  = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),(2*j) + 1));
        rightValue = TRI_LookupArrayJson(right, leftName->_value._string.data);        

        if (isEqualJson(leftValue, rightValue)) {
          continue;
        }
        return false;
      }
      
      return true;
    }    
    
    case TRI_JSON_LIST: {
      int j;
      
      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }
      
      for (j = 0; j < left->_value._objects._length; ++j) {
        TRI_json_t* subLeft;
        TRI_json_t* subRight;
        subLeft  = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),j));
        subRight = (TRI_json_t*)(TRI_AtVector(&(right->_value._objects),j));
        if (isEqualJson(subLeft, subRight)) {
          continue;
        }
        return false;
      }
      
      return true;
    }   
    
    default: {
      assert(false);
    }  
  }        
}


static void generateEqualBitMaskHelper(TRI_json_t* valueList, TRI_json_t* value, uint64_t* mask) {
  int i;
  uint64_t other = 0;
  uint64_t tempMask = 0;
  
  for (i = 0; i < valueList->_value._objects._length; ++i) {    
    int k;
    TRI_json_t* listEntry = (TRI_json_t*)(TRI_AtVector(&(valueList->_value._objects), i));                 
        
    // ...........................................................................
    // if the ith possible set of values is not a list, do comparison 
    // ...........................................................................
        
    if (listEntry->_type != TRI_JSON_LIST) {
      if (isEqualJson(value, listEntry)) {
        tempMask = tempMask | (1 << i);
      }  
      continue; // there may be further matches!
    }
        
        
    // ...........................................................................
    // ith entry in the set of possible values is a list
    // ...........................................................................
        
    // ...........................................................................
    // Special case of an empty list -- this means all other values
    // ...........................................................................
        
    if (listEntry->_value._objects._length == 0) { // special case determine what the bit position of other is
      other = (1 << i);
      continue; // there may be further matches!
    }
        
        
    for (k = 0; k < listEntry->_value._objects._length; k++) {
      TRI_json_t* subListEntry;
      subListEntry = (TRI_json_t*)(TRI_AtVector(&(listEntry->_value._objects), k));            
      if (isEqualJson(value, subListEntry)) {
        tempMask = tempMask | (1 << i);
        break;
      }  
    }      
        
  }
  
  if (tempMask != 0) {
    *mask = *mask | tempMask;
    return;
  }
  
  if (other != 0) {
    *mask = *mask | other;
  }  
}  

            
int generateInsertBitMask(BitarrayIndex* baIndex, const BitarrayIndexElement* element, TRI_bitarray_mask_t* mask) {

  TRI_shaper_t* shaper;
  int j;
  int shiftLeft;
  
  // ...........................................................................
  // some safety checks first
  // ...........................................................................
  
  if (baIndex == NULL || element == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  if (element->collection == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................
  // We could be trying to store an 'undefined' document into the bitarray
  // We determine this implicitly. If element->numFields b == 0, then we
  // assume that the document did not have any matching attributes, yet since
  // we are here we wish to store this fact.
  // ...........................................................................
  
  if (!baIndex->_supportUndef && (element->numFields == 0 || element->fields == NULL)) {
    return TRI_ERROR_INTERNAL;
  }

  
  if (baIndex->_supportUndef && element->numFields == 0) {
    mask->_mask       = 1;
    mask->_ignoreMask = 0;
    return TRI_ERROR_NO_ERROR;
  }  

  
  // ...........................................................................
  // attempt to convert the stored TRI_shaped_json_t into TRI_Json_t so that
  // we can make a comparison between what values the bitarray index requires
  // and what values the document has sent.
  // ...........................................................................
  
  shaper      = ((TRI_doc_collection_t*)(element->collection))->_shaper;
  mask->_mask = 0;
  shiftLeft   = 0;
  
  for (j = 0; j < baIndex->_values._length; ++j) {
    TRI_json_t* valueList;
    TRI_json_t* value;
    uint64_t    other;    
    uint64_t    tempMask;
    
    value      = TRI_JsonShapedJson(shaper, &(element->fields[j])); // from shaped json to simple json   
    valueList  = (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j));
    other      = 0;
    tempMask   = 0;
    
    
    // .........................................................................
    // value is now the shaped json converted into plain json for comparison
    // ........................................................................

    generateEqualBitMaskHelper(valueList, value, &tempMask);       
         
    // ............................................................................
    // remove the json entry created from the shaped json
    // ............................................................................
    
    TRI_FreeJson(shaper->_memoryZone, value);
           
    mask->_mask = mask->_mask | (tempMask << shiftLeft);
    
    shiftLeft += valueList->_value._objects._length;
  }  
  
  return TRI_ERROR_NO_ERROR;
}


      
int generateEqualBitMask(BitarrayIndex* baIndex, const TRI_relation_index_operator_t* relationOperator, TRI_bitarray_mask_t* mask) {
  int j;
  int shiftLeft;
  int ignoreShiftLeft;
  // ...........................................................................
  // some safety checks first
  // ...........................................................................
  
  if (baIndex == NULL || relationOperator == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................
  // supportUndef -- refers to where the document does not have 1 or more 
  // attributes which are defined in the index. (Not related to whether or
  // not the attribute has a value defined in the set of supported values.)
  // ...........................................................................
  
  
  // ...........................................................................
  // if the number of attributes is 0, then something must be wrong
  // ...........................................................................
  
  if (relationOperator->_numFields == 0) {
    return TRI_ERROR_INTERNAL;
  }

  
  // ...........................................................................
  // if an attribute which was defined in the index, was not sent by the client,
  // then that bitarray column is ignored.
  // ...........................................................................
    
  mask->_mask       = 0;
  mask->_ignoreMask = 0;
  shiftLeft         = 0;
  ignoreShiftLeft   = 0;
  
  for (j = 0; j < baIndex->_values._length; ++j) { // loop over the number of attributes defined in the index
    TRI_json_t* valueList;
    TRI_json_t* value;
    uint64_t    tempMask;
    
    
    value     = (TRI_json_t*)(TRI_AtVector(&(relationOperator->_parameters->_value._objects), j));       
    valueList = (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j));
    
    
    ignoreShiftLeft += valueList->_value._objects._length;
    
    // .........................................................................
    // client did not send us this attribute (hence undefined value), therefore
    // this particular column we ignore
    // .........................................................................
    
    if (value->_type == TRI_JSON_UNUSED) {    
      uint64_t tempInt;
      tempMask = ~((~(uint64_t)(0)) << ignoreShiftLeft);
      tempInt  = (~(uint64_t)(0)) << (ignoreShiftLeft - valueList->_value._objects._length);
      mask->_ignoreMask = mask->_ignoreMask | (tempMask & tempInt);      
      tempMask = 0;
    }
        
    
    // .........................................................................
    // the client sent us one or more values for the attribute. If client sent
    // us [a_1,a_2,...,a_n] as a value, then this is interpretated as turning 
    // on those bits. Note that if the value is itself a json list, then
    // this must be sent as [[l_1,l_2,...,l_m]]
    // .........................................................................
    
    else {
      tempMask = 0;

      
      // ........................................................................
      // the value (for this jth attribute) sent by the client is NOT a list
      // ........................................................................

      if (value->_type != TRI_JSON_LIST) {
        generateEqualBitMaskHelper(valueList, value, &tempMask);       
      }
      
      
      // ........................................................................
      // the value (for this jth attribute) sent by the client IS  a list
      // we now have to loop through all the entries in this list
      // ........................................................................
      
      else {
        int i;
        for (i = 0; i < value->_value._objects._length; ++i) {
          TRI_json_t* listEntry = (TRI_json_t*)(TRI_AtVector(&(value->_value._objects), i));                         
          generateEqualBitMaskHelper(valueList, listEntry, &tempMask);       
        }
      }
      
    }     
     
    // ............................................................................
    // When we create a bitarray index, for example: ensureBitarray("x",[0,[],1,2,3])
    // and we insert doc with {"x" : "hello world"}, then, since the value of "x"
    // does not match 0,1,2 or 3 and [] appears as a valid list item, the doc is
    // inserted with a mask of 01000
    // This is what other means below
    // ............................................................................
    
    mask->_mask = mask->_mask | (tempMask << shiftLeft);
    
    shiftLeft += valueList->_value._objects._length;
  }  
  
  // ................................................................................
  // check whether we actually ignore everything!
  // ................................................................................

  if (mask->_mask == 0 && !baIndex->_supportUndef) {
    return TRI_ERROR_INTERNAL;
  }    
  
  //debugPrintMaskHeader("lookup mask/ignore mask");
  //debugPrintMask(baIndex,mask->_mask);
  //debugPrintMask(baIndex,mask->_ignoreMask);
  //debugPrintMaskFooter("");
  
  return TRI_ERROR_NO_ERROR;
}




/*
void debugPrintMaskFooter(const char* footer) {
  printf("%s\n\n",footer);
}

void debugPrintMaskHeader(const char* header) {
  printf("------------------- %s --------------------------\n",header);
}


void debugPrintMask(BitarrayIndex* baIndex, uint64_t mask) {
  int j;
  
  for (j = 0; j < baIndex->_bitarray->_numColumns; ++j) {
    if ((mask & ((uint64_t)(1) << j)) == 0) {
      printf("0");
    }
    else {
      printf("1");
    }
  }
  printf("\n");
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


