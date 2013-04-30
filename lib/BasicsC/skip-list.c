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

#include <BasicsC/logging.h>
#include <BasicsC/random.h>

#include <BasicsC/skip-list.h>

#define SKIP_LIST_ABSOLUTE_MAX_HEIGHT 100

// -----------------------------------------------------------------------------
// --SECTION--                                                          SKIPLIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                          common private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup skip-list
/// @{
////////////////////////////////////////////////////////////////////////////////


// .............................................................................
// Forward declared static functions
// .............................................................................

static int     CreateNewNode        (TRI_skip_list_base_t* skiplist, TRI_skip_list_node_t**); 
static int     FreeSkipListNode     (TRI_skip_list_base_t* skiplist, TRI_skip_list_node_t* node); 
static int     GrowNodeHeight       (TRI_skip_list_node_t* node, uint32_t newHeight);
static int     JoinNodes            (TRI_skip_list_node_t* leftNode, TRI_skip_list_node_t* rightNode, uint32_t startLevel, uint32_t endLevel); 
static void*   NextNodeBaseSkipList (TRI_skip_list_base_t* skiplist, void* currentNode); 
static void*   PrevNodeBaseSkipList (TRI_skip_list_base_t* skiplist, void* currentNode); 
static int32_t RandLevel            (TRI_skip_list_base_t* skiplist); 


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

int TRI_InitSkipList (TRI_skip_list_t* skiplist, uint32_t elementSize,
                      int (*compareElementElement) (TRI_skip_list_t*, void*, void*, int),
                      int (*compareKeyElement) (TRI_skip_list_t*, void*, void*, int),
                      int (*actionElement) (TRI_skip_list_t*, TRI_skip_list_action_e, void*, const void*, void*),
                      TRI_skip_list_prob_e probability, uint32_t maximumHeight) {

  int result;
  
  
  // ..........................................................................  
  // Safety thirst
  // ..........................................................................  
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
    
  // ..........................................................................  
  // Assign the call back functions
  // ..........................................................................  
  
  if (compareElementElement == NULL || compareKeyElement == NULL || actionElement == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  skiplist->_compareElementElement = compareElementElement;
  skiplist->_compareKeyElement     = compareKeyElement;
  skiplist->_actionElement         = actionElement;


  // ..........................................................................  
  // A unique skiplist was created
  // ..........................................................................  
  
  skiplist->_base._unique = true;
  
  // ..........................................................................  
  // Assign the maximum height of the skip list. This maximum height must be
  // no greater than the absolute max height defined as a compile time parameter
  // ..........................................................................  
  
  skiplist->_base._maxHeight = maximumHeight;
  if (maximumHeight > SKIP_LIST_ABSOLUTE_MAX_HEIGHT) {
    LOG_ERROR("Invalid maximum height for skiplist");
    return TRI_ERROR_INTERNAL;
  }  
  
  // ..........................................................................  
  // Assign the probability and determine the number of random numbers which
  // we will require -- do it once off here  
  // ..........................................................................  
  skiplist->_base._prob      = probability;
  skiplist->_base._numRandom = 0;
  switch (skiplist->_base._prob) {
  
    case TRI_SKIP_LIST_PROB_HALF: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 32);
      if ((skiplist->_base._maxHeight % 32) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
  
    case TRI_SKIP_LIST_PROB_THIRD: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    
    case TRI_SKIP_LIST_PROB_QUARTER: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    
    default: {
      LOG_ERROR("Invalid skiplist probability used");
      return TRI_ERROR_INTERNAL;
    }    
  }  // end of switch statement
  
  
  // ..........................................................................  
  // Create storage for where to store the random numbers which we generated
  // do it here once off.
  // ..........................................................................  
  
  skiplist->_base._random = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(uint32_t) * skiplist->_base._numRandom, false);
  if (skiplist->_base._random == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  
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
  
  result = GrowNodeHeight(&(skiplist->_base._startNode), 2); // may fail
  if (result == TRI_ERROR_NO_ERROR) {
    result = GrowNodeHeight(&(skiplist->_base._endNode), 2); // may fail
    if (result == TRI_ERROR_NO_ERROR) {
      // ......................................................................
      // Join the empty lists together
      // [N]<----------------------------------->[N]
      // [N]<----------------------------------->[N]
      // ..........................................................................  
      result = JoinNodes(&(skiplist->_base._startNode), &(skiplist->_base._endNode), 0, 1); // list 0 & 1  
    }  
  }
  
  if (result != TRI_ERROR_NO_ERROR) {
    FreeSkipListNode(&(skiplist->_base), &(skiplist->_base._startNode)); 
    FreeSkipListNode(&(skiplist->_base), &(skiplist->_base._endNode)); 
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplist->_base._random);    
    return result;
  }
  
  return TRI_ERROR_NO_ERROR;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipList (TRI_skip_list_t* skiplist) {
  TRI_skip_list_node_t* nextNode;
  TRI_skip_list_node_t* oldNextNode;
  
  if (skiplist == NULL) {
    return;
  }  

  nextNode = &(skiplist->_base._startNode);
  while (nextNode != NULL) {
    oldNextNode = nextNode->_column[0]._next;
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode->_column);
    if (nextNode->_element != NULL) {
      skiplist->_actionElement(skiplist, TRI_SKIP_LIST_DESTROY_ELEMENT, nextNode->_element, NULL, NULL);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode->_element);
    }  
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode);
    nextNode = oldNextNode;    
  }  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplist->_base._random);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipList (TRI_skip_list_t* skiplist) {
  TRI_DestroySkipList(skiplist);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplist);
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

