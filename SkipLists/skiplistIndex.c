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
#include "VocBase/document-collection.h"


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Common private methods
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------



////////////////////////////////////////////////////////////////////////////////
/// @brief Helper method for recursion for CompareShapedJsonShapedJson
////////////////////////////////////////////////////////////////////////////////

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
        case TRI_SHAPE_ILLEGAL: 
        {
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
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
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
          if (leftType == TRI_SHAPE_SHORT_STRING) {
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
            else if (TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
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
            else if (TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
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
      assert(false);
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
/// @brief Attempts to determine if there is a next document within an interval - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

static bool SkiplistHasNextIterationCallback(TRI_skiplist_iterator_t* iterator) {
  TRI_skiplist_iterator_interval_t* interval;
  void* leftNode;
  
  // ............................................................................. 
  // Some simple checks.
  // ............................................................................. 
  
  if (iterator == NULL) {
    return false;
  }

  if (iterator->_intervals._length == 0) {
    return false;
  }  

  // ............................................................................. 
  // this check is as follows if we have more intervals than the one
  // we are currently working on -- then of course we have a next doc  
  // ............................................................................. 
  if (iterator->_intervals._length - 1 > iterator->_currentInterval) {
    return true;
  }

  
  // ............................................................................. 
  // Obtain the current interval -- in case we ever use more than one interval
  // ............................................................................. 
  
  interval =  (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
  
  // ............................................................................. 
  // Obtain the left end point we are currently at
  // ............................................................................. 
  
  if (iterator->_cursor == NULL) {
    leftNode = interval->_leftEndPoint;    
  }  
  else {
    leftNode = iterator->_cursor;
  }


  // ............................................................................. 
  // If the left == right end point AND there are no more intervals then we have
  // no next.
  // ............................................................................. 
  
  if (leftNode == interval->_rightEndPoint) {
    return false;
  }
    
  // ...........................................................................
  // interval of the type (a,b) -- but nothing between a and b
  // such intervals are optimised out so will not be here
  // ...........................................................................
  if (iterator->_index->unique) {
    leftNode = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, leftNode);    
  }
  else {    
    leftNode = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, leftNode);    
  }
    

  // ...........................................................................
  // Check various possibilities
  // ...........................................................................
  
  if (leftNode == NULL) {
    return false;
  }
    
  if (leftNode == interval->_rightEndPoint) {
    return false;
  }
     
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Attempts to determine if there is a previous document within an interval - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

static bool SkiplistHasPrevIterationCallback(TRI_skiplist_iterator_t* iterator) {
  TRI_skiplist_iterator_interval_t* interval;
  void* rightNode;
  
  // ............................................................................. 
  // Some simple checks.
  // ............................................................................. 
  
  if (iterator == NULL) {
    return false;
  }

  if (iterator->_intervals._length == 0) {
    return false;
  }  

  // ............................................................................. 
  // this check is as follows if we have more intervals than the one
  // we are currently working on -- then of course we have a prev doc  
  // ............................................................................. 
  if (iterator->_currentInterval > 0 ) {
    return true;
  }

  
  // ............................................................................. 
  // Obtain the current interval -- in case we ever use more than one interval
  // ............................................................................. 
  
  interval =  (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
  
  // ............................................................................. 
  // Obtain the left end point we are currently at
  // ............................................................................. 
  
  if (iterator->_cursor == NULL) {
    rightNode = interval->_rightEndPoint;    
  }  
  else {
    rightNode = iterator->_cursor;
  }


  // ............................................................................. 
  // If the left == right end point AND there are no more intervals then we have
  // no next.
  // ............................................................................. 
  
  if (rightNode == interval->_leftEndPoint) {
    return false;
  }
    
  // ...........................................................................
  // interval of the type (a,b) -- but nothing between a and b
  // such intervals are optimised out so will not be here
  // ...........................................................................
  if (iterator->_index->unique) {
    rightNode = TRI_PrevNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, rightNode);    
  }
  else {    
    rightNode = TRI_PrevNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, rightNode);    
  }
    

  // ...........................................................................
  // Check various possibilities
  // ...........................................................................
  
  if (rightNode == NULL) {
    return false;
  }
    
  if (rightNode == interval->_leftEndPoint) {
    return false;
  }
     
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Jumps forwards or backwards by jumpSize and returns the document
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistIteration(TRI_skiplist_iterator_t* iterator, int64_t jumpSize) {
  TRI_skiplist_iterator_interval_t* interval;
  TRI_skiplist_node_t* currentNode;
  int64_t j;
  
  // ............................................................................. 
  // Some simple checks.
  // ............................................................................. 
  
  if (iterator == NULL) {
    return NULL;
  }

  if (iterator->_intervals._length == 0) {
    return NULL;
  }  
 
  currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);
  
  if (jumpSize == 0) {
    if (currentNode == NULL) {
      return NULL;
    }
    else {       
      return &(currentNode->_element);
    }    
  }

  
  
  // ............................................................................. 
  // If the current cursor is NULL and jumpSize < 0, then start at the endpoint of
  // the right most interval.
  // ............................................................................. 
  if (currentNode == NULL && jumpSize < 0) {
    interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_intervals._length - 1) );    
    
    if (iterator->_index->unique) {
      iterator->_cursor = TRI_PrevNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, interval->_rightEndPoint);    
    }
    else {
      iterator->_cursor = TRI_PrevNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, interval->_rightEndPoint);    
    }    
    
    currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);

    if (currentNode == NULL) {
      return NULL;
    }
    
    if (currentNode == interval->_leftEndPoint) {
      return NULL;
    }    

    return &(currentNode->_element);
  }
  
  
  // ............................................................................. 
  // If the current cursor is NULL and jumpSize > 0, then start at the left point of
  // the left most interval.
  // ............................................................................. 
  if (currentNode == NULL && jumpSize > 0) {
    interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), 0) );
    
    if (iterator->_index->unique) {
      iterator->_cursor = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, interval->_leftEndPoint);    
    }  
    else {
      iterator->_cursor = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, interval->_leftEndPoint);    
    }    
    
    currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);

    if (currentNode == NULL) {
      return NULL;
    }
    
    if (currentNode == interval->_rightEndPoint) {
    
      if (iterator->_index->unique) {
        iterator->_cursor = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, interval->_leftEndPoint);    
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, interval->_leftEndPoint);    
      }      

      currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);
      return NULL;
    }
    
    return &(currentNode->_element);
  }
  
  
  // ............................................................................. 
  // Obtain the current interval we are at.
  // ............................................................................. 

  interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
  // ............................................................................. 
  // use the current cursor and move jumpSize back.
  // ............................................................................. 
  if (jumpSize < 0) {
    jumpSize = -jumpSize;
    for (j = 0; j < jumpSize; ++j) {
      if (iterator->_cursor == interval->_leftEndPoint) {
        if (iterator->_currentInterval == 0) {
          return NULL;
        }  
        --iterator->_currentInterval;
        interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
        iterator->_cursor = interval->_rightEndPoint;
      }      
  
      if (iterator->_index->unique) {
        iterator->_cursor = TRI_PrevNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, iterator->_cursor);
      }
      else {
        iterator->_cursor = TRI_PrevNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, iterator->_cursor);
      }      
      
    } 
    
    if (iterator->_cursor == interval->_leftEndPoint) {
      if (iterator->_currentInterval == 0) {
        return NULL;
      }  
      --iterator->_currentInterval;
      interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
      iterator->_cursor = interval->_rightEndPoint;
      
      if (iterator->_index->unique) {
        iterator->_cursor = TRI_PrevNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, iterator->_cursor);
      }
      else {
        iterator->_cursor = TRI_PrevNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, iterator->_cursor);
      }      
      
    }  
  }


  // ............................................................................. 
  // use the current cursor and move jumpSize forward.
  // ............................................................................. 
  if (jumpSize > 0) {
  
    for (j = 0; j < jumpSize; ++j) {
      if (iterator->_cursor == interval->_rightEndPoint) {
        if (iterator->_currentInterval == (iterator->_intervals._length - 1)) {
          return NULL;
        }  
        ++iterator->_currentInterval;
        interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), 
                                                         iterator->_currentInterval) );    
        iterator->_cursor = interval->_leftEndPoint;
      }      

      if (iterator->_index->unique) {
        iterator->_cursor = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, iterator->_cursor);
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, iterator->_cursor);
      }      
    } 
    
    if (iterator->_cursor == interval->_rightEndPoint) {
      if (iterator->_currentInterval == (iterator->_intervals._length - 1)) {
        return NULL;
      }  
      ++iterator->_currentInterval;
      interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
      iterator->_cursor = interval->_leftEndPoint;

      if (iterator->_index->unique) {
        iterator->_cursor = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, iterator->_cursor);
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, iterator->_cursor);
      }
      
    }      
  }

  
  currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);
  if (currentNode == NULL) {
    return NULL;
  }
  return &(currentNode->_element);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping forward by 1
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistNextIterationCallback(TRI_skiplist_iterator_t* iterator) {
  return SkiplistIteration(iterator,1);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping forward by jumpSize docs
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistNextsIterationCallback(TRI_skiplist_iterator_t* iterator, int64_t jumpSize) {
  return SkiplistIteration(iterator,jumpSize);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping backwards by 1
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistPrevIterationCallback(TRI_skiplist_iterator_t* iterator) {
  return SkiplistIteration(iterator,-1);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping backwards by jumpSize docs
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistPrevsIterationCallback(TRI_skiplist_iterator_t* iterator, int64_t jumpSize) {
  return SkiplistIteration(iterator,-jumpSize);
}







//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Unique Skiplists 
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Private Methods Unique Skiplists
//------------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief compares two elements in a skip list
////////////////////////////////////////////////////////////////////////////////

static int CompareElementElement (struct TRI_skiplist_s* skiplist, void* leftElement, void* rightElement, int defaultEqual) {                                
  // .............................................................................
  // Compare two elements and determines:
  // left < right   : return -1
  // left == right  : return 0
  // left > right   : return 1
  // .............................................................................
  int compareResult;                                    
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
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
  // we return 1 to indicate that the leftElement is bigger than the rightElement
  // this has the effect of moving to the next node in the skiplist.
  // ............................................................................
   
  return defaultEqual;
}




////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key and an element
////////////////////////////////////////////////////////////////////////////////

static int CompareKeyElement (struct TRI_skiplist_s* skiplist, void* leftElement, void* rightElement, int defaultEqual) {
  // .............................................................................
  // Compare two elements and determines:
  // left < right   : return -1
  // left == right  : return 0
  // left > right   : return 1
  // .............................................................................
  int compareResult;                               
  size_t numFields;
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
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




//------------------------------------------------------------------------------
// Public Methods Unique Skiplists
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new unique entry skiplist
////////////////////////////////////////////////////////////////////////////////

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



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into a unique skip list
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex_add(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_InsertKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, element, element, false);  
  if (result) {
   return 0;
  }    
  return -1;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief Locates one or more ranges within the unique skiplist and returns iterator
//////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_iterator_t* SkiplistIndex_find(SkiplistIndex* skiplistIndex, TRI_vector_t* shapeList, TRI_sl_operator_t* slOperator) {
  SkiplistIndexElement             values;
  TRI_skiplist_iterator_t*         results;
  TRI_sl_relation_operator_t*      relationOperator;
  TRI_skiplist_iterator_interval_t interval;
 
 
  results = TRI_Allocate(sizeof(TRI_skiplist_iterator_t));    
  results->_index = skiplistIndex;
  TRI_InitVector(&(results->_intervals), sizeof(TRI_skiplist_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_hasNext         = SkiplistHasNextIterationCallback;
  results->_next            = SkiplistNextIterationCallback;
  results->_nexts           = SkiplistNextsIterationCallback;
  results->_hasPrev         = SkiplistHasPrevIterationCallback;
  results->_prev            = SkiplistPrevIterationCallback;
  results->_prevs           = SkiplistPrevsIterationCallback;
  
  // ............................................................................. 
  // For now we are only supporting the one relation operator
  // The NOT EQUAL operator NE will produce two intervals (-infinity,a) u (a,infinity)  
  // ............................................................................. 
  if (slOperator->_type == TRI_SL_AND_OPERATOR ||
      slOperator->_type == TRI_SL_NOT_OPERATOR ||
      slOperator->_type == TRI_SL_OR_OPERATOR  ||
      slOperator->_type == TRI_SL_NE_OPERATOR)  {
    assert(false);
  }
  
  relationOperator = (TRI_sl_relation_operator_t*)(slOperator);
  
  // ............................................................................. 
  // There is an issue with storing TRI_shaped_json_t instead of TRI_json_t
  // The shaped json relay on a shaper to determine the internal structure
  // conceivably we can be getting many many random lookups for which effectively
  // a shape has to be created and stored.   
  // ............................................................................. 
  
  // ............................................................................. 
  // When we are performing a lookup we are comparing the TRI_shaped_json_t 
  // stored in a document with the data sent to use by some external source (client).
  // Hence we are only interested in comparing the field values rather than ensuring
  // that we have exactly the same document -- which will be required when we
  // are asked to remove a document.
  // ............................................................................. 
  
  values.fields     = relationOperator->_fields;
  values.numFields  = relationOperator->_numFields;
  values.collection = relationOperator->_collection;
  
  // ............................................................................. 
  // TODO: Allow for abitrary number of intervals and then arrange intervals
  // as a set of disjpint intervals.
  // ............................................................................. 
  
  // ............................................................................. 
  // We wish to obtain an interval for which the documents will reside within this
  // interval. This is how the lookup procedure operates to obtain these intervals
  // ............................................................................. 

  // ............................................................................. 
  // The relation operation for the skiplist is "==" EQ
  // ............................................................................. 
  
  if (slOperator->_type == TRI_SL_EQ_OPERATOR) {
    if (relationOperator->_numFields == shapeList->_length) {
      interval._leftEndPoint  = TRI_LookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      if (interval._leftEndPoint == NULL) {
        return results;
      }  
      else {
        interval._rightEndPoint = TRI_NextNodeSkipList (skiplistIndex->skiplist.uniqueSkiplist, interval._leftEndPoint);      
        interval._leftEndPoint  = TRI_PrevNodeSkipList (skiplistIndex->skiplist.uniqueSkiplist, interval._leftEndPoint);      
      }        
      if ( (interval._leftEndPoint != NULL) &&  (interval._rightEndPoint != NULL) ) {
        TRI_PushBackVector(&(results->_intervals),&interval);
      }  
      return results;
    }  
    interval._leftEndPoint  = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
    interval._rightEndPoint = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
    if ( (interval._leftEndPoint != NULL) &&  (interval._rightEndPoint != NULL) ) {
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }

  
  // ............................................................................. 
  // The relation operation for the skiplist is "<=" LE
  // Special - take care of the case where
  //           number of attributes of the key is less than index number
  // can be optimised when the num fields == index definition num fields
  // i.e use lookup instead of leftlookup
  // ............................................................................. 
    
  if (slOperator->_type == TRI_SL_LE_OPERATOR) {
    if (relationOperator->_numFields > 1) {
      values.numFields  = relationOperator->_numFields - 1;
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      values.numFields  = relationOperator->_numFields;
    }  
    else {
      interval._leftEndPoint  = TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);
    }
    interval._rightEndPoint = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
    
    if (interval._rightEndPoint != NULL) {
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }  

  // ............................................................................. 
  // The relation operation for the skiplist is "<" LT
  // ............................................................................. 
  
  if (slOperator->_type == TRI_SL_LT_OPERATOR) {
    if (relationOperator->_numFields > 1) {
      values.numFields  = relationOperator->_numFields - 1;
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      values.numFields  = relationOperator->_numFields;
    }  
    else {
      interval._leftEndPoint  = TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);
    }
    interval._rightEndPoint = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
    
    if (interval._rightEndPoint != NULL) {
      interval._rightEndPoint = TRI_NextNodeSkipList (skiplistIndex->skiplist.uniqueSkiplist, interval._rightEndPoint);      
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }  

  
  // ............................................................................. 
  // The relation operation for the skiplist is ">=" GE
  // ............................................................................. 
  
  if (slOperator->_type == TRI_SL_GE_OPERATOR) {
    if (relationOperator->_numFields > 1) {
      values.numFields  = relationOperator->_numFields - 1;
      interval._rightEndPoint = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      values.numFields  = relationOperator->_numFields;
    }
    else {
      interval._rightEndPoint = TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);
    }
    
    interval._leftEndPoint  = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
    if (interval._leftEndPoint != NULL) {
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }  
  
  
    
  // ............................................................................. 
  // The relation operation for the skiplist is ">" LT
  // ............................................................................. 

  if (slOperator->_type == TRI_SL_GT_OPERATOR) {
    if (relationOperator->_numFields > 1) {
      values.numFields  = relationOperator->_numFields - 1;
      interval._rightEndPoint = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      values.numFields  = relationOperator->_numFields;
    }
    else {
      interval._rightEndPoint = TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);
    }
    
    interval._leftEndPoint  = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
    if (interval._leftEndPoint != NULL) {
      interval._leftEndPoint = TRI_PrevNodeSkipList (skiplistIndex->skiplist.uniqueSkiplist, interval._leftEndPoint);      
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }  
  
  return results;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for addIndex 
//////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex_insert(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  return SkiplistIndex_add(skiplistIndex,element);
} 



//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the skip list
//////////////////////////////////////////////////////////////////////////////////

bool SkiplistIndex_remove(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_RemoveElementSkipList(skiplistIndex->skiplist.uniqueSkiplist, element, NULL); 
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates a skiplist entry
//////////////////////////////////////////////////////////////////////////////////

bool SkiplistIndex_update(SkiplistIndex* skiplistIndex, const SkiplistIndexElement* beforeElement, const SkiplistIndexElement* afterElement) {
  // updates an entry in the skip list, first removes beforeElement, 
  // then adds the afterElement -- should never be called here
  // call SkiplistIndex_remove first and then SkiplistIndex_add
  assert(false);
}







//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-skiplist non-unique skiplist indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Private methods
//------------------------------------------------------------------------------



////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two elements
////////////////////////////////////////////////////////////////////////////////

static int MultiCompareElementElement (TRI_skiplist_multi_t* multiSkiplist, void* leftElement, void* rightElement, int defaultEqual) {

  int compareResult;                                    
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
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
  
  if (leftElement == rightElement) {
    return 0;
  }    

  if (hLeftElement->numFields != hRightElement->numFields) {
    assert(false);
  }
  
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
  
  return defaultEqual;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two keys
////////////////////////////////////////////////////////////////////////////////

static int  MultiCompareKeyElement (TRI_skiplist_multi_t* multiSkiplist, void* leftElement, void* rightElement, int defaultEqual) {

  int compareResult;                                    
  size_t numFields;
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
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





//------------------------------------------------------------------------------
// Public Methods Non-Unique Muilti Skiplists
//------------------------------------------------------------------------------






////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new non-uniqe (allows duplicates) multi skiplist
////////////////////////////////////////////////////////////////////////////////

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
                        MultiCompareElementElement,
                        MultiCompareKeyElement, 
                        TRI_SKIPLIST_PROB_HALF, 40);
                   
  return skiplistIndex;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into a multi skiplist
////////////////////////////////////////////////////////////////////////////////

int MultiSkiplistIndex_add(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_InsertElementSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, element, false);  
  if (result) {
    return 0;
  }    
  // failure could be caused by attempting to add the SAME (as in same doc ptr) document
  return -1;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief Locates one or more ranges within the unique skiplist and returns iterator
//////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_iterator_t* MultiSkiplistIndex_find(SkiplistIndex* skiplistIndex, TRI_vector_t* shapeList, TRI_sl_operator_t* slOperator) {
  SkiplistIndexElement             values;
  TRI_skiplist_iterator_t*         results;
  TRI_sl_relation_operator_t*      relationOperator;
  TRI_skiplist_iterator_interval_t interval;
 
 
  results = TRI_Allocate(sizeof(TRI_skiplist_iterator_t));    
  results->_index = skiplistIndex;
  TRI_InitVector(&(results->_intervals), sizeof(TRI_skiplist_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_hasNext         = SkiplistHasNextIterationCallback;
  results->_next            = SkiplistNextIterationCallback;
  results->_nexts           = SkiplistNextsIterationCallback;
  results->_hasPrev         = SkiplistHasPrevIterationCallback;
  results->_prev            = SkiplistPrevIterationCallback;
  results->_prevs           = SkiplistPrevsIterationCallback;
  
  // ............................................................................. 
  // For now we are only supporting the one relation operator
  // The NOT EQUAL operator NE will procedure two intervals (-infinity,a) u (a,infinity)  
  // ............................................................................. 
  if (slOperator->_type == TRI_SL_AND_OPERATOR ||
      slOperator->_type == TRI_SL_NOT_OPERATOR ||
      slOperator->_type == TRI_SL_OR_OPERATOR  ||
      slOperator->_type == TRI_SL_NE_OPERATOR)  {
    assert(false);
  }
  
  relationOperator = (TRI_sl_relation_operator_t*)(slOperator);
  
  values.fields     = relationOperator->_fields;
  values.numFields  = relationOperator->_numFields;
  values.collection = relationOperator->_collection;
  
  // ............................................................................. 
  // TODO: Allow for abitrary number of intervals and then arrange intervals
  // as a set of disjpint intervals.
  // ............................................................................. 
  
  // ............................................................................. 
  // We wish to obtain an interval for which the documents will reside within this
  // interval. This is how the lookup procedure operates to obtain these intervals
  // ............................................................................. 

  // ............................................................................. 
  // The relation operation for the skiplist is "==" EQ
  // ............................................................................. 
  
  if (slOperator->_type == TRI_SL_EQ_OPERATOR) {    
    interval._leftEndPoint  = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
    interval._rightEndPoint = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
    if ( (interval._leftEndPoint != NULL) &&  (interval._rightEndPoint != NULL) ) {
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }

  
  // ............................................................................. 
  // The relation operation for the skiplist is "<=" LE
  // Special - take care of the case where
  //           number of attributes of the key is less than index number
  // can be optimised when the num fields == index definition num fields
  // i.e use lookup instead of leftlookup
  // ............................................................................. 
    
  if (slOperator->_type == TRI_SL_LE_OPERATOR) {
    if (relationOperator->_numFields > 1) {
      values.numFields  = relationOperator->_numFields - 1;
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      values.numFields  = relationOperator->_numFields;
    }  
    else {
      interval._leftEndPoint  = TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
    }
    interval._rightEndPoint = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
    
    if (interval._rightEndPoint != NULL) {
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }  

  // ............................................................................. 
  // The relation operation for the skiplist is "<" LT
  // ............................................................................. 
  
  if (slOperator->_type == TRI_SL_LT_OPERATOR) {
    if (relationOperator->_numFields > 1) {
      values.numFields  = relationOperator->_numFields - 1;
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      values.numFields  = relationOperator->_numFields;
    }  
    else {
      interval._leftEndPoint  = TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
    }
    interval._rightEndPoint = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
    
    if (interval._rightEndPoint != NULL) {
      interval._rightEndPoint = TRI_NextNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, interval._rightEndPoint);      
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }  

  
  // ............................................................................. 
  // The relation operation for the skiplist is ">=" GE
  // ............................................................................. 
  
  if (slOperator->_type == TRI_SL_GE_OPERATOR) {
    if (relationOperator->_numFields > 1) {
      values.numFields  = relationOperator->_numFields - 1;
      interval._rightEndPoint = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      values.numFields  = relationOperator->_numFields;
    }
    else {
      interval._rightEndPoint = TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
    }
    
    interval._leftEndPoint  = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
    if (interval._leftEndPoint != NULL) {
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }  
  
  
    
  // ............................................................................. 
  // The relation operation for the skiplist is ">" LT
  // ............................................................................. 

  if (slOperator->_type == TRI_SL_GT_OPERATOR) {
    if (relationOperator->_numFields > 1) {
      values.numFields  = relationOperator->_numFields - 1;
      interval._rightEndPoint = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      values.numFields  = relationOperator->_numFields;
    }
    else {
      interval._rightEndPoint = TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
    }
    
    interval._leftEndPoint  = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
    if (interval._leftEndPoint != NULL) {
      interval._leftEndPoint = TRI_PrevNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, interval._leftEndPoint);      
      TRI_PushBackVector(&(results->_intervals),&interval);
    }  
    return results;
  }  
  
  return results;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for addIndex 
//////////////////////////////////////////////////////////////////////////////////

int MultiSkiplistIndex_insert(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  return MultiSkiplistIndex_add(skiplistIndex,element);
} 


//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the skiplist
//////////////////////////////////////////////////////////////////////////////////

bool MultiSkiplistIndex_remove(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_RemoveElementSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, element, NULL); 
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates and entry in a multi skiplist
//////////////////////////////////////////////////////////////////////////////////

bool MultiSkiplistIndex_update(SkiplistIndex* skiplistIndex, SkiplistIndexElement* beforeElement, SkiplistIndexElement* afterElement) {
  assert(false); // should never be called directly
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


