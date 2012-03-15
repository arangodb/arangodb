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
// --SECTION--                                                          SKIPLIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                          common private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_common
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

static void TRI_DestroyBaseSkipList(TRI_skiplist_base_t* baseSkiplist) {
  TRI_skiplist_node_t* nextNode;
  TRI_skiplist_node_t* oldNextNode;

  if (baseSkiplist == NULL) {
    return;
  }  
  
  nextNode = &(baseSkiplist->_startNode);
  while (nextNode != NULL) {
    oldNextNode = nextNode->_column[0]._next;
    TRI_Free(nextNode->_column);
    if ((nextNode != &(baseSkiplist->_startNode)) && (nextNode != &(baseSkiplist->_endNode))) {
      TRI_Free(nextNode);
    }
    nextNode = oldNextNode;    
  }  
  TRI_Free(baseSkiplist->_random);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the internal structure allocation for a node
////////////////////////////////////////////////////////////////////////////////

static void TRI_DestroySkipListNode (TRI_skiplist_node_t* node) {

  if (node == NULL) {
    return;
  }  
  
  TRI_Free(node->_column);
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
/// @brief frees a node, destroying it first
////////////////////////////////////////////////////////////////////////////////

static void TRI_FreeSkipListNode (TRI_skiplist_base_t* skiplist, TRI_skiplist_node_t* node) {
  TRI_DestroySkipListNode(node);     
  if ( (node == &(skiplist->_startNode)) ||
     (node == &(skiplist->_endNode)) ) {
    return;   
  }  
  TRI_Free(node);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief joins a left node and right node together
////////////////////////////////////////////////////////////////////////////////

static void JoinNodes(TRI_skiplist_node_t* leftNode, TRI_skiplist_node_t* rightNode, uint32_t startLevel, uint32_t endLevel) {
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



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void* TRI_NextNodeBaseSkipList(TRI_skiplist_base_t* skiplist, void* currentNode) {
  TRI_skiplist_node_t* node;
  
  if (currentNode == NULL) {
    return &(skiplist->_startNode);
  }  
  if (currentNode == &(skiplist->_endNode)) {
    return NULL;
  }  
  node = (TRI_skiplist_node_t*)(currentNode);
  return(node->_column[0]._next);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void* TRI_PrevNodeBaseSkipList(TRI_skiplist_base_t* skiplist, void* currentNode) {
  TRI_skiplist_node_t* node;
  
  if (currentNode == NULL) {
    return &(skiplist->_endNode);
  }  
  if (currentNode == &(skiplist->_startNode)) {
    return NULL;
  }  
  node = (TRI_skiplist_node_t*)(currentNode);
  return(node->_column[0]._prev);
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
/// @}
////////////////////////////////////////////////////////////////////////////////





// -----------------------------------------------------------------------------
// --SECTION--                      unique skiplist constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_unique
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an skip list
////////////////////////////////////////////////////////////////////////////////

void TRI_InitSkipList (TRI_skiplist_t* skiplist, size_t elementSize,
                       int (*compareElementElement) (TRI_skiplist_t*, void*, void*, int),
                       int (*compareKeyElement) (TRI_skiplist_t*, void*, void*, int),
                       TRI_skiplist_prob_e probability, uint32_t maximumHeight) {

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



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipList (TRI_skiplist_t* skiplist) {
  if (skiplist != NULL) {
    TRI_DestroyBaseSkipList( (TRI_skiplist_base_t*)(skiplist) );
  }  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipList (TRI_skiplist_t* skiplist) {
  TRI_DestroySkipList(skiplist);
  TRI_Free(skiplist);
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                  unique skiplist public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_unique
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the end node associated with a skip list
////////////////////////////////////////////////////////////////////////////////

void* TRI_EndNodeSkipList(TRI_skiplist_t* skiplist) {
  if (skiplist != NULL) {
    return &(skiplist->_base._endNode);
  }
  return NULL;  
}  



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the skip list 
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementSkipList(TRI_skiplist_t* skiplist, void* element, bool overwrite) {
  // Use TRI_InsertKeySkipList instead of calling this method
  assert(false);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeySkipList (TRI_skiplist_t* skiplist, void* key, void* element, bool overwrite) {
  //This uses the compareKeyElement callback  
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
  

  // ...........................................................................  
  // Copy the contents of element into the new node to be inserted.
  // If a duplicate has been found, then we destroy the allocated memory.
  // ...........................................................................  
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
  currentLevel = skiplist->_base._startNode._colLength - 1; // NOT current height BUT current level is required here
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
      compareResult = skiplist->compareKeyElement(skiplist,key,&(nextNode->_element), 0);   
      
      // .......................................................................    
      // The element matches the next element. Overwrite if possible and return.
      // We do not allow non-unique elements.
      // .......................................................................    
      if (compareResult == 0) {
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



////////////////////////////////////////////////////////////////////////////////
/// @brief returns greatest node less than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipList (TRI_skiplist_t* skiplist, void* key) {

  int32_t level;
  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  int compareResult;

  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return NULL;
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
    
    
      // .......................................................................
      // We are at the lowest level of the lists, and we haven't found the item
      // yet. 
      // .......................................................................
      if (currentLevel == 0) {
        return currentNode;
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
      compareResult = skiplist->compareKeyElement(skiplist,key,&(nextNode->_element), -1);   
    
      // .......................................................................    
      // -1 is returned if the number of fields (attributes) in the key is LESS
      // than the number of fields in the index definition. This has the effect
      // of being slightly less efficient since we have to proceed to the level
      // 0 list in the set of skip lists.
      // .......................................................................    
    
      
      // .......................................................................    
      // We have found the item!
      // .......................................................................    
      if (compareResult == 0) {
        assert(false);
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
        return currentNode;
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



////////////////////////////////////////////////////////////////////////////////
/// @brief locate a node using an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementSkipList (TRI_skiplist_t* skiplist, void* element) {  
  assert(false); // there is no way we can be here
} 



////////////////////////////////////////////////////////////////////////////////
/// @brief returns node which matches a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipList (TRI_skiplist_t* skiplist, void* key) {

  int32_t level;
  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  int compareResult;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return NULL;
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
      compareResult = skiplist->compareKeyElement(skiplist,key,&(nextNode->_element), 0);   
      
      // .......................................................................    
      // We have found the item!
      // .......................................................................    
      if (compareResult == 0) {
        return nextNode;
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



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipList(TRI_skiplist_t* skiplist, void* currentNode) {
  if (skiplist != NULL) {
    return TRI_NextNodeBaseSkipList( (TRI_skiplist_base_t*)(skiplist), currentNode);
  }
  return NULL;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the previous node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipList(TRI_skiplist_t* skiplist, void* currentNode) {
  if (skiplist != NULL) {
    return TRI_PrevNodeBaseSkipList( (TRI_skiplist_base_t*)(skiplist), currentNode);
  }  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementSkipList (TRI_skiplist_t* skiplist, void* element, void* old) {
  // This uses the compareElementElement callback
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
      compareResult = skiplist->compareElementElement(skiplist,element,&(nextNode->_element), 0);   
      
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



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeySkipList (TRI_skiplist_t* skiplist, void* key, void* old) {
  // Use the TRI_RemoveElementSkipList method instead.
  assert(false);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns smallest node greater than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipList (TRI_skiplist_t* skiplist, void* key) {

  int32_t level;
  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* prevNode;
  int compareResult;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return NULL;
  }


  // ...........................................................................  
  // Determine the starting level and the starting node
  // ...........................................................................  
  
  currentLevel = skiplist->_base._startNode._colLength - 1;
  currentNode  = &(skiplist->_base._endNode);

  
  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    prevNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._prev);



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
    
    if (prevNode == &(skiplist->_base._startNode)) {    
    
      // .......................................................................
      // We are at the lowest level of the lists, and we haven't found the item
      // yet. Eventually we would like to return iterators.
      // .......................................................................
      if (currentLevel == 0) {
        return currentNode;
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
      compareResult = skiplist->compareKeyElement(skiplist,key,&(prevNode->_element),1);   
      

      // .......................................................................    
      // If the number of fields (attributes) in the key is LESS than the number
      // of fields in the element to be compared to, then EVEN if the keys which
      // which are common to both equate as EQUAL, we STILL return 1 rather than
      // 0! This ensures that the right interval end point is correctly positioned
      // -- slightly inefficient since the lowest level skip list 0 has to be reached
      // in this case.
      // .......................................................................    
      
            
      // .......................................................................    
      // We have found the item!
      // .......................................................................    
      if (compareResult == 0) {
        assert(false);
      }
      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult < 0) {
        currentNode = prevNode;
        goto START;
      }  
              

      // .......................................................................    
      // We have reached the lowest level of the lists -- no such item.
      // .......................................................................    
      if (currentLevel == 0) {
        return currentNode;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node associated with a skip list.
////////////////////////////////////////////////////////////////////////////////



void* TRI_StartNodeSkipList(TRI_skiplist_t* skiplist) {
  if (skiplist != NULL) {
    return &(skiplist->_base._startNode);
  }
  return NULL;  
}  




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////





// -----------------------------------------------------------------------------
// --SECTION--                  non-unique skiplist constructors and destructors
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_non_unique
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a multi skip list which allows duplicate entries
////////////////////////////////////////////////////////////////////////////////

void TRI_InitSkipListMulti (TRI_skiplist_multi_t* skiplist,
                            size_t elementSize,
                            int (*compareElementElement) (TRI_skiplist_multi_t*, void*, void*, int),
                            int (*compareKeyElement) (TRI_skiplist_multi_t*, void*, void*, int),
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
/// @brief destroys a multi skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipListMulti (TRI_skiplist_multi_t* skiplist) {
  if (skiplist == NULL) {
    return;
  }  
  TRI_DestroyBaseSkipList( (TRI_skiplist_base_t*)(skiplist) );
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipListMulti (TRI_skiplist_multi_t* skiplist) {
  TRI_DestroySkipListMulti(skiplist);
  TRI_Free(skiplist);
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////




// -----------------------------------------------------------------------------
// --SECTION--                                non-unique skiplist public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_non_unique
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief Returns the end node associated with a skip list.
////////////////////////////////////////////////////////////////////////////////

void* TRI_EndNodeSkipListMulti(TRI_skiplist_multi_t* skiplist) {
  if (skiplist != NULL) {
    return &(skiplist->_base._endNode);
  }
  return NULL;  
}  



////////////////////////////////////////////////////////////////////////////////
/// @brief returns greatest node less than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListMulti(TRI_skiplist_multi_t* skiplist, void* key) {

  int32_t level;
  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* nextNode;
  int compareResult;

  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return NULL;
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
    
    
      // .......................................................................
      // We are at the lowest level of the lists, and we haven't found the item
      // yet. Eventually we would like to return iterators.
      // .......................................................................
      if (currentLevel == 0) {
        return currentNode;
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
      compareResult = skiplist->compareKeyElement(skiplist,key,&(nextNode->_element), -1);   
      
      
      // .......................................................................    
      // We have found the item! Not possible
      // .......................................................................    
      if (compareResult == 0) {
        //return &(nextNode->_element);
        //return currentNode;
        assert(false);
        return nextNode->_column[0]._prev;
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
        return currentNode;
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



////////////////////////////////////////////////////////////////////////////////
/// @brief locate a node using an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementSkipListMulti(TRI_skiplist_multi_t* skiplist, void* element) {  
  assert(false); // there is no way you should be here
} 



////////////////////////////////////////////////////////////////////////////////
/// @brief returns node which matches a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipListMulti(TRI_skiplist_multi_t* skiplist, void* key) {
  assert(false); // there is no way you should be here
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to a multi skip list using an element for searching
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementSkipListMulti(TRI_skiplist_multi_t* skiplist, void* element, bool overwrite) {
  //This uses the compareElementElement callback  
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
  

  // ...........................................................................  
  // Copy the contents of element into the new node to be inserted.
  // If a duplicate has been found, then we destroy the allocated memory.
  // ...........................................................................  
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
  currentLevel = skiplist->_base._startNode._colLength - 1; // NOT current height BUT current level is required here
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
      compareResult = skiplist->compareElementElement(skiplist, element, &(nextNode->_element), -1);   
      
      
      // .......................................................................    
      // The element matches the next element. Overwrite if possible and return.
      // We do not allow non-unique elements.
      // .......................................................................    
      if (compareResult == 0) {
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
    /*
    printf("%s:%u:%u:%u:%u:%u:%u:%u:%u\n",__FILE__,__LINE__,
      (uint64_t)(tempLeftNode),
      (uint64_t)(&(skiplist->_base._startNode)),
      (uint64_t)(newNode->_column[j]._prev),
      (uint64_t)(tempRightNode),
      (uint64_t)(&(skiplist->_base._endNode)),
      (uint64_t)(newNode->_column[j]._next),
      (uint64_t)(newNode)
      );
      */
  }  

  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to a multi skip list 
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeySkipListMulti(TRI_skiplist_multi_t* skiplist, void* key, void* element, bool overwrite) {
  // Use TRI_InsertelementSkipList instead of calling this method
  assert(false);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipListMulti(TRI_skiplist_multi_t* skiplist, void* currentNode) {
  if (skiplist != NULL) {
    return TRI_NextNodeBaseSkipList( (TRI_skiplist_base_t*)(skiplist), currentNode);
  }
  return NULL;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the previous node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipListMulti(TRI_skiplist_multi_t* skiplist, void* currentNode) {
  if (skiplist != NULL) {
    return TRI_PrevNodeBaseSkipList( (TRI_skiplist_base_t*)(skiplist), currentNode);
  }  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes a key/element from a multi skip list
////////////////////////////////////////////////////////////////////////////////



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
      compareResult = skiplist->compareElementElement(skiplist,element,&(nextNode->_element), -1);   
      
      // .......................................................................    
      // We have found an item which matches the key
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
/// @brief removes a key/element from a multi skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeySkipListMulti(TRI_skiplist_multi_t* skiplist, void* key, void* old) {
  // Use the TRI_RemoveElementSkipListMulti method instead.
  assert(false);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns smallest node greater than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipListMulti(TRI_skiplist_multi_t* skiplist, void* key) {

  int32_t level;
  int32_t currentLevel;
  TRI_skiplist_node_t* currentNode;
  TRI_skiplist_node_t* prevNode;
  int compareResult;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return NULL;
  }


  // ...........................................................................  
  // Determine the starting level and the starting node
  // ...........................................................................  
  
  currentLevel = skiplist->_base._startNode._colLength - 1;
  currentNode  = &(skiplist->_base._endNode);

  
  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    prevNode = (TRI_skiplist_node_t*)(currentNode->_column[currentLevel]._prev);



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
    
    if (prevNode == &(skiplist->_base._startNode)) {    
    
      // .......................................................................
      // We are at the lowest level of the lists, and we haven't found the item
      // yet. Eventually we would like to return iterators.
      // .......................................................................
      if (currentLevel == 0) {
        return currentNode;
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
      compareResult = skiplist->compareKeyElement(skiplist,key,&(prevNode->_element), 0);   
      
      
      // .......................................................................    
      // We have found the item!
      // .......................................................................    
      if (compareResult == 0) {
        //return &(nextNode->_element);
        //return currentNode;
        return prevNode->_column[0]._next;
      }
      
      // .......................................................................    
      // The element is greater than the next node element. Keep going on this
      // level.
      // .......................................................................    
      if (compareResult < 0) {
        currentNode = prevNode;
        goto START;
      }  
              

      // .......................................................................    
      // We have reached the lowest level of the lists -- no such item.
      // .......................................................................    
      if (currentLevel == 0) {
        return currentNode;
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



////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node associated with a multi skip list.
////////////////////////////////////////////////////////////////////////////////

void* TRI_StartNodeSkipListMulti(TRI_skiplist_multi_t* skiplist) {
  return &(skiplist->_base._startNode);
}  



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