void* TRI_EndNodeSkipList(TRI_skip_list_t* skiplist) {
  if (skiplist != NULL) {
    return &(skiplist->_base._endNode);
  }
  return NULL;  
}  



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the skip list 
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipList(TRI_skip_list_t* skiplist, void* element, bool overwrite) {
  // Use TRI_InsertKeySkipList instead of calling this method
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipList (TRI_skip_list_t* skiplist, void* key, void* element, bool overwrite) {
  int32_t newHeight;
  int32_t currentLevel;
  uint32_t oldColLength;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* nextNode;
  TRI_skip_list_node_t* newNode;
  TRI_skip_list_node_t* tempLeftNode;
  TRI_skip_list_node_t* tempRightNode;
  int compareResult;
  int result ;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
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
    return TRI_ERROR_INTERNAL;
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
    result = GrowNodeHeight(&(skiplist->_base._startNode), newHeight);
    if (result == TRI_ERROR_NO_ERROR) {
      result = GrowNodeHeight(&(skiplist->_base._endNode), newHeight);
      if (result == TRI_ERROR_NO_ERROR) {
        result = JoinNodes(&(skiplist->_base._startNode),&(skiplist->_base._endNode), oldColLength , newHeight - 1); 
      }  
    }  
    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }
  }
  

  // ...........................................................................  
  // Create the new node to be inserted. If there is some sort of failure,
  // then we delete the node memory.
  // ...........................................................................  
  result = CreateNewNode(&(skiplist->_base), &newNode);
  if (result != TRI_ERROR_NO_ERROR) { // out of memory?
    return result;
  }
  

  // ...........................................................................  
  // Copy the contents of element into the new node to be inserted.
  // If a duplicate has been found, then we destroy the allocated memory.
  // ...........................................................................  
  newNode->_column    = NULL;
  newNode->_colLength = 0;  
  newNode->_extraData = NULL;
  newNode->_element   = NULL;
  
  result = skiplist->_actionElement(skiplist, TRI_SKIP_LIST_CREATE_ELEMENT, newNode->_element, element, NULL);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }
  
  result = GrowNodeHeight(newNode, newHeight);
  if (result != TRI_ERROR_NO_ERROR) {
    FreeSkipListNode(&(skiplist->_base), newNode);
    return result;
  }
  
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
    nextNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._next);


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
      
      compareResult = skiplist->_compareKeyElement(skiplist,key, nextNode->_element, 0);

      
      // .......................................................................    
      // The element matches the next element. Overwrite if possible and return.
      // We do not allow elements with duplicate 'keys'.
      // .......................................................................    
      if (compareResult == 0) {
        FreeSkipListNode(&(skiplist->_base), newNode);
        if (overwrite) {
          result = skiplist->_actionElement(skiplist, TRI_SKIP_LIST_REPLACE_ELEMENT, nextNode->_element, element, NULL);
          return result;
        }
        return TRI_set_errno(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
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
  
  {
    int j; 
    for (j = 0; j < newHeight; ++j) {
      tempLeftNode  = newNode->_column[j]._prev;
      tempRightNode = tempLeftNode->_column[j]._next;
      JoinNodes(tempLeftNode, newNode, j, j);
      JoinNodes(newNode, tempRightNode, j, j);
    }  
  }
  return TRI_ERROR_NO_ERROR;
}




////////////////////////////////////////////////////////////////////////////////
/// @brief returns greatest node less than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipList (TRI_skip_list_t* skiplist, void* key) {
  int32_t currentLevel;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* nextNode;

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
    nextNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._next);



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
      int compareResult;
    
      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->_compareKeyElement(skiplist, key, nextNode->_element, -1);

    
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
    

    
//  END:

  assert(false); // there is no way we can be here  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief locate a node using an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementSkipList (TRI_skip_list_t* skiplist, void* element) {  
  assert(false); // there is no way we can be here
  return NULL;
} 



