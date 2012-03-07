////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. O
/// @author Copyright 2006-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "skiplist.h"
#include <BasicsC/random.h>

#define SKIPLIST_ABSOLUTE_MAX_HEIGHT 100
// -----------------------------------------------------------------------------
// --SECTION--                                                 ASSOCIATIVE ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElement (TRI_skiplist_t* array, void* element) {
}


////////////////////////////////////////////////////////////////////////////////
/// @brief determines at what 'height' the item is to be added
////////////////////////////////////////////////////////////////////////////////

static int32_t RandLevel (TRI_skiplist_base_t* skiplist) {

  uint32_t level = 0;
  int counter    = 0;
  uint32_t* ptr  = skiplist->_random;
  int j;


  // ...........................................................................
  // Obtain the random numbers and store them in the pre allocated storage
  // ...........................................................................
  for (j = 0; j < skiplist->_numRandom; ++j) {
    *ptr = TRI_UInt32Random();  
    ++ptr;
  }
  ptr = skiplist->_random;
  
  
  // ...........................................................................
  // Use the bit list to determine the probability of the level.
  // For 1/2: if bit (0) we stop, otherwise increase level.
  // For 1/3: if bits (0,0) we stop, if bits (1,1) ignore and continue, otherwise increase level
  // For 1/4: if bits (0,0) we stop, otherwise increase level
  // ...........................................................................
  switch (skiplist->_prob) {
  
    case TRI_SKIPLIST_PROB_HALF: {
      counter = 0;
      while (level < skiplist->_maxHeight) {
        if ((1 & (*ptr)) == 0) {
          break;
        }
        ++level;
        (*ptr) = (*ptr) >> 1;
        ++counter;
        if (counter == 32) {
          ++ptr;
          counter = 0;
        }
      }
      break;
    }
    
    case TRI_SKIPLIST_PROB_THIRD: {
      while (level < skiplist->_maxHeight) {
        if ((3 & (*ptr)) == 0) {
          break;
        }  
        else if ((3 & (*ptr)) == 3) {
          // do nothing do not increase level
        }
        else {
          ++level;
        }  
        (*ptr) = (*ptr) >> 2;
        ++counter;
        if (counter == 16) {
          ++ptr;
          counter = 0;          
        }
      }
      break;
    }
    
    case TRI_SKIPLIST_PROB_QUARTER: {
      counter = 0;
      while (level < skiplist->_maxHeight) {
        if ((3 & (*ptr)) == 0) {
          break;
        }  
        ++level;
        (*ptr) = (*ptr) >> 2;
        ++counter;
        if (counter == 16) {
          ++ptr;
          counter = 0;          
        }
      }
      break;
    }
    
    default: {
      return -1;
    }    
  }
  return level;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Grow the node at the height specified.
////////////////////////////////////////////////////////////////////////////////

static void GrowNodeHeight(TRI_skiplist_node_t* node, uint32_t newHeight) {
                           
  TRI_skiplist_nb_t* oldColumn = node->_column;
  uint32_t j;

  if (node->_colLength >= newHeight) {
    return;
  }
  
  node->_column = TRI_Allocate(sizeof(TRI_skiplist_node_t) * newHeight);
  memcpy(node->_column, oldColumn, node->_colLength * sizeof(TRI_skiplist_node_t) );
  
  // ...........................................................................
  // Initialise the storage
  // ...........................................................................
  
  for (j = node->_colLength; j < newHeight; ++j) {
    (node->_column)[j]._prev = NULL; 
    (node->_column)[j]._next = NULL; 
  }
  
  TRI_Free(oldColumn);
  node->_colLength = newHeight;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief joins a left node and right node together
////////////////////////////////////////////////////////////////////////////////

static void JoinNodes(TRI_skiplist_node_t* leftNode, TRI_skiplist_node_t* rightNode, 
                      uint32_t startLevel, uint32_t endLevel) {
  uint32_t j;
  
  if (startLevel > endLevel) { // something wrong
    assert(false);
    return;
  }  

  // change level to heigth   
  endLevel += 1;
    
  if (leftNode->_colLength < endLevel) {
    assert(false);
    return;
  }

  if (rightNode->_colLength < endLevel) {
    assert(false);
    return;
  }
  
  for (j = startLevel; j < endLevel; ++j) {
    (leftNode->_column)[j]._next = rightNode;  
    (rightNode->_column)[j]._prev = leftNode;  
  }  
}


static void TRI_DestroySkipListNode (TRI_skiplist_node_t* node) {

  if (node == NULL) {
    return;
  }  
  
  TRI_Free(node->_column);
}

static void TRI_FreeSkipListNode (TRI_skiplist_base_t* skiplist, TRI_skiplist_node_t* node) {
  TRI_DestroySkipListNode(node);     
  if ( (node == &(skiplist->_startNode)) ||
     (node == &(skiplist->_endNode)) ) {
    return;   
  }  
  TRI_Free(node);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an skip list
////////////////////////////////////////////////////////////////////////////////

void TRI_InitSkipList (TRI_skiplist_t* skiplist,
                       size_t elementSize,
                       int (*compareElementElement) (TRI_skiplist_t*, void*, void*),
                       int (*compareKeyElement) (TRI_skiplist_t*, void*, void*),
                       TRI_skiplist_prob_e probability,
                       uint32_t maximumHeight) {

  if (skiplist == NULL) {
    return;
  }
  
  // ..........................................................................  
  // Assign the comparision call back functions
  // ..........................................................................  
  
  skiplist->compareElementElement = compareElementElement;
  skiplist->compareKeyElement     = compareKeyElement;

  // ..........................................................................  
  // Assign the maximum height of the skip list. This maximum height must be
  // no greater than the absolute max height defined as a compile time parameter
  // ..........................................................................  
  skiplist->_base._maxHeight = maximumHeight;
  if (maximumHeight > SKIPLIST_ABSOLUTE_MAX_HEIGHT) {
    printf("%s:%d:Invalid maximum height for skiplist\n",__FILE__,__LINE__);
    assert(false);
  }  
  
  // ..........................................................................  
  // Assign the probability and determine the number of random numbers which
  // we will require -- do it once off here  
  // ..........................................................................  
  skiplist->_base._prob      = probability;
  skiplist->_base._numRandom = 0;
  switch (skiplist->_base._prob) {
    case TRI_SKIPLIST_PROB_HALF: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 32);
      if ((skiplist->_base._maxHeight % 32) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    case TRI_SKIPLIST_PROB_THIRD: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    case TRI_SKIPLIST_PROB_QUARTER: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    default: {
      assert(false);
      // todo: log error
      break;
    }    
  }  // end of switch statement
  
  // ..........................................................................  
  // Create storage for where to store the random numbers which we generated
  // do it here once off.
  // ..........................................................................  
  skiplist->_base._random = TRI_Allocate(sizeof(uint32_t) * skiplist->_base._numRandom);
  
  // ..........................................................................  
  // Assign the element size
  // ..........................................................................  
  skiplist->_base._elementSize = elementSize;

  
  // ..........................................................................  
  // Initialise the vertical storage of the lists and the place where we
  // are going to store elements
  // ..........................................................................  
  skiplist->_base._startNode._column          = NULL;
  skiplist->_base._startNode._colLength       = 0; 
  skiplist->_base._startNode._extraData       = NULL;
  skiplist->_base._startNode._element         = NULL;
  
  skiplist->_base._endNode._column            = NULL;
  skiplist->_base._endNode._colLength         = 0; 
  skiplist->_base._endNode._extraData         = NULL;
  skiplist->_base._endNode._element           = NULL;
  
  // ..........................................................................  
  // Whenever a probability of 1/2, 1/3, 1/4 is used, on average there will be
  // each node will have a height of two. So initialise the start and end nodes
  // with this 'average' height
  // ..........................................................................  
  GrowNodeHeight(&(skiplist->_base._startNode), 2);
  GrowNodeHeight(&(skiplist->_base._endNode), 2);

  // ..........................................................................  
  // Join the empty lists together
  // [N]<----------------------------------->[N]
  // [N]<----------------------------------->[N]
  // ..........................................................................  
  JoinNodes(&(skiplist->_base._startNode),&(skiplist->_base._endNode),0,1); // list 0 & 1  
}


void TRI_InitSkipListMulti (TRI_skiplist_multi_t* skiplist,
                       size_t elementSize,
                       int (*compareElementElement) (TRI_skiplist_multi_t*, void*, void*),
                       int (*compareKeyElement) (TRI_skiplist_multi_t*, void*, void*),
                       TRI_skiplist_prob_e probability,
                       uint32_t maximumHeight) {

  if (skiplist == NULL) {
    return;
  }
  
  // ..........................................................................  
  // Assign the comparision call back functions
  // ..........................................................................  
  
  skiplist->compareElementElement = compareElementElement;
  skiplist->compareKeyElement     = compareKeyElement;

  // ..........................................................................  
  // Assign the maximum height of the skip list. This maximum height must be
  // no greater than the absolute max height defined as a compile time parameter
  // ..........................................................................  
  skiplist->_base._maxHeight = maximumHeight;
  if (maximumHeight > SKIPLIST_ABSOLUTE_MAX_HEIGHT) {
    printf("%s:%d:Invalid maximum height for skiplist\n",__FILE__,__LINE__);
    assert(false);
  }  
  
  // ..........................................................................  
  // Assign the probability and determine the number of random numbers which
  // we will require -- do it once off here  
  // ..........................................................................  
  skiplist->_base._prob      = probability;
  skiplist->_base._numRandom = 0;
  switch (skiplist->_base._prob) {
    case TRI_SKIPLIST_PROB_HALF: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 32);
      if ((skiplist->_base._maxHeight % 32) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    case TRI_SKIPLIST_PROB_THIRD: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    case TRI_SKIPLIST_PROB_QUARTER: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    default: {
      assert(false);
      // todo: log error
      break;
    }    
  }  // end of switch statement
  
  // ..........................................................................  
  // Create storage for where to store the random numbers which we generated
  // do it here once off.
  // ..........................................................................  
  skiplist->_base._random = TRI_Allocate(sizeof(uint32_t) * skiplist->_base._numRandom);
  
  // ..........................................................................  
  // Assign the element size
  // ..........................................................................  
  skiplist->_base._elementSize = elementSize;

  
  // ..........................................................................  
  // Initialise the vertical storage of the lists and the place where we
  // are going to store elements
  // ..........................................................................  
  skiplist->_base._startNode._column          = NULL;
  skiplist->_base._startNode._colLength       = 0; 
  skiplist->_base._startNode._extraData       = NULL;
  skiplist->_base._startNode._element         = NULL;
  
  skiplist->_base._endNode._column            = NULL;
  skiplist->_base._endNode._colLength         = 0; 
  skiplist->_base._endNode._extraData         = NULL;
  skiplist->_base._endNode._element           = NULL;
  
  // ..........................................................................  
  // Whenever a probability of 1/2, 1/3, 1/4 is used, on average 
  // each node will have a height of two. So initialise the start and end nodes
  // with this 'average' height
  // ..........................................................................  
  GrowNodeHeight(&(skiplist->_base._startNode), 2);
  GrowNodeHeight(&(skiplist->_base._endNode), 2);

  // ..........................................................................  
  // Join the empty lists together
  // [N]<----------------------------------->[N]
  // [N]<----------------------------------->[N]
  // ..........................................................................  
  JoinNodes(&(skiplist->_base._startNode),&(skiplist->_base._endNode),0,1); // list 0 & 1  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipList (TRI_skiplist_t* skiplist) {
  TRI_skiplist_node_t* nextNode;
  TRI_skiplist_node_t* oldNextNode;

  if (skiplist == NULL) {
    return;
  }  
  
  nextNode = &(skiplist->_base._startNode);
  while (nextNode != NULL) {
    oldNextNode = nextNode->_column[0]._next;
    TRI_Free(nextNode->_column);
    if ((nextNode != &(skiplist->_base._startNode)) && (nextNode != &(skiplist->_base._endNode))) {
      TRI_Free(nextNode);
    }
    nextNode = oldNextNode;    
  }  
  TRI_Free(skiplist->_base._random);
}


void TRI_DestroySkipListMulti (TRI_skiplist_multi_t* skiplist) {
  TRI_skiplist_node_t* nextNode;
  TRI_skiplist_node_t* oldNextNode;

  if (skiplist == NULL) {
    return;
  }  
  
  nextNode = &(skiplist->_base._startNode);
  while (nextNode != NULL) {
    oldNextNode = nextNode->_column[0]._next;
    TRI_Free(nextNode->_column);
    if ((nextNode != &(skiplist->_base._startNode)) && (nextNode != &(skiplist->_base._endNode))) {
      TRI_Free(nextNode);
    }
    nextNode = oldNextNode;    
  }  
  TRI_Free(skiplist->_base._random);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipList (TRI_skiplist_t* skiplist) {
  TRI_DestroySkipList(skiplist);
  TRI_Free(skiplist);
}

void TRI_FreeSkipListMulti (TRI_skiplist_multi_t* skiplist) {
  TRI_DestroySkipListMulti(skiplist);
  TRI_Free(skiplist);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipList (TRI_skiplist_t* skiplist, void* key) {
  assert(false);
  return NULL;
}

TRI_vector_pointer_t TRI_LookupByKeySkipListMulti (TRI_skiplist_multi_t* skiplist, void* key) {

  TRI_vector_pointer_t result;
  int32_t level;
  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  int compareResult;
  
  TRI_InitVectorPointer(&result);
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return result;
  }


  // ...........................................................................  
  // Determine the starting level and the starting node
  // ...........................................................................  
  
  currentLevel = skiplist->_base._startNode._colLength - 1;
  currentNode  = &(skiplist->_base._startNode);

  //printf("%s:%d:!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:%u\n",__FILE__,__LINE__,currentLevel);
  
  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    nextNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._next);



    // .........................................................................  
    // WE HAVE FOUR CASES TO CONSIDER
    // .........................................................................  
    
    // .........................................................................  
    // CASE ONE: 
    // At this level we have the smallest (start) and largest (end) nodes ONLY.
    // CASE TWO:
    // We have arrived at the end of the nodes and we are not at the 
    // start of the nodes either.
    // .........................................................................  
    
    if (nextNode == &(skiplist->_base._endNode)) {    
    
      // .......................................................................
      // We are at the lowest level of the lists, and we haven't found the item
      // yet. Eventually we would like to return iterators.
      // .......................................................................
      if (currentLevel == 0) {
        return result;
      }
      
      // .......................................................................
      // We have not yet reached the lowest level continue down.
      // .......................................................................
      --currentLevel;
      
      goto START;
    }  


    
    // .........................................................................  
    // CASE THREE:
    // We are the smallest left most node and the NEXT node is NOT the end node.
    // Compare this element with the element in the right node to see what we do.
    // CASE FOUR:
    // We are somewhere in the middle of a list, away from the smallest and 
    // largest nodes.
    // .........................................................................  
    
    else { // nextNode != &(skiplist->_endNode)
      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->compareKeyElement(skiplist,key,&(nextNode->_element));   
      
      // .......................................................................    
      // We have found the item! Treat it like a bigger item
      // .......................................................................    
      if (compareResult == 0) {
  //printf("%s:%d:!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:%u\n",__FILE__,__LINE__,currentLevel);
      }
      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult > 0) {
        currentNode = nextNode;
        goto START;
      }  
              

      // .......................................................................    
      // We have reached the lowest level of the lists -- no such item.
      // .......................................................................    
      if (currentLevel == 0 && compareResult != 0) {
  //printf("%s:%d:!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:%u\n",__FILE__,__LINE__,currentLevel);
        return result;
      }
      
      if (currentLevel == 0 && compareResult == 0) {
  //printf("%s:%d:!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:%u\n",__FILE__,__LINE__,currentLevel);
        currentNode = nextNode;
        goto END;
      }
      
      // .......................................................................    
      // Drop down the list
      // .......................................................................    
      --currentLevel;
      
      goto START;
    }  
    

    
  END:

  while (true) {
    TRI_PushBackVectorPointer(&result, &(currentNode->_element));           
    nextNode = (TRI_skiplist_node_t*)(currentNode->_column[0]._next);
    compareResult = skiplist->compareKeyElement(skiplist,key,&(nextNode->_element));   
    if (compareResult != 0) {
      break;
    }
    else {
      currentNode = nextNode;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByKeySkipList (TRI_skiplist_t* skiplist, void* key) {
  assert(false);
  return NULL;
}

TRI_vector_pointer_t TRI_FindByKeySkipListMulti (TRI_skiplist_multi_t* skiplist, void* key) {
  assert(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementSkipList (TRI_skiplist_t* skiplist, void* element) {  

  int32_t level;
  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  int compareResult;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return false;
  }


  // ...........................................................................  
  // Determine the starting level and the starting node
  // ...........................................................................  
  
  currentLevel = skiplist->_base._startNode._colLength - 1;
  currentNode  = &(skiplist->_base._startNode);

  
  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    nextNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._next);



    // .........................................................................  
    // WE HAVE FOUR CASES TO CONSIDER
    // .........................................................................  
    
    // .........................................................................  
    // CASE ONE: 
    // At this level we have the smallest (start) and largest (end) nodes ONLY.
    // CASE TWO:
    // We have arrived at the end of the nodes and we are not at the 
    // start of the nodes either.
    // .........................................................................  
    
    if (nextNode == &(skiplist->_base._endNode)) {    
    
    //printf("%s:%u:%u\n",__FILE__,__LINE__,currentLevel);
    
      // .......................................................................
      // We are at the lowest level of the lists, and we haven't found the item
      // yet. Eventually we would like to return iterators.
      // .......................................................................
      if (currentLevel == 0) {
        return NULL;
      }
      
      // .......................................................................
      // We have not yet reached the lowest level continue down.
      // .......................................................................
      --currentLevel;
      
      goto START;
    }  


    
    // .........................................................................  
    // CASE THREE:
    // We are the smallest left most node and the NEXT node is NOT the end node.
    // Compare this element with the element in the right node to see what we do.
    // CASE FOUR:
    // We are somewhere in the middle of a list, away from the smallest and 
    // largest nodes.
    // .........................................................................  
    
    else { // nextNode != &(skiplist->_endNode
      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->compareElementElement(skiplist,element,&(nextNode->_element));   
      
    //printf("%s:%u:%u:%u\n",__FILE__,__LINE__,currentLevel,compareResult);
      
      // .......................................................................    
      // We have found the item!
      // .......................................................................    
      if (compareResult == 0) {
        return &(nextNode->_element);
      }
      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult > 0) {
        currentNode = nextNode;
        goto START;
      }  
              

      // .......................................................................    
      // We have reached the lowest level of the lists -- no such item.
      // .......................................................................    
      if (currentLevel == 0) {
        return NULL;
      }
      
      // .......................................................................    
      // Drop down the list
      // .......................................................................    
      --currentLevel;
      
      goto START;
    }  
    

    
  END:

  assert(false); // there is no way we can be here  
  return NULL;
}


TRI_vector_pointer_t TRI_LookupByElementSkipListMulti (TRI_skiplist_multi_t* skiplist, void* element) {  
  assert(false);
}
////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByElementSkipList (TRI_skiplist_t* skiplist, void* element) {
  return NULL;
}

TRI_vector_pointer_t TRI_FindByElementSkipListMulti (TRI_skiplist_multi_t* skiplist, void* element) {
  assert(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementSkipList (TRI_skiplist_t* skiplist, void* element, bool overwrite) {
  
  int32_t newHeight;
  int32_t currentLevel;
  uint32_t oldColLength;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  TRI_skiplist_node_t* newNode;
  TRI_skiplist_node_t* tempLeftNode;
  TRI_skiplist_node_t* tempRightNode;
  int compareResult;
  int j;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return false;
  }

  
  // ...........................................................................  
  // Determine the number of levels in which to add the item. That is, determine
  // the height of the node so that it participates in that many lists.
  // ...........................................................................  
  
  newHeight = RandLevel(&(skiplist->_base));

  // ...........................................................................  
  // Something wrong since the newHeight must be non-negative
  // ...........................................................................  
  
  if (newHeight < 0) {
    return false;
  }  
  
  // ...........................................................................  
  // convert the level to a height
  // ...........................................................................  
  newHeight += 1;
  
  /*
  printf("%s:%u:%u\n",__FILE__,__LINE__,newHeight);  
  currentNode  = &(skiplist->_base._startNode);
  for (int j = currentNode->_colLength - 1; j > -1; --j) {
    printf("%s:%d:!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:%d:%u:%u:%u:%u:%u\n",__FILE__,__LINE__,
           j,
           currentNode->_colLength,
           (uint64_t)((TRI_skiplist_node_t*)(currentNode->_column[j]._prev)),
           (uint64_t)(&(skiplist->_base._startNode)),
           (uint64_t)((TRI_skiplist_node_t*)(currentNode->_column[j]._next)),
           (uint64_t)(&(skiplist->_base._endNode))
           );
  }
  */
    
  // ...........................................................................  
  // Grow lists if required by increasing the height of the start and end nodes
  // ...........................................................................  
  oldColLength = skiplist->_base._startNode._colLength;
  if ((uint32_t)(newHeight) > oldColLength) {
    GrowNodeHeight(&(skiplist->_base._startNode), newHeight);
    GrowNodeHeight(&(skiplist->_base._endNode), newHeight);
    JoinNodes(&(skiplist->_base._startNode),&(skiplist->_base._endNode), oldColLength , newHeight - 1); 
  }
  

  // ...........................................................................  
  // Create the new node to be inserted. If there is some sort of failure,
  // then we delete the node memory.
  // ...........................................................................  
  newNode = TRI_Allocate(sizeof(TRI_skiplist_node_t) + skiplist->_base._elementSize);
  if (newNode == NULL) { // out of memory?
    return false;
  }
  
  newNode->_extraData = NULL;
  newNode->_colLength = 0;  
  memcpy(&(newNode->_element),element,skiplist->_base._elementSize);
  GrowNodeHeight(newNode, newHeight);

  
  // ...........................................................................  
  // Determine the path where the new item is to be inserted. If the item 
  // already exists either replace it or return false. Recall that this 
  // skip list is used for unique key/value pairs. Use the skiplist-multi 
  // non-unique key/value pairs.
  // ...........................................................................  
  currentLevel = skiplist->_base._startNode._colLength - 1; // NOT current height BUT current level
  currentNode  = &(skiplist->_base._startNode);

  
  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    nextNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._next);


    // .........................................................................  
    // WE HAVE FOUR CASES TO CONSIDER
    // .........................................................................  
    
    // .........................................................................  
    // CASE ONE: 
    // At this level we have the smallest (start) and largest (end) nodes ONLY.
    // CASE TWO:
    // We have arrived at the end of the nodes and we are not at the 
    // start of the nodes either.
    // .........................................................................  
    
    if (nextNode == &(skiplist->_base._endNode)) {    
      
      
      // .......................................................................
      // Store the current node and level in the path
      // .......................................................................
      if (currentLevel < newHeight) {
        newNode->_column[currentLevel]._prev = currentNode;
      }    
    
      // .......................................................................
      // if we are at the lowest level of the lists, insert the item to the 
      // right of the current node
      // .......................................................................
      if (currentLevel == 0) {
        goto END;
      }
      
      // .......................................................................
      // We have not yet reached the lowest level continue down.
      // .......................................................................
      --currentLevel;
      
      goto START;
    }  


    
    // .........................................................................  
    // CASE THREE:
    // We are the smallest left most node and the NEXT node is NOT the end node.
    // Compare this element with the element in the right node to see what we do.
    // CASE FOUR:
    // We are somewhere in the middle of a list, away from the smallest and 
    // largest nodes.
    // .........................................................................  
    
    else { // nextNode != &(skiplist->_endNode
    
      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->compareElementElement(skiplist,element,&(nextNode->_element));   
      
  //printf("%s:%u:%u:%u:%u\n",__FILE__,__LINE__,newHeight,currentLevel,compareResult);  
      
      // .......................................................................    
      // The element matches the next element. Overwrite if possible and return.
      // We do not allow non-unique elements.
      // .......................................................................    
      if (compareResult == 0) {
        printf("%s:%u:should not happen\n",__FILE__,__LINE__);
        if (overwrite) {
         memcpy(&(nextNode->_element),element,skiplist->_base._elementSize);
         TRI_FreeSkipListNode(&(skiplist->_base), newNode);
         return true;       
        }
        TRI_FreeSkipListNode(&(skiplist->_base), newNode);
        return false;
      }
      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult > 0) {
        currentNode = nextNode;
        goto START;
      }  
              

      // .......................................................................    
      // The element is less than the next node. Can we drop down the list?
      // Store the current node and level in the path.
      // .......................................................................
      if (currentLevel < newHeight) {
        newNode->_column[currentLevel]._prev = currentNode;
      }
      
      // .......................................................................    
      // We have reached the lowest level of the lists. Time to insert item.
      // .......................................................................    
      if (currentLevel == 0) {
        goto END;
      }
      
      // .......................................................................    
      // Drop down the list
      // .......................................................................    
      --currentLevel;
      
      goto START;
    }  
    

    
  END:

  // ..........................................................................
  // Ok finished with the loop and we should have a path with AT MOST
  // SKIPLIST_ABSOLUTE_MAX_HEIGHT number of elements.
  // ..........................................................................
  
  
  for (j = 0; j < newHeight; ++j) {
    tempLeftNode  = newNode->_column[j]._prev;
    tempRightNode = tempLeftNode->_column[j]._next;
    JoinNodes(tempLeftNode, newNode, j, j);
    JoinNodes(newNode, tempRightNode, j, j);
  }  

  return true;
}


bool TRI_InsertElementSkipListMulti (TRI_skiplist_multi_t* skiplist, void* element, bool overwrite) {
 
  int32_t newHeight;
  int32_t currentLevel;
  int pathPosition;
  uint32_t oldColLength;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  TRI_skiplist_node_t* newNode;
  TRI_skiplist_node_t* tempLeftNode;
  TRI_skiplist_node_t* tempRightNode;
  int compareResult;
  int j;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return false;
  }

  
  // ...........................................................................  
  // Determine the number of levels in which to add the item. That is, determine
  // the height of the node so that it participates in that many lists.
  // ...........................................................................  
  
  newHeight = RandLevel(&(skiplist->_base));
  
  // ...........................................................................  
  // Something wrong since the newHeight must be non-negative
  // ...........................................................................  
  
  if (newHeight < 0) {
    return false;
  }  
    
  // ...........................................................................  
  // Convert level to height
  // ...........................................................................  
  newHeight += 1;
  
  // ...........................................................................  
  // Grow lists if required by increasing the height of the start and end nodes
  // ...........................................................................  
  oldColLength = skiplist->_base._startNode._colLength;
  if ((uint32_t)(newHeight) > oldColLength) {
  
    GrowNodeHeight(&(skiplist->_base._startNode), newHeight);
    GrowNodeHeight(&(skiplist->_base._endNode), newHeight);
    JoinNodes(&(skiplist->_base._startNode),&(skiplist->_base._endNode), oldColLength, newHeight - 1); 
  }
  

  // ...........................................................................  
  // Create the new node to be inserted. If there is some sort of failure,
  // then we delete the node memory.
  // ...........................................................................  
  newNode = TRI_Allocate(sizeof(TRI_skiplist_node_t) + skiplist->_base._elementSize);
  if (newNode == NULL) { // out of memory?
    return false;
  }
  
  newNode->_extraData = NULL;
  newNode->_colLength = 0;  
  memcpy(&(newNode->_element),element,skiplist->_base._elementSize);
  GrowNodeHeight(newNode, newHeight);
  
  // ...........................................................................  
  // Recall that this 
  // skip list is used for unique key/value pairs. Use the skiplist-multi 
  // non-unique key/value pairs.
  // ...........................................................................  
  currentLevel = skiplist->_base._startNode._colLength - 1; // NOT current height BUT current level
  currentNode  = &(skiplist->_base._startNode);

  
  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    nextNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._next);


    // .........................................................................  
    // WE HAVE FOUR CASES TO CONSIDER
    // .........................................................................  
    
    // .........................................................................  
    // CASE ONE: 
    // At this level we have the smallest (start) and largest (end) nodes ONLY.
    // CASE TWO:
    // We have arrived at the end of the nodes and we are not at the 
    // start of the nodes either.
    // .........................................................................  
    
    if (nextNode == &(skiplist->_base._endNode)) {    
      
      
      // .......................................................................
      // Store the current node in the current level
      // .......................................................................
      if (currentLevel < newHeight) {
        newNode->_column[currentLevel]._prev = currentNode;
      }
      
      // .......................................................................
      // We are at the lowest level of the lists, insert the item to the 
      // right of the current node
      // .......................................................................
      if (currentLevel == 0) {
        goto END;
      }
      
      // .......................................................................
      // We have not yet reached the lowest level continue down.
      // .......................................................................
      --currentLevel;
      
      goto START;
    }  


    
    // .........................................................................  
    // CASE THREE:
    // We are the smallest left most node and the NEXT node is NOT the end node.
    // Compare this element with the element in the right node to see what we do.
    // CASE FOUR:
    // We are somewhere in the middle of a list, away from the smallest and 
    // largest nodes.
    // .........................................................................  
    
    else { // nextNode != &(skiplist->_endNode)
    
      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->compareElementElement(skiplist,element,&(nextNode->_element));   
      
      // .......................................................................    
      // The element is the same
      // .......................................................................    
      if (compareResult == 0) {
        printf("%s:%u:should not happen\n",__FILE__,__LINE__);
        if (overwrite) {
         memcpy(&(nextNode->_element),element,skiplist->_base._elementSize);
         TRI_FreeSkipListNode(&(skiplist->_base), newNode);
         return true;       
        }
        TRI_FreeSkipListNode(&(skiplist->_base), newNode);
        return false;
      }
      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult > 0) {
        currentNode = nextNode;
        goto START;
      }  
              

      // .......................................................................    
      // The element is less than the next node. Can we drop down the list?
      // Store the current node and level in the path.
      // .......................................................................
      if (currentLevel < newHeight) {
        newNode->_column[currentLevel]._prev = currentNode;
      }  
      
      // .......................................................................    
      // We have reached the lowest level of the lists. Time to insert item.
      // .......................................................................    
      if (currentLevel == 0) {
        goto END;
      }
      
      // .......................................................................    
      // Drop down the list
      // .......................................................................    
      --currentLevel;
      
      goto START;
    }  
    

    
  END:

  for (j = 0; j < newHeight; ++j) {
    tempLeftNode  = newNode->_column[j]._prev;
    tempRightNode = tempLeftNode->_column[j]._next;
    JoinNodes(tempLeftNode, newNode, j, j);
    JoinNodes(newNode, tempRightNode, j, j);
  }  

  return true;
}
////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeySkipList (TRI_skiplist_t* skiplist, void* key, void* element, bool overwrite) {
  return false;
}

