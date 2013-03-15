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

#include "skiplistEx.h"
#include "compareEx.h"

#define SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT 100

// -----------------------------------------------------------------------------
// --SECTION--                                                       SKIPLIST_EX
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// --SECTION--                                       STATIC FORWARD DECLARATIONS
// --SECTION--                                          common private functions
// -----------------------------------------------------------------------------

static void    DestroyBaseSkipListEx  (TRI_skiplistEx_base_t*);
static void    DestroySkipListExNode  (TRI_skiplistEx_base_t*, TRI_skiplistEx_node_t*);
static void    FreeSkipListExNode     (TRI_skiplistEx_base_t*, TRI_skiplistEx_node_t*);
static bool    GrowNodeHeight         (TRI_skiplistEx_node_t*, uint32_t);
static void*   NextNodeBaseSkipListEx (TRI_skiplistEx_base_t*, void*, uint64_t);
static void*   PrevNodeBaseSkipListEx (TRI_skiplistEx_base_t*, void*, uint64_t);
static int32_t RandLevel              (TRI_skiplistEx_base_t*);


// ..............................................................................
// These operations have to be handled very carefully for transactions which are
// supposedly lock free.
// ..............................................................................

// ..............................................................................
// DestroyNodeCAS: Attempts to physically (as opposed to logically) remove the
//                 node from the skip list. In the first iteration of a lock
//                 free skip list can we call this directly without Garbage
//                 Collection -- knowning that no other writers are operating
//                 at the same time? No other insertions/deletions are happening.
//
// InsertNodeCAS: inserts a node into the skip list - with transaction and 
//                lock-free support.
//
// RemoveNodeCAS: marks nodes as ghosts to be destroyed as some future time.
// ..............................................................................

static void DestroyNodeCAS (TRI_skiplistEx_node_t*, uint64_t);
static void InsertNodeCAS  (TRI_skiplistEx_node_t*, uint64_t);
static void JoinNodesCAS   (TRI_skiplistEx_node_t*, TRI_skiplistEx_node_t*, uint32_t, uint32_t, uint64_t);
static void RemoveNodeCAS  (TRI_skiplistEx_node_t*, uint64_t);

// ..............................................................................
// DestroyNodeNoCAS: simply removes the node from its nearest neighbours and
//                    joins these nearest neighbours together without being
//                    directly concerned about transactions. NOT USED
//
// InsertNodeNoCAS: simply joins two nodes and is not concerned directly with
//                 transactions. It will however store the transaction number
//                 of the writer in the node.
//
// RemoveNodeNoCAS: simply marks the node as a ghost node which should be removed
//                  at some future time. It will store the transaction number
//                  of the writer in the node. NOT USED
// ..............................................................................

static void DestroyNodeNoCAS (TRI_skiplistEx_node_t*, uint64_t);
static void InsertNodeNoCAS  (TRI_skiplistEx_node_t*, uint64_t);
static void JoinNodesNoCAS   (TRI_skiplistEx_node_t*, TRI_skiplistEx_node_t*, uint32_t, uint32_t, uint64_t);
static void RemoveNodesNoCAS (TRI_skiplistEx_node_t*, uint64_t);



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