////////////////////////////////////////////////////////////////////////////////
/// @brief returns node which matches a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipList (TRI_skip_list_t* skiplist, void* key) {
  int32_t currentLevel;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* nextNode;
  
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
    nextNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._next);



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
      int compareResult;

      // .......................................................................
      // Use the callback to determine if the element is less or greater than
      // the next node element.
      // .......................................................................    
      compareResult = skiplist->_compareKeyElement(skiplist, key, nextNode->_element, 0);
      
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
    

    
//  END:

  assert(false); // there is no way we can be here  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipList(TRI_skip_list_t* skiplist, void* currentNode) {
  if (skiplist != NULL) {
    return NextNodeBaseSkipList( (TRI_skip_list_base_t*)(skiplist), currentNode);
  }
  return NULL;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the previous node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipList(TRI_skip_list_t* skiplist, void* currentNode) {
  if (skiplist != NULL) {
    return PrevNodeBaseSkipList( (TRI_skip_list_base_t*)(skiplist), currentNode);
  }  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementSkipList (TRI_skip_list_t* skiplist, void* element, void* old) {
  int32_t currentLevel;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* nextNode;
  TRI_skip_list_node_t* tempLeftNode;
  TRI_skip_list_node_t* tempRightNode;
  int compareResult;
  unsigned int j;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................  
  // Start at the top most list and left most position of that list.
  // ...........................................................................  
  currentLevel = skiplist->_base._startNode._colLength - 1; // we want 'level' not 'height'
  currentNode  = &(skiplist->_base._startNode);

  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    
    nextNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._next);


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
        return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING; 
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
      compareResult = skiplist->_compareElementElement(skiplist, element, nextNode->_element, -1);
      
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
        return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING; 
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
    skiplist->_actionElement(skiplist, TRI_SKIP_LIST_REPLACE_ELEMENT, old, currentNode->_element, NULL);
  }
  
  
  // ..........................................................................
  // Attempt to remove the node which will then remove element as well
  // ..........................................................................
    
  
  for (j = 0; j < currentNode->_colLength; ++j) {
    tempLeftNode  = currentNode->_column[j]._prev;
    tempRightNode = currentNode->_column[j]._next;
    JoinNodes(tempLeftNode, tempRightNode, j, j);
  }  
  
  FreeSkipListNode(&(skiplist->_base), currentNode);
  return TRI_ERROR_NO_ERROR;
 
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipList (TRI_skip_list_t* skiplist, void* key, void* old) {
  // Use the TRI_RemoveElementSkipList method instead.
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns smallest node greater than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipList (TRI_skip_list_t* skiplist, void* key) {
  int32_t currentLevel;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* prevNode;
  
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
    prevNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._prev);



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
      int compareResult = skiplist->_compareKeyElement(skiplist, key, prevNode->_element, 1);
      

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
      // The key is less than the prev node element. Keep going on this
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
    

    
//  END:

  assert(false); // there is no way we can be here  
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node associated with a skip list.
////////////////////////////////////////////////////////////////////////////////



void* TRI_StartNodeSkipList(TRI_skip_list_t* skiplist) {
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

int TRI_InitSkipListMulti (TRI_skip_list_multi_t* skiplist,
                           uint32_t elementSize,
                           int (*compareElementElement) (TRI_skip_list_multi_t*, void*, void*, int),
                           int (*compareKeyElement) (TRI_skip_list_multi_t*, void*, void*, int),
                           bool (*equalElementElement) (TRI_skip_list_multi_t*, void*, void*),
                           int (*actionElement) (TRI_skip_list_multi_t*, TRI_skip_list_action_e, void*, const void*, void*),
                           TRI_skip_list_prob_e probability,
                           uint32_t maximumHeight) {

  int result;
  
  
  // ..........................................................................  
  // Safety thirst
  // ..........................................................................  
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
 
  // ..........................................................................  
  // Assign the call back functions
  // ..........................................................................  
  
  if (compareElementElement == NULL || compareKeyElement == NULL || 
      equalElementElement == NULL   || actionElement == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  
  skiplist->_compareElementElement = compareElementElement; 
  skiplist->_compareKeyElement     = compareKeyElement; 
  skiplist->_equalElementElement   = equalElementElement; 
  skiplist->_actionElement         = actionElement; 
  
  // ..........................................................................  
  // A unique skiplist was created
  // ..........................................................................  
  
  skiplist->_base._unique = false;
  
  // ..........................................................................  
  // Assign the maximum height of the skip list. This maximum height must be
  // no greater than the absolute max height defined as a compile time parameter
  // ..........................................................................  
  
  skiplist->_base._maxHeight = maximumHeight;
  if (maximumHeight > SKIP_LIST_ABSOLUTE_MAX_HEIGHT) {
    LOG_ERROR("Invalid maximum height for skiplist");
    return TRI_ERROR_INTERNAL;
  }  
  
  // ..........................................................................  
  // Assign the probability and determine the number of random numbers which
  // we will require -- do it once off here  
  // ..........................................................................  
  skiplist->_base._prob      = probability;
  skiplist->_base._numRandom = 0;
  switch (skiplist->_base._prob) {
  
    case TRI_SKIP_LIST_PROB_HALF: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 32);
      if ((skiplist->_base._maxHeight % 32) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
  
    case TRI_SKIP_LIST_PROB_THIRD: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    
    case TRI_SKIP_LIST_PROB_QUARTER: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    
    default: {
      LOG_ERROR("Invalid skiplist probability used");
      return TRI_ERROR_INTERNAL;
    }    
  }  // end of switch statement
  
  
  // ..........................................................................  
  // Create storage for where to store the random numbers which we generated
  // do it here once off.
  // ..........................................................................  
  
  skiplist->_base._random = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(uint32_t) * skiplist->_base._numRandom, false);
  if (skiplist->_base._random == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  
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
  
  result = GrowNodeHeight(&(skiplist->_base._startNode), 2); // may fail
  if (result == TRI_ERROR_NO_ERROR) {
    result = GrowNodeHeight(&(skiplist->_base._endNode), 2); // may fail
    if (result == TRI_ERROR_NO_ERROR) {
      // ......................................................................
      // Join the empty lists together
      // [N]<----------------------------------->[N]
      // [N]<----------------------------------->[N]
      // ..........................................................................  
      result = JoinNodes(&(skiplist->_base._startNode), &(skiplist->_base._endNode), 0, 1); // list 0 & 1  
    }  
  }
  
  if (result != TRI_ERROR_NO_ERROR) {
    FreeSkipListNode(&(skiplist->_base), &(skiplist->_base._startNode)); 
    FreeSkipListNode(&(skiplist->_base), &(skiplist->_base._endNode)); 
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplist->_base._random);    
    return result;
  }
  
  return TRI_ERROR_NO_ERROR;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipListMulti (TRI_skip_list_multi_t* skiplist) {
  TRI_skip_list_node_t* nextNode;
  TRI_skip_list_node_t* oldNextNode;
  
  if (skiplist == NULL) {
    return;
  }  

  nextNode = &(skiplist->_base._startNode);
  while (nextNode != NULL) {
    oldNextNode = nextNode->_column[0]._next;
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode->_column);
    if (nextNode->_element != NULL) {
      skiplist->_actionElement(skiplist, TRI_SKIP_LIST_DESTROY_ELEMENT, nextNode->_element, NULL, NULL);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode->_element);
    }  
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode);
    nextNode = oldNextNode;    
  }  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplist->_base._random);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipListMulti (TRI_skip_list_multi_t* skiplist) {
  TRI_DestroySkipListMulti(skiplist);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplist);
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