bool TRI_InsertKeySkipListMulti (TRI_skiplist_multi_t* skiplist, void* key, void* element, bool overwrite) {
 
  int32_t newHeight;
  int32_t currentLevel;
  uint32_t oldColLength;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  TRI_skiplist_node_t* newNode;
  TRI_skiplist_node_t* tempLeftNode;
  TRI_skiplist_node_t* tempRightNode;
  int compareResult;
  int j;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return false;
  }

  
  // ...........................................................................  
  // Determine the number of levels in which to add the item. That is, determine
  // the height of the node so that it participates in that many lists.
  // ...........................................................................  
  
  newHeight = RandLevel(&(skiplist->_base));
  
  // ...........................................................................  
  // Something wrong since the newHeight must be non-negative
  // ...........................................................................  
  
  if (newHeight < 0) {
    return false;
  }  
    
  
  // ...........................................................................  
  // Convert level to height
  // ...........................................................................  
  newHeight += 1;
  
  // ...........................................................................  
  // Grow lists if required by increasing the height of the start and end nodes
  // ...........................................................................  
  oldColLength = skiplist->_base._startNode._colLength;
  if ((uint32_t)(newHeight) > oldColLength) {  
    GrowNodeHeight(&(skiplist->_base._startNode), newHeight);
    GrowNodeHeight(&(skiplist->_base._endNode), newHeight);
    JoinNodes(&(skiplist->_base._startNode),&(skiplist->_base._endNode), oldColLength, newHeight - 1); 
  }
  

  // ...........................................................................  
  // Create the new node to be inserted. If there is some sort of failure,
  // then we delete the node memory.
  // ...........................................................................  
  newNode = TRI_Allocate(sizeof(TRI_skiplist_node_t) + skiplist->_base._elementSize);
  if (newNode == NULL) { // out of memory?
    return false;
  }
  
  newNode->_extraData = NULL;
  newNode->_colLength = 0;  
  memcpy(&(newNode->_element),element,skiplist->_base._elementSize);
  GrowNodeHeight(newNode, newHeight);
  
  // ...........................................................................  
  // Recall that this 
  // skip list is used for unique key/value pairs. Use the skiplist-multi 
  // non-unique key/value pairs.
  // ...........................................................................  
  currentLevel = skiplist->_base._startNode._colLength - 1; // NOT current height BUT current level
  currentNode  = &(skiplist->_base._startNode);

  
  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    nextNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._next);


    // .........................................................................  
    // WE HAVE FOUR CASES TO CONSIDER
    // .........................................................................  
    
    // .........................................................................  
    // CASE ONE: 
    // At this level we have the smallest (start) and largest (end) nodes ONLY.
    // CASE TWO:
    // We have arrived at the end of the nodes and we are not at the 
    // start of the nodes either.
    // .........................................................................  
    
    if (nextNode == &(skiplist->_base._endNode)) {    
      
      
      // .......................................................................
      // Store the current node in the current level
      // .......................................................................
      if (currentLevel < newHeight) {
        newNode->_column[currentLevel]._prev = currentNode;
      }
      
      // .......................................................................
      // We are at the lowest level of the lists, insert the item to the 
      // right of the current node
      // .......................................................................
      if (currentLevel == 0) {
        goto END;
      }
      
      // .......................................................................
      // We have not yet reached the lowest level continue down.
      // .......................................................................
      --currentLevel;
      
      goto START;
    }  


    
    // .........................................................................  
    // CASE THREE:
    // We are the smallest left most node and the NEXT node is NOT the end node.
    // Compare this element with the element in the right node to see what we do.
    // CASE FOUR:
    // We are somewhere in the middle of a list, away from the smallest and 
    // largest nodes.
    // .........................................................................  
    
    else { // nextNode != &(skiplist->_endNode)
    
      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->compareKeyElement(skiplist,key,&(nextNode->_element));   

      // .......................................................................    
      // The keys match element we just assume that the element to insert 
      // is smaller. We should ensure that the elements are not the same
      // .......................................................................    
      if (compareResult == 0) {
      }
      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult > 0) {
        currentNode = nextNode;
        goto START;
      }  
              

      // .......................................................................    
      // The element is less than the next node. Can we drop down the list?
      // Store the current node and level in the path.
      // .......................................................................
      if (currentLevel < newHeight) {
        newNode->_column[currentLevel]._prev = currentNode;
      }  
      
      // .......................................................................    
      // We have reached the lowest level of the lists. Time to insert item.
      // .......................................................................    
      if (currentLevel == 0) {
        goto END;
      }
      
      // .......................................................................    
      // Drop down the list
      // .......................................................................    
      --currentLevel;
      
      goto START;
    }  
    

    
  END:

  
  for (j = 0; j < newHeight; ++j) {
    tempLeftNode  = newNode->_column[j]._prev;
    tempRightNode = tempLeftNode->_column[j]._next;
    JoinNodes(tempLeftNode, newNode, j, j);
    JoinNodes(newNode, tempRightNode, j, j);
  }  
  
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list.
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementSkipList (TRI_skiplist_t* skiplist, void* element, void* old) {

  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  TRI_skiplist_node_t* tempLeftNode;
  TRI_skiplist_node_t* tempRightNode;
  int compareResult;
  unsigned int j;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return false;
  }


  // ...........................................................................  
  // Start at the top most list and left most position of that list.
  // ...........................................................................  
  currentLevel = skiplist->_base._startNode._colLength - 1; // current level not height
  currentNode  = &(skiplist->_base._startNode);

  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    nextNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._next);


    // .........................................................................  
    // WE HAVE FOUR CASES TO CONSIDER
    // .........................................................................  
    
    // .........................................................................  
    // CASE ONE: 
    // At this level we have the smallest (start) and largest (end) nodes ONLY.
    // CASE TWO:
    // We have arrived at the end of the nodes and we are not at the 
    // start of the nodes either.
    // .........................................................................  
    
    if (nextNode == &(skiplist->_base._endNode)) {    
    
      // .......................................................................
      // We are at the lowest level of the lists, and we haven't found the item
      // yet. Nothing to remove so return.
      // .......................................................................
      if (currentLevel == 0) {
        return false;
      }
      
      // .......................................................................
      // We have not yet reached the lowest level continue down.
      // .......................................................................
      --currentLevel;
      
      goto START;
    }  


    
    // .........................................................................  
    // CASE THREE:
    // We are the smallest left most node and the NEXT node is NOT the end node.
    // Compare this element with the element in the right node to see what we do.
    // CASE FOUR:
    // We are somewhere in the middle of a list, away from the smallest and 
    // largest nodes.
    // .........................................................................  
    
    else { // nextNode != &(skiplist->_endNode
    
      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->compareElementElement(skiplist,element,&(nextNode->_element));   
      
      // .......................................................................    
      // We have found the item!
      // .......................................................................    
      if (compareResult == 0) {
        currentNode = nextNode;
        goto END;          
      }
      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult > 0) {
        currentNode = nextNode;
        goto START;
      }  
              
              
      // .......................................................................    
      // We have reached the lowest level of the lists -- no such item.
      // .......................................................................    
      if (currentLevel == 0) {
        return false;
      }
      
      // .......................................................................    
      // Drop down the list
      // .......................................................................    
      --currentLevel;
      
      goto START;
    }  
    

    
  END:
  
  
  // ..........................................................................
  // If requested copy the contents of the element we have located into the
  // storage sent.
  // ..........................................................................
  
  if (old != NULL) {
    memcpy(old, &(currentNode->_element), skiplist->_base._elementSize);
  }
  
  
  // ..........................................................................
  // Attempt to remove element
  // ..........................................................................
    
  
  for (j = 0; j < currentNode->_colLength; ++j) {
    tempLeftNode  = currentNode->_column[j]._prev;
    tempRightNode = currentNode->_column[j]._next;
    JoinNodes(tempLeftNode, tempRightNode, j, j);
  }  

  TRI_Free(currentNode->_column);
  TRI_Free(currentNode);
  
  return true;
 
}