int TRI_InitSkipListEx (TRI_skiplistEx_t* skiplist, size_t elementSize,
                         int (*compareElementElement) (TRI_skiplistEx_t*, void*, void*, int),
                         int (*compareKeyElement) (TRI_skiplistEx_t*, void*, void*, int),
                         TRI_skiplistEx_prob_e probability, 
                         uint32_t maximumHeight, 
                         uint64_t lastKnownTransID) {

  bool growResult;
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  // ..........................................................................  
  // Assign the STATIC comparision call back functions
  // ..........................................................................  
  
  skiplist->compareElementElement = IndexStaticCompareElementElement; // compareElementElement;
  skiplist->compareKeyElement     = IndexStaticCompareKeyElement;     // compareKeyElement;

  // ..........................................................................  
  // Assign the maximum height of the skip list. This maximum height must be
  // no greater than the absolute max height defined as a compile time parameter
  // ..........................................................................  
  skiplist->_base._maxHeight = maximumHeight;
  if (maximumHeight > SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT) {
    LOG_ERROR("Invalid maximum height for skiplist");
    assert(false);
  }  
  
  // ..........................................................................  
  // Assign the probability and determine the number of random numbers which
  // we will require -- do it once off here  
  // ..........................................................................  
  skiplist->_base._prob      = probability;
  skiplist->_base._numRandom = 0;
  switch (skiplist->_base._prob) {
    case TRI_SKIPLIST_EX_PROB_HALF: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 32);
      if ((skiplist->_base._maxHeight % 32) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    case TRI_SKIPLIST_EX_PROB_THIRD: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    case TRI_SKIPLIST_EX_PROB_QUARTER: {
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
  skiplist->_base._random = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(uint32_t) * skiplist->_base._numRandom, false);
  // TODO: memory allocation might fail
  
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
  skiplist->_base._startNode._delTransID      = UINT64_MAX;
  skiplist->_base._startNode._insTransID      = lastKnownTransID;

  
  skiplist->_base._endNode._column            = NULL;
  skiplist->_base._endNode._colLength         = 0; 
  skiplist->_base._endNode._extraData         = NULL;
  skiplist->_base._endNode._element           = NULL;
  skiplist->_base._endNode._delTransID        = UINT64_MAX;
  skiplist->_base._endNode._insTransID        = lastKnownTransID;
  
  // ..........................................................................  
  // Whenever a probability of 1/2, 1/3, 1/4 is used, on average there will be
  // each node will have a height of two. So initialise the start and end nodes
  // with this 'average' height
  // ..........................................................................  
  growResult = GrowNodeHeight(&(skiplist->_base._startNode), 2); // may fail
  growResult = growResult && GrowNodeHeight(&(skiplist->_base._endNode), 2); // may fail
  if (! growResult) {
    // TODO: undo growth by cutting down the node height
    return TRI_ERROR_INTERNAL;
  }

  // ..........................................................................  
  // Join the empty lists together
  // no locking requirements for joining nodes since the skip list index is not known 
  // to anyone yet!
  // [N]<----------------------------------->[N]
  // [N]<----------------------------------->[N]
  // ..........................................................................    
  JoinNodesNoCAS(&(skiplist->_base._startNode), &(skiplist->_base._endNode), 0, 1, lastKnownTransID); // list 0 & 1    
  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipListEx(TRI_skiplistEx_t* skiplist) {
  if (skiplist != NULL) {
    DestroyBaseSkipListEx( (TRI_skiplistEx_base_t*)(skiplist) );
  }  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipListEx(TRI_skiplistEx_t* skiplist) {
  TRI_DestroySkipListEx(skiplist);
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

// .............................................................................
// Observe that this is some sort of read transaction. The only possibilitiy
// we have is that the index must have been created AFTER this read transaction
// occurred (given that the skip list is valid of course). We do not check
// for this. (Note: the START (HEAD) and END (TAIL) nodes never change once
// the skip list is created.)
// .............................................................................

void* TRI_EndNodeSkipListEx(TRI_skiplistEx_t* skiplist) {
  if (skiplist != NULL) {  
    return &(skiplist->_base._endNode);
  }
  return NULL;  
}  



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the skip list 
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipListEx(TRI_skiplistEx_t* skiplist, void* element, bool overwrite, uint64_t thisTransID) {
  // Use TRI_InsertKeySkipList instead of calling this method
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipListEx (TRI_skiplistEx_t* skiplist, void* key, void* element, bool overwrite, uint64_t thisTransID) {
  //This uses the compareKeyElement callback  
  int32_t newHeight;
  int32_t currentLevel;
  uint32_t oldColLength;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  TRI_skiplistEx_node_t* newNode;
  TRI_skiplistEx_node_t* tempLeftNode;
  TRI_skiplistEx_node_t* tempRightNode;
  int compareResult;
  bool growResult;
  int j;
  
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
    growResult = GrowNodeHeight(&(skiplist->_base._startNode), newHeight);
    growResult = growResult && GrowNodeHeight(&(skiplist->_base._endNode), newHeight);
    if (!growResult) {
      // todo: undo growth by cutting down the node height
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    JoinNodesCAS(&(skiplist->_base._startNode),&(skiplist->_base._endNode), oldColLength , newHeight - 1, thisTransID); 
  }
  

  // ...........................................................................  
  // Create the new node to be inserted. If there is some sort of failure,
  // then we delete the node memory.
  // ...........................................................................  
  newNode = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_node_t) + skiplist->_base._elementSize, false);
  if (newNode == NULL) { // out of memory?
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  

  // ...........................................................................  
  // Copy the contents of element into the new node to be inserted.
  // If a duplicate has been found, then we destroy the allocated memory.
  // ...........................................................................  
  newNode->_column    = NULL;
  newNode->_colLength = 0;  
  newNode->_extraData = NULL;
  
  j = IndexStaticCopyElementElement(&(skiplist->_base), &(newNode->_element), element);
  if (j != TRI_ERROR_NO_ERROR) {
      return j;
  }
    
  growResult = GrowNodeHeight(newNode, newHeight);
  if (!growResult) {
    FreeSkipListExNode(&(skiplist->_base), newNode);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  
  // ...........................................................................  
  // Assign the deletion transaction id and the insertion transaction id
  // ...........................................................................  
  
  newNode->_delTransID = UINT64_MAX; // since we are inserting this new node it can not be deleted
  newNode->_insTransID = thisTransID; // this is what was given to us

  
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
    nextNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._next);


    // .........................................................................  
    // Since this skiplist apparently supports transactions we have a few 
    // things to consider. ** NOTE ** we are here INSERTING a new node in a 
    // UNIQUE skiplist.
    // 
    // We attempt to locate the 'position' of where the node should be inserted
    // by imagining we have a KEY-> key + epsilon. Now with the ASSUMPTION that
    // while this WRITER is writing (here) NO NEW writers and readers can be started
    // we check the previous node
    // .........................................................................  
    
    
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
      compareResult = IndexStaticCompareKeyElement(skiplist,key,&(nextNode->_element), 0);

      
      // .......................................................................    
      // The element matches the next element. 
      // However since we support transactions some things are different and we
      // we have to traead carefully.
      // .......................................................................    
      
      if (compareResult == 0) {
      
        // .....................................................................
        // It may happen that this node is NOT deleted and simply there -         
        // check the ins & del transaction numbers.
        // .....................................................................
        
        if (nextNode->_insTransID <= thisTransID) {
          // ...................................................................            
          // node has been previously inserted 
          // ...................................................................            

          if (nextNode->_delTransID > thisTransID) { 
          // ...................................................................            
          // Node has NOT been deleted (e.g. imagine it will be deleted some time in the future).
          // Treat this as a duplicate key, overwrite if possible and return. 
          // We do not allow elements with duplicate 'keys'.
          // ...................................................................            
            FreeSkipListExNode(&(skiplist->_base), newNode);

            if (overwrite) {
              j = IndexStaticCopyElementElement(&(skiplist->_base), &(nextNode->_element), element);
              return j;
            }
            return TRI_set_errno(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
          }   
          
          // ...................................................................
          // The only case left here is that the node has been deleted by either
          // this transaction (which could happen in an UPDATE) or by some
          // previous write transaction. Treat this case as if the element is 
          // greater than the next node element. Keep going on this level.
          // ...................................................................
          currentNode = nextNode;
          goto START;
          
        }
        
        
        if (nextNode->_insTransID > thisTransID) { 
          // ...................................................................            
          // Something terrible has happened since writers have been serialized,
          // how is that an existing node has a higher transaction number than
          // this transaction
          // ...................................................................            
          printf("%s:%s:%d:Can not be here!\n",__FILE__,__FUNCTION__,__LINE__);
          assert(false); // there is no way we can be here  
        }          
        
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
  // SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT number of elements.
  // ..........................................................................
  

  // ..........................................................................
  // this is the tricky part II since we have to attempt to do this as 
  // 'lock-free' as possible
  // ..........................................................................
  
  // this should be called JoinTower!
  for (j = 0; j < newHeight; ++j) {
    tempLeftNode  = newNode->_column[j]._prev;
    tempRightNode = tempLeftNode->_column[j]._next;
    JoinNodesCAS(tempLeftNode, newNode, j, j, thisTransID);
    JoinNodesCAS(newNode, tempRightNode, j, j, thisTransID);
  }  

  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns greatest node less than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListEx(TRI_skiplistEx_t* skiplist, void* key, uint64_t thisTransID) {
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;

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
    nextNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._next);



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
      compareResult = IndexStaticCompareKeyElement(skiplist,key,&(nextNode->_element), -1);

    
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

void* TRI_LookupByElementSkipListEx (TRI_skiplistEx_t* skiplist, void* element, uint64_t thisTransID) {  
  assert(false); // there is no way we can be here
  return NULL;
} 



////////////////////////////////////////////////////////////////////////////////
/// @brief returns node which matches a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipListEx (TRI_skiplistEx_t* skiplist, void* key, uint64_t thisTransID) {
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  
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
    nextNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._next);



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
      compareResult = IndexStaticCompareKeyElement(skiplist,key,&(nextNode->_element), 0);
      
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

void* TRI_NextNodeSkipListEx(TRI_skiplistEx_t* skiplist, void* currentNode, uint64_t thisTransID) {
  if (skiplist != NULL) {
    return NextNodeBaseSkipListEx( (TRI_skiplistEx_base_t*)(skiplist), currentNode, thisTransID);
  }
  return NULL;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the previous node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipListEx(TRI_skiplistEx_t* skiplist, void* currentNode, uint64_t thisTransID) {
  if (skiplist != NULL) {
    return PrevNodeBaseSkipListEx( (TRI_skiplistEx_base_t*)(skiplist), currentNode, thisTransID);
  }  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementSkipListEx (TRI_skiplistEx_t* skiplist, void* element, void* old, uint64_t thisTransID) {
  // This uses the compareElementElement callback
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  TRI_skiplistEx_node_t* tempLeftNode;
  TRI_skiplistEx_node_t* tempRightNode;
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
    
    nextNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._next);


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
      compareResult = IndexStaticCompareElementElement(skiplist,element,&(nextNode->_element), -1);
      
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
    IndexStaticCopyElementElement(&(skiplist->_base), old, &(currentNode->_element));
  }
  
  
  // ..........................................................................
  // Attempt to remove the node which will then remove element as well
  // ..........................................................................
    
  
  for (j = 0; j < currentNode->_colLength; ++j) {
    tempLeftNode  = currentNode->_column[j]._prev;
    tempRightNode = currentNode->_column[j]._next;
    JoinNodesCAS(tempLeftNode, tempRightNode, j, j, thisTransID);
  }  
  
  FreeSkipListExNode(&(skiplist->_base), currentNode);
  return TRI_ERROR_NO_ERROR;
 
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipListEx(TRI_skiplistEx_t* skiplist, void* key, void* old, uint64_t thisTransID) {
  // Use the TRI_RemoveElementSkipList method instead.
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns smallest node greater than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipListEx(TRI_skiplistEx_t* skiplist, void* key, uint64_t thisTransID) {
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* prevNode;
  
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
    prevNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._prev);



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
      int compareResult = IndexStaticCompareKeyElement(skiplist, key, &(prevNode->_element), 1);
      

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
      // The key is greater than the next node element. Keep going on this
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



void* TRI_StartNodeSkipListEx(TRI_skiplistEx_t* skiplist) {
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

int TRI_InitSkipListExMulti (TRI_skiplistEx_multi_t* skiplist,
                              size_t elementSize,
                              int (*compareElementElement) (TRI_skiplistEx_multi_t*, void*, void*, int),
                              int (*compareKeyElement) (TRI_skiplistEx_multi_t*, void*, void*, int),
                              bool (*equalElementElement) (TRI_skiplistEx_multi_t*, void*, void*),
                              TRI_skiplistEx_prob_e probability,
                              uint32_t maximumHeight,
                              uint64_t lastKnownTransID) {

  bool growResult;
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  // ..........................................................................  
  // Assign the comparision call back functions
  // ..........................................................................  
  
  skiplist->compareElementElement = IndexStaticMultiCompareElementElement; //compareElementElement;
  skiplist->compareKeyElement     = IndexStaticMultiCompareKeyElement; // compareKeyElement;
  skiplist->equalElementElement   = IndexStaticMultiEqualElementElement; //equalElementElement;
  
  // ..........................................................................  
  // Assign the maximum height of the skip list. This maximum height must be
  // no greater than the absolute max height defined as a compile time parameter
  // ..........................................................................  
  skiplist->_base._maxHeight = maximumHeight;
  if (maximumHeight > SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT) {
    LOG_ERROR("Invalid maximum height for skiplist");
    assert(false);
  }  
  
  // ..........................................................................  
  // Assign the probability and determine the number of random numbers which
  // we will require -- do it once off here  
  // ..........................................................................  
  skiplist->_base._prob      = probability;
  skiplist->_base._numRandom = 0;
  switch (skiplist->_base._prob) {
    case TRI_SKIPLIST_EX_PROB_HALF: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 32);
      if ((skiplist->_base._maxHeight % 32) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    case TRI_SKIPLIST_EX_PROB_THIRD: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    case TRI_SKIPLIST_EX_PROB_QUARTER: {
      // determine the number of random numbers which we require.
      skiplist->_base._numRandom = (skiplist->_base._maxHeight / 16);
      if ((skiplist->_base._maxHeight % 16) != 0) {
        ++(skiplist->_base._numRandom);
      }      
      break;
    }
    default: {
      assert(false);
      // TODO: log error
      break;
    }    
  }  // end of switch statement
  
  // ..........................................................................  
  // Create storage for where to store the random numbers which we generated
  // do it here once off.
  // ..........................................................................  
  skiplist->_base._random = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(uint32_t) * skiplist->_base._numRandom, false);
  /* TODO: memory allocation might fail */
  
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
  growResult = GrowNodeHeight(&(skiplist->_base._startNode), 2);
  growResult = growResult && GrowNodeHeight(&(skiplist->_base._endNode), 2);
  if (!growResult) {
    // todo: truncate the nodes and return
    return TRI_ERROR_INTERNAL;
  }
  
  // ..........................................................................  
  // Join the empty lists together
  // [N]<----------------------------------->[N]
  // [N]<----------------------------------->[N]
  // ..........................................................................  
  JoinNodesNoCAS(&(skiplist->_base._startNode),&(skiplist->_base._endNode),0,1, lastKnownTransID); // list 0 & 1  
  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipListExMulti (TRI_skiplistEx_multi_t* skiplist) {
  if (skiplist == NULL) {
    return;
  }  
  DestroyBaseSkipListEx( (TRI_skiplistEx_base_t*)(skiplist) );
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipListExMulti (TRI_skiplistEx_multi_t* skiplist) {
  TRI_DestroySkipListExMulti(skiplist);
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

void* TRI_EndNodeSkipListExMulti(TRI_skiplistEx_multi_t* skiplist) {
  if (skiplist != NULL) {
    return &(skiplist->_base._endNode);
  }
  return NULL;  
}  



////////////////////////////////////////////////////////////////////////////////
/// @brief returns greatest node less than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* key, uint64_t thistransID) {
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;

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
    nextNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._next);



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
      int compareResult = IndexStaticMultiCompareKeyElement(skiplist,key,&(nextNode->_element), -1);
      
      
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

void* TRI_LookupByElementSkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* element, uint64_t thisTransID) {  
  assert(false); // there is no way you should be here
  return 0;
} 



////////////////////////////////////////////////////////////////////////////////
/// @brief returns node which matches a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* key, uint64_t thisTransID) {
  assert(false); // there is no way you should be here
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to a multi skip list using an element for searching
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* element, bool overwrite, uint64_t thisTransID) {
  //This uses the compareElementElement callback  
  int32_t newHeight;
  int32_t currentLevel;
  uint32_t oldColLength;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  TRI_skiplistEx_node_t* newNode;
  TRI_skiplistEx_node_t* tempLeftNode;
  TRI_skiplistEx_node_t* tempRightNode;
  bool growResult;
  int compareResult;
  int j;
  
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
    growResult = GrowNodeHeight(&(skiplist->_base._startNode), newHeight);
    growResult = growResult && GrowNodeHeight(&(skiplist->_base._endNode), newHeight);
    if (!growResult) {
      // todo: truncate the nodes and return;
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    JoinNodesCAS(&(skiplist->_base._startNode),&(skiplist->_base._endNode), oldColLength , newHeight - 1, thisTransID); 
  }
  

  // ...........................................................................  
  // Create the new node to be inserted. If there is some sort of failure,
  // then we delete the node memory.
  // ...........................................................................  
  newNode = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_node_t) + skiplist->_base._elementSize, false);
  if (newNode == NULL) { // out of memory?
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  

  // ...........................................................................  
  // Copy the contents of element into the new node to be inserted.
  // If a duplicate has been found, then we destroy the allocated memory.
  // ...........................................................................  
  newNode->_column    = NULL;  
  newNode->_colLength = 0;  
  newNode->_extraData = NULL;
  
  j = IndexStaticCopyElementElement(&(skiplist->_base), &(newNode->_element),element);
  
  if (j != TRI_ERROR_NO_ERROR) {
    return j;
  }
  
  growResult = GrowNodeHeight(newNode, newHeight);
  if (!growResult) {
    FreeSkipListExNode(&(skiplist->_base), newNode);
    return TRI_ERROR_OUT_OF_MEMORY;
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
    nextNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._next);


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
      compareResult = IndexStaticMultiCompareElementElement(skiplist,element,&(nextNode->_element), -1);
      
      
      // .......................................................................    
      // The element matches the next element. Overwrite if possible and return.
      // We do not allow non-unique elements (non-unique 'keys' ok).
      // .......................................................................    
      if (compareResult == 0) {
        FreeSkipListExNode(&(skiplist->_base), newNode);
        if (overwrite) {
          j = IndexStaticCopyElementElement(&(skiplist->_base), &(nextNode->_element),element);
          return j;
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
  
  for (j = 0; j < newHeight; ++j) {
    tempLeftNode  = newNode->_column[j]._prev;
    tempRightNode = tempLeftNode->_column[j]._next;
    JoinNodesCAS(tempLeftNode, newNode, j, j, thisTransID);
    JoinNodesCAS(newNode, tempRightNode, j, j, thisTransID);
  }  

  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to a multi skip list 
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* key, void* element, bool overwrite, uint64_t thisTransID) {
  // Use TRI_InsertelementSkipListEx instead of calling this method
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* currentNode, uint64_t thisTransID) {
  if (skiplist != NULL) {
    return NextNodeBaseSkipListEx( (TRI_skiplistEx_base_t*)(skiplist), currentNode, thisTransID);
  }
  return NULL;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the previous node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* currentNode, uint64_t thisTransID) {
  if (skiplist != NULL) {
    return PrevNodeBaseSkipListEx( (TRI_skiplistEx_base_t*)(skiplist), currentNode, thisTransID);
  }  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes a key/element from a multi skip list
////////////////////////////////////////////////////////////////////////////////



int TRI_RemoveElementSkipListExMulti (TRI_skiplistEx_multi_t* skiplist, void* element, void* old, uint64_t thisTransID) {

  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  TRI_skiplistEx_node_t* tempLeftNode;
  TRI_skiplistEx_node_t* tempRightNode;
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
  currentLevel = skiplist->_base._startNode._colLength - 1; // current level not height
  currentNode  = &(skiplist->_base._startNode);

  START:

  
    // .........................................................................
    // Find the next node in the current level of the lists. 
    // .........................................................................  
    nextNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._next);


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
      compareResult = IndexStaticMultiCompareElementElement(skiplist,element,&(nextNode->_element), TRI_SKIPLIST_EX_COMPARE_SLIGHTLY_LESS);
      
      // .......................................................................    
      // We have found an item which matches the key
      // .......................................................................    
      if (compareResult == TRI_SKIPLIST_EX_COMPARE_STRICTLY_EQUAL) {
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
        if (compareResult == TRI_SKIPLIST_EX_COMPARE_STRICTLY_LESS) {
          return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING;
        }
        
        // .....................................................................
        // The element could be located (by matching the key) and we are at the lowest level
        // .....................................................................
        if (compareResult == TRI_SKIPLIST_EX_COMPARE_SLIGHTLY_LESS) {
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
  // locate the correct elemet -- since we allow duplicates
  // ..........................................................................
  
  while (currentNode != NULL) {
    if (IndexStaticMultiEqualElementElement(skiplist, element, &(currentNode->_element))) {
      break;
    }
    currentNode = NextNodeBaseSkipListEx(&(skiplist->_base), currentNode, thisTransID); 
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
    IndexStaticCopyElementElement(&(skiplist->_base), old, &(currentNode->_element));
  }

  
  // ..........................................................................
  // remove element
  // ..........................................................................
  
  for (j = 0; j < currentNode->_colLength; ++j) {
    tempLeftNode  = currentNode->_column[j]._prev;
    tempRightNode = currentNode->_column[j]._next;
    JoinNodesCAS(tempLeftNode, tempRightNode, j, j, thisTransID);
  }  

  FreeSkipListExNode(&(skiplist->_base), currentNode);
  
  return TRI_ERROR_NO_ERROR;
 
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes a key/element from a multi skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* key, void* old, uint64_t thisTransID) {
  // Use the TRI_RemoveElementSkipListExMulti method instead.
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns smallest node greater than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* key, uint64_t thisTransID) {
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* prevNode;
  
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
    prevNode = (TRI_skiplistEx_node_t*)(currentNode->_column[currentLevel]._prev);



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
      int compareResult = IndexStaticMultiCompareKeyElement(skiplist,key,&(prevNode->_element), 1);
      
      
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

void* TRI_StartNodeSkipListExMulti(TRI_skiplistEx_multi_t* skiplist) {
  return &(skiplist->_base._startNode);
}  


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF STATIC FORWARD DECLARED FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SkiplistEx_common
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

static void DestroyBaseSkipListEx(TRI_skiplistEx_base_t* baseSkiplist) {
  TRI_skiplistEx_node_t* nextNode;
  TRI_skiplistEx_node_t* oldNextNode;

  if (baseSkiplist == NULL) {
    return;
  }  
  
  nextNode = &(baseSkiplist->_startNode);
  while (nextNode != NULL) {
    oldNextNode = nextNode->_column[0]._next;
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode->_column);
    if ((nextNode != &(baseSkiplist->_startNode)) && (nextNode != &(baseSkiplist->_endNode))) {
      IndexStaticDestroyElement(baseSkiplist, &(nextNode->_element));
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode);
    }
    nextNode = oldNextNode;    
  }  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, baseSkiplist->_random);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the internal structure allocation for a node
////////////////////////////////////////////////////////////////////////////////

static void DestroySkipListExNode (TRI_skiplistEx_base_t* skiplist, TRI_skiplistEx_node_t* node) {
  if (node == NULL) {
    return;
  }  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, node->_column);
  IndexStaticDestroyElement(skiplist, &(node->_element));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief frees a node, destroying it first
////////////////////////////////////////////////////////////////////////////////

static void FreeSkipListExNode (TRI_skiplistEx_base_t* skiplist, TRI_skiplistEx_node_t* node) {
  DestroySkipListExNode(skiplist, node);
  if ( (node == &(skiplist->_startNode)) ||
     (node == &(skiplist->_endNode)) ) {
    return;   
  }  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, node);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Grow the node at the height specified.
////////////////////////////////////////////////////////////////////////////////

static bool GrowNodeHeight (TRI_skiplistEx_node_t* node, uint32_t newHeight) {                           
  TRI_skiplistEx_nb_t* oldColumn = node->_column;
  uint32_t j;

  if (node->_colLength >= newHeight) {
    return true;
  }
  
  node->_column = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_nb_t) * newHeight, false);
  
  if (node->_column == NULL) {
    // out of memory?
    return false;
  }
  
  if (oldColumn != NULL) {
    memcpy(node->_column, oldColumn, node->_colLength * sizeof(TRI_skiplistEx_nb_t) );
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, oldColumn);
  }
  
  // ...........................................................................
  // Initialise the storage
  // ...........................................................................
  
  for (j = node->_colLength; j < newHeight; ++j) {
    (node->_column)[j]._prev = NULL; 
    (node->_column)[j]._next = NULL; 
  }
  
  node->_colLength = newHeight;
  
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys node
////////////////////////////////////////////////////////////////////////////////

static void DestroyNodeCAS (TRI_skiplistEx_node_t* theNode, 
                             uint64_t thisTransID) {
  assert(false);                             
}                             

static void InsertNodeCAS (TRI_skiplistEx_node_t* theNode, 
                           uint64_t thisTransID) {
  assert(false);                             
}

static void JoinNodesCAS(TRI_skiplistEx_node_t* leftNode, 
                           TRI_skiplistEx_node_t* rightNode, 
                           uint32_t startLevel, 
                           uint32_t endLevel, 
                           uint64_t thisTransID) {
  assert(false);                           
}

static void RemoveNodeCAS (TRI_skiplistEx_node_t* theNode, 
                           uint64_t thisTransID) {
  assert(false);                             
}                             

static void DestroyNodeNoCAS (TRI_skiplistEx_node_t* theNode, 
                              uint64_t thisTransID) {
  assert(false);                             
}                             

static void InsertNodeNoCAS (TRI_skiplistEx_node_t* theNode, 
                             uint64_t thisTransID) {
  assert(false);                             
}


////////////////////////////////////////////////////////////////////////////////
/// @brief joins a left node and right node together
////////////////////////////////////////////////////////////////////////////////



static void JoinNodesNoCAS(TRI_skiplistEx_node_t* leftNode, 
                           TRI_skiplistEx_node_t* rightNode, 
                           uint32_t startLevel, 
                           uint32_t endLevel, 
                           uint64_t thisTransID) {
  uint32_t j;
  
  if (startLevel > endLevel) { // something wrong
    assert(false);
    return;
  }  

  // change level to height
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


static void RemoveNodeNoCAS (TRI_skiplistEx_node_t* theNode, 
                             uint64_t thisTransID) {
  assert(false);                             
}                             


////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void* NextNodeBaseSkipListEx(TRI_skiplistEx_base_t* skiplist, void* currentNode, uint64_t thisTransID) {
  TRI_skiplistEx_node_t* node;
  
  if (currentNode == NULL) {
    return &(skiplist->_startNode);
  }  
  if (currentNode == &(skiplist->_endNode)) {
    return NULL;
  }  
  node = (TRI_skiplistEx_node_t*)(currentNode);
  return(node->_column[0]._next);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void* PrevNodeBaseSkipListEx(TRI_skiplistEx_base_t* skiplist, void* currentNode, uint64_t thisTransID) {
  TRI_skiplistEx_node_t* node;
  
  if (currentNode == NULL) {
    return &(skiplist->_endNode);
  }  
  if (currentNode == &(skiplist->_startNode)) {
    return NULL;
  }  
  node = (TRI_skiplistEx_node_t*)(currentNode);
  return(node->_column[0]._prev);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief determines at what 'height' the item is to be added
////////////////////////////////////////////////////////////////////////////////

static int32_t RandLevel (TRI_skiplistEx_base_t* skiplist) {

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
  
    case TRI_SKIPLIST_EX_PROB_HALF: {
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
    
    case TRI_SKIPLIST_EX_PROB_THIRD: {
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
    
    case TRI_SKIPLIST_EX_PROB_QUARTER: {
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