void* TRI_EndNodeSkipListMulti(TRI_skip_list_multi_t* skiplist) {
  if (skiplist != NULL) {
    return &(skiplist->_base._endNode);
  }
  return NULL;  
}  



////////////////////////////////////////////////////////////////////////////////
/// @brief returns greatest node less than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListMulti(TRI_skip_list_multi_t* skiplist, void* key) {
  int32_t currentLevel;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* nextNode;

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
    nextNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._next);



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
      int compareResult = skiplist->_compareKeyElement(skiplist, key, nextNode->_element, -1);
      
      
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
    

    
//  END:

  assert(false); // there is no way we can be here  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief locate a node using an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementSkipListMulti(TRI_skip_list_multi_t* skiplist, void* element) {  
  assert(false); // there is no way you should be here
  return 0;
} 



////////////////////////////////////////////////////////////////////////////////
/// @brief returns node which matches a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipListMulti(TRI_skip_list_multi_t* skiplist, void* key) {
  assert(false); // there is no way you should be here
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to a multi skip list using an element for searching
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipListMulti(TRI_skip_list_multi_t* skiplist, void* element, bool overwrite) {
  int32_t newHeight;
  int32_t currentLevel;
  uint32_t oldColLength;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* nextNode;
  TRI_skip_list_node_t* newNode;
  TRI_skip_list_node_t* tempLeftNode;
  TRI_skip_list_node_t* tempRightNode;
  int result;
  int compareResult;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
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
    return TRI_ERROR_INTERNAL;
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
    result = GrowNodeHeight(&(skiplist->_base._startNode), newHeight);
    if (result == TRI_ERROR_NO_ERROR) {
      result = GrowNodeHeight(&(skiplist->_base._endNode), newHeight);
      if (result == TRI_ERROR_NO_ERROR) {
        result = JoinNodes(&(skiplist->_base._startNode), &(skiplist->_base._endNode), oldColLength , newHeight - 1); 
      }
    }
    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }      
  }
  

  // ...........................................................................  
  // Create the new node to be inserted. If there is some sort of failure,
  // then we delete the node memory.
  // ...........................................................................  
  result = CreateNewNode(&(skiplist->_base), &newNode);
  if (result != TRI_ERROR_NO_ERROR) { // out of memory?
    return result;
  }

  // ...........................................................................  
  // Copy the contents of element into the new node to be inserted.
  // If a duplicate has been found, then we destroy the allocated memory.
  // ...........................................................................  
  newNode->_column    = NULL;  
  newNode->_colLength = 0;  
  newNode->_extraData = NULL;
  newNode->_element   = NULL;
  
  result = skiplist->_actionElement(skiplist, TRI_SKIP_LIST_CREATE_ELEMENT, newNode->_element, element, NULL);
  
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }
  
  result = GrowNodeHeight(newNode, newHeight);
  if (result != TRI_ERROR_NO_ERROR) {
    FreeSkipListNode(&(skiplist->_base), newNode);
    return result;
  }
  
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
    nextNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._next);


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
      compareResult = skiplist->_compareElementElement(skiplist, element, nextNode->_element, -1);
      
      
      // .......................................................................    
      // The element matches the next element. Overwrite if possible and return.
      // We do not allow non-unique elements (non-unique 'keys' ok).
      // .......................................................................    
      if (compareResult == 0) {
        FreeSkipListNode(&(skiplist->_base), newNode);
        if (overwrite) {
          result  = skiplist->_actionElement(skiplist, TRI_SKIP_LIST_REPLACE_ELEMENT, nextNode->_element, element, NULL);
          return result;
        }
        return TRI_ERROR_ARANGO_INDEX_SKIPLIST_INSERT_ITEM_DUPLICATED;
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
  {
    int j;  
    for (j = 0; j < newHeight; ++j) {
      tempLeftNode  = newNode->_column[j]._prev;
      tempRightNode = tempLeftNode->_column[j]._next;
      JoinNodes(tempLeftNode, newNode, j, j);
      JoinNodes(newNode, tempRightNode, j, j);
    }  
  }
  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to a multi skip list 
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipListMulti(TRI_skip_list_multi_t* skiplist, void* key, void* element, bool overwrite) {
  // Use TRI_InsertelementSkipList instead of calling this method
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipListMulti(TRI_skip_list_multi_t* skiplist, void* currentNode) {
  if (skiplist != NULL) {
    return NextNodeBaseSkipList( (TRI_skip_list_base_t*)(skiplist), currentNode);
  }
  return NULL;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the previous node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipListMulti(TRI_skip_list_multi_t* skiplist, void* currentNode) {
  if (skiplist != NULL) {
    return PrevNodeBaseSkipList( (TRI_skip_list_base_t*)(skiplist), currentNode);
  }  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes a key/element from a multi skip list
////////////////////////////////////////////////////////////////////////////////



int TRI_RemoveElementSkipListMulti (TRI_skip_list_multi_t* skiplist, void* element, void* old) {

  int32_t currentLevel;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* nextNode;
  TRI_skip_list_node_t* tempLeftNode;
  TRI_skip_list_node_t* tempRightNode;
  int compareResult;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
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
    nextNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._next);


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
        return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING;
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
      compareResult = skiplist->_compareElementElement(skiplist, element, nextNode->_element, TRI_SKIP_LIST_COMPARE_SLIGHTLY_LESS);
      
      // .......................................................................    
      // We have found an item which matches the key
      // .......................................................................    
      if (compareResult == TRI_SKIP_LIST_COMPARE_STRICTLY_EQUAL) {
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
      
        // .....................................................................
        // The element could not be located        
        // .....................................................................
        if (compareResult == TRI_SKIP_LIST_COMPARE_STRICTLY_LESS) {
          return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING;
        }
        
        // .....................................................................
        // The element could be located (by matching the key) and we are at the lowest level
        // .....................................................................
        if (compareResult == TRI_SKIP_LIST_COMPARE_SLIGHTLY_LESS) {
          goto END;
        }
        
        // can not occur
        assert(false);
      }
      
      // .......................................................................    
      // Drop down the list
      // .......................................................................    
      --currentLevel;
      
      goto START;
    }  
    

    
  END:
  
  // ..........................................................................
  // locate the correct element -- since we allow duplicates
  // ..........................................................................
  
  while (currentNode != NULL) {
    if (skiplist->_equalElementElement(skiplist, element, currentNode->_element)) {
      break;
    }
    currentNode = NextNodeBaseSkipList(&(skiplist->_base), currentNode); 
  }
  
  
  // ..........................................................................
  // The actual element could not be located - an element with a matching key
  // may exist, but the same data stored within the element could not be located
  // ..........................................................................
  
  if (currentNode == NULL) {
    return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING;
  }

  
  // ..........................................................................
  // Perhaps the user wants a copy before we destory the data?
  // ..........................................................................
  
  if (old != NULL) {
    skiplist->_actionElement(skiplist, TRI_SKIP_LIST_REPLACE_ELEMENT, old, currentNode->_element, NULL);
  }

  
  // ..........................................................................
  // remove element
  // ..........................................................................
  {
    unsigned int j;  
    for (j = 0; j < currentNode->_colLength; ++j) {
      tempLeftNode  = currentNode->_column[j]._prev;
      tempRightNode = currentNode->_column[j]._next;
      JoinNodes(tempLeftNode, tempRightNode, j, j);
    }  
  }
  FreeSkipListNode(&(skiplist->_base), currentNode);
  
  return TRI_ERROR_NO_ERROR;
 
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes a key/element from a multi skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipListMulti(TRI_skip_list_multi_t* skiplist, void* key, void* old) {
  // Use the TRI_RemoveElementSkipListMulti method instead.
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns smallest node greater than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipListMulti(TRI_skip_list_multi_t* skiplist, void* key) {
  int32_t currentLevel;
  TRI_skip_list_node_t* currentNode;
  TRI_skip_list_node_t* prevNode;
  
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
    prevNode = (TRI_skip_list_node_t*)(currentNode->_column[currentLevel]._prev);



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
      int compareResult = skiplist->_compareKeyElement(skiplist, key, prevNode->_element, 1);
      
      
      // .......................................................................    
      // We have found the item! Not possible since we are searching by key!
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
    

    
//  END:

  assert(false); // there is no way we can be here  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node associated with a multi skip list.
////////////////////////////////////////////////////////////////////////////////

void* TRI_StartNodeSkipListMulti(TRI_skip_list_multi_t* skiplist) {
  return &(skiplist->_base._startNode);
}  

















// .............................................................................
// Implementation of forward declared functions
// .............................................................................

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a node, destroying it first
////////////////////////////////////////////////////////////////////////////////

static int CreateNewNode(TRI_skip_list_base_t* skiplist, TRI_skip_list_node_t** newNode) {
  *newNode = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skip_list_node_t), false);
  if (*newNode == NULL) { // out of memory?
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  (*newNode)->_element = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, skiplist->_elementSize, false);
  if ((*newNode)->_element == NULL) { // out of memory?
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, *newNode);
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}
  
static int FreeSkipListNode (TRI_skip_list_base_t* skiplist, TRI_skip_list_node_t* node) {

  if (node == NULL || skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
  }  
  
  if (node->_column != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, node->_column);
  }   
  node->_colLength = 0;
  node->_column    = NULL;
  
  if (  (node != &(skiplist->_startNode)) &&
        (node != &(skiplist->_endNode))  ) {
    
    
    // .........................................................................
    // execute the destroy element callback function which will do whatever
    // needs to be done with the element.
    // .........................................................................    
    
    if (node->_element != NULL) {
      if (skiplist->_unique) {
        ((TRI_skip_list_t*)(skiplist))->_actionElement((TRI_skip_list_t*)(skiplist), TRI_SKIP_LIST_DESTROY_ELEMENT, node->_element, NULL, NULL);
      }
      else {
        ((TRI_skip_list_multi_t*)(skiplist))->_actionElement((TRI_skip_list_multi_t*)(skiplist), TRI_SKIP_LIST_DESTROY_ELEMENT, node->_element, NULL, NULL);
      }      
      node->_element = NULL;
    }
    
    // .........................................................................    
    // Only free the memory if the node is neither the Start nor the End node
    // .........................................................................    

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, node);
  }  
  
  return TRI_ERROR_NO_ERROR;  
}




////////////////////////////////////////////////////////////////////////////////
/// @brief Grow the node at the height specified.
////////////////////////////////////////////////////////////////////////////////

static int GrowNodeHeight (TRI_skip_list_node_t* node, uint32_t newHeight) {
                           
  TRI_skip_list_nb_t* newColumn = NULL;
  uint32_t j;


  if (node->_colLength >= newHeight) {
    return TRI_ERROR_NO_ERROR;
  }

  
  // ...........................................................................
  // Create new node column (tower)
  // ...........................................................................
  
  newColumn = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skip_list_nb_t) * newHeight, false);
  if (newColumn == NULL) { // out of memory?
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  
  // ...........................................................................
  // Copy the contents of the column (tower) from the existing column into
  // the new column. Note that the old column may have a tower of height 0!
  // ...........................................................................
  
  if (node->_column != NULL) { 
    // I assume that the height of the tower must be greater than zero.
    memcpy(newColumn, node->_column, node->_colLength * sizeof(TRI_skip_list_nb_t) );
  }
  
  // ...........................................................................
  // Initialise the next/prev pointers of the linked lists to NULL
  // ...........................................................................
  
  for (j = node->_colLength; j < newHeight; ++j) {
    newColumn[j]._prev  = NULL; 
    newColumn[j]._next = NULL; 
  }
  
  // ...........................................................................
  // Assign the new tower to the existing node and remove any memory which
  // has been allocated to the old tower
  // ...........................................................................
  node->_colLength = newHeight;
  if (node->_column != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, node->_column);
  }  
  node->_column = newColumn;
  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief joins a left node and right node together