bool TRI_RemoveElementSkipListMulti (TRI_skiplist_multi_t* skiplist, void* element, void* old) {

  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  TRI_skiplist_node_t* tempLeftNode;
  TRI_skiplist_node_t* tempRightNode;
  int compareResult;
  unsigned int j;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return false;
  }


  // ...........................................................................  
  // Start at the top most list and left most position of that list.
  // ...........................................................................  
  currentLevel = skiplist->_base._startNode._colLength - 1; // current level not height
  currentNode  = &(skiplist->_base._startNode);

  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    nextNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._next);


    // .........................................................................  
    // WE HAVE FOUR CASES TO CONSIDER
    // .........................................................................  
    
    // .........................................................................  
    // CASE ONE: 
    // At this level we have the smallest (start) and largest (end) nodes ONLY.
    // CASE TWO:
    // We have arrived at the end of the nodes and we are not at the 
    // start of the nodes either.
    // .........................................................................  
    
    if (nextNode == &(skiplist->_base._endNode)) {    
    
      // .......................................................................
      // We are at the lowest level of the lists, and we haven't found the item
      // yet. Nothing to remove so return.
      // .......................................................................
      if (currentLevel == 0) {
        return false;
      }
      
      // .......................................................................
      // We have not yet reached the lowest level continue down.
      // .......................................................................
      --currentLevel;
      
      goto START;
    }  


    
    // .........................................................................  
    // CASE THREE:
    // We are the smallest left most node and the NEXT node is NOT the end node.
    // Compare this element with the element in the right node to see what we do.
    // CASE FOUR:
    // We are somewhere in the middle of a list, away from the smallest and 
    // largest nodes.
    // .........................................................................  
    
    else { // nextNode != &(skiplist->_endNode
    
      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->compareKeyElement(skiplist,element,&(nextNode->_element));   
      
      // .......................................................................    
      // We have found an item which matches the key
      // .......................................................................    
      if (compareResult == 0) {
      }

      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult > 0) {
        currentNode = nextNode;
        goto START;
      }  
              
              
      // .......................................................................    
      // We have reached the lowest level of the lists -- no such item.
      // .......................................................................    
      if (currentLevel == 0) {
        goto END;
      }
      
      // .......................................................................    
      // Drop down the list
      // .......................................................................    
      --currentLevel;
      
      goto START;
    }  
    

    
  END:
  
  
  // ..........................................................................
  // Find the actual node we are deleting
  // ..........................................................................
  
  currentNode = NULL;
  while (nextNode != &(skiplist->_base._endNode)) {
    compareResult = skiplist->compareElementElement(skiplist,element,&(nextNode->_element));   
    if (compareResult != 0) {
      nextNode = nextNode->_column[0]._next;
    }
    else {
      currentNode = nextNode;
      break;
    }    
  }
  
  // ..........................................................................
  // could not find the actual node 
  // ..........................................................................
  
  if (currentNode == NULL) {
    printf("%s:%u\n:could not find node",__FILE__,__LINE__);
    return false;
  }  

  if (old != NULL) {
    memcpy(old, &(currentNode->_element), skiplist->_base._elementSize);
  }
    
  // ..........................................................................
  // remove element 
  // ..........................................................................
      
  for (j = 0; j < currentNode->_colLength; ++j) {
    tempLeftNode  = currentNode->_column[j]._prev;
    tempRightNode = currentNode->_column[j]._next;
    JoinNodes(tempLeftNode, tempRightNode, j, j);
  }  

  TRI_Free(currentNode->_column);
  TRI_Free(currentNode);
  
  return true;
 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeySkipList (TRI_skiplist_t* skiplist, void* key, void* old) {
  assert(false);
  return false;
}

bool TRI_RemoveKeySkipListMulti (TRI_skiplist_multi_t* skiplist, void* key, void* old) {
  assert(false);
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