////////////////////////////////////////////////////////////////////////////////

static int JoinNodes(TRI_skip_list_node_t* leftNode, TRI_skip_list_node_t* rightNode, uint32_t startLevel, uint32_t endLevel) {
  uint32_t j;
  
  if (startLevel > endLevel) { // something wrong
    assert(false);
    return TRI_ERROR_INTERNAL;
  }  

  // change level to heigth   
  endLevel += 1;
    
  if (leftNode->_colLength < endLevel) {
    assert(false);
    return TRI_ERROR_INTERNAL;
  }

  if (rightNode->_colLength < endLevel) {
    assert(false);
    return TRI_ERROR_INTERNAL;
  }
  
  for (j = startLevel; j < endLevel; ++j) {
    (leftNode->_column)[j]._next = rightNode;  
    (rightNode->_column)[j]._prev = leftNode;  
  }  
  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void* NextNodeBaseSkipList(TRI_skip_list_base_t* skiplist, void* currentNode) {
  TRI_skip_list_node_t* node;
  
  if (currentNode == NULL) {
    return &(skiplist->_startNode);
  }  
  if (currentNode == &(skiplist->_endNode)) {
    return NULL;
  }  
  node = (TRI_skip_list_node_t*)(currentNode);
  return(node->_column[0]._next);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void* PrevNodeBaseSkipList(TRI_skip_list_base_t* skiplist, void* currentNode) {
  TRI_skip_list_node_t* node;
  
  if (currentNode == NULL) {
    return &(skiplist->_endNode);
  }  
  if (currentNode == &(skiplist->_startNode)) {
    return NULL;
  }  
  node = (TRI_skip_list_node_t*)(currentNode);
  return(node->_column[0]._prev);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief determines at what 'height' the item is to be added
////////////////////////////////////////////////////////////////////////////////

static int32_t RandLevel (TRI_skip_list_base_t* skiplist) {

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
  
    case TRI_SKIP_LIST_PROB_HALF: {
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
    
    case TRI_SKIP_LIST_PROB_THIRD: {
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
    
    case TRI_SKIP_LIST_PROB_QUARTER: {
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
