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
/// @author Anonymous
/// @author Copyright 2006-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/locks.h>
#include <BasicsC/logging.h>
#include <BasicsC/random.h>

#include "skiplistEx.h"
#include "compareEx.h"

#ifdef _WIN32 
  #include <BasicsC/win-utils.h>
#endif
  

// -----------------------------------------------------------------------------
// --SECTION--                                                       SKIPLIST_EX
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// --SECTION--                                           Private Type Structures
// -----------------------------------------------------------------------------
typedef enum {   
  TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG,   // the nearest neighbour node is normal            
  TRI_SKIPLIST_EX_BRICKED_NEAREST_NEIGHBOUR_FLAG,  // the nearest neighbour node is bricked - next/prev pointers can not be modified           

  TRI_SKIPLIST_EX_NORMAL_TOWER_NODE_FLAG,          // normal tower node, no removal pending
  TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG,           // glass tower node, skipped in a lookup, removal pending

  TRI_SKIPLIST_EX_FREE_TO_GROW_START_END_NODES_FLAG,     // start/end nodes special this and flag below ensures that
  TRI_SKIPLIST_EX_NOT_FREE_TO_GROW_START_END_NODES_FLAG  // the tower height of these nodes is performed sequentially  
} 
TRI_skiplistEx_tower_node_flag_e;  

static unsigned int CAS_FAILURE_SLEEP_TIME = 1000;
static unsigned int SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT = 100;
static unsigned int SKIPLIST_EX_CAS_FAILURES_MAX_LOOP = 10;

// -----------------------------------------------------------------------------
// --SECTION--                                       STATIC FORWARD DECLARATIONS
// --SECTION--                                          common private functions
// -----------------------------------------------------------------------------

static void    DestroyBaseSkipListEx  (TRI_skiplistEx_base_t*);
static void    DestroySkipListExNode  (TRI_skiplistEx_base_t*, TRI_skiplistEx_node_t*);
static void    FreeSkipListExNode     (TRI_skiplistEx_base_t*, TRI_skiplistEx_node_t*);
static int     GrowNewNodeHeight      (TRI_skiplistEx_node_t*, uint32_t, uint32_t, int);
static int     GrowStartEndNodes      (TRI_skiplistEx_base_t*, uint32_t);

static void*   NextNodeBaseSkipListEx (TRI_skiplistEx_base_t*, void*, uint64_t);
static void*   PrevNodeBaseSkipListEx (TRI_skiplistEx_base_t*, void*, uint64_t);
static int32_t RandLevel              (TRI_skiplistEx_base_t*);

static void JoinStartEndNodes (TRI_skiplistEx_node_t*, TRI_skiplistEx_node_t*, uint32_t, uint32_t);
static int  JoinNewNodeCas    (TRI_skiplistEx_node_t* newNode); // when node is inserted
static int  UnJoinOldNodeCas  (TRI_skiplistEx_node_t* oldNode); // when node is removed

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

// .............................................................................
// TODO: The static integer variables CAS_FAILURE_SLEEP_TIME(1000), 
// SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT(100) and SKIPLIST_EX_CAS_FAILURES_MAX_LOOP(10)
// should be adjusted upon startup of the server -- command line perhaps?
// .............................................................................

int TRI_InitSkipListEx (TRI_skiplistEx_t* skiplist, size_t elementSize,
                         int (*compareElementElement) (TRI_skiplistEx_t*, void*, void*, int),
                         int (*compareKeyElement) (TRI_skiplistEx_t*, void*, void*, int),
                         TRI_skiplistEx_prob_e probability, 
                         uint32_t maximumHeight, 
                         uint64_t lastKnownTransID) {

  int result;
  
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
  if (maximumHeight == 0) {
    maximumHeight = SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT;
  }  
  skiplist->_base._maxHeight = maximumHeight;
  if (maximumHeight > SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT) {
    LOG_ERROR("Invalid maximum height for skiplist");
    assert(false);
    return TRI_ERROR_INTERNAL;
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
      LOG_ERROR("Invalid probability assigned to skiplist");
      assert(false);
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
  skiplist->_base._startNode._delTransID      = UINT64_MAX;
  skiplist->_base._startNode._insTransID      = lastKnownTransID;

  
  skiplist->_base._endNode._column            = NULL;
  skiplist->_base._endNode._colLength         = 0; 
  skiplist->_base._endNode._extraData         = NULL;
  skiplist->_base._endNode._element           = NULL;
  skiplist->_base._endNode._delTransID        = UINT64_MAX;
  skiplist->_base._endNode._insTransID        = lastKnownTransID;

  
  // ...........................................................................
  // 32 bit integer CAS flag
  // ...........................................................................
  skiplist->_base._growStartEndNodesFlag = TRI_SKIPLIST_EX_FREE_TO_GROW_START_END_NODES_FLAG;
  
  // ..........................................................................  
  // Whenever a probability of 1/2, 1/3, 1/4 is used, on average there will be
  // each node will have a height of two. So initialise the start and end nodes
  // with this 'average' height
  // ..........................................................................  
  result = GrowNewNodeHeight(&(skiplist->_base._startNode), skiplist->_base._maxHeight, 2,TRI_ERROR_NO_ERROR); // may fail
  result = GrowNewNodeHeight(&(skiplist->_base._endNode), skiplist->_base._maxHeight, 2, result); // may fail
  
  if (result != TRI_ERROR_NO_ERROR) { 
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)(skiplist->_base._random));
    
    if (skiplist->_base._startNode._column != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)(skiplist->_base._startNode._column));
    }  
    
    if (skiplist->_base._endNode._column != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)(skiplist->_base._endNode._column));
    }  
    
    return result;
  }

  
  // ..........................................................................  
  // Join the empty lists together
  // no locking requirements for joining nodes since the skip list index is not known 
  // to anyone yet!
  // [N]<----------------------------------->[N]
  // [N]<----------------------------------->[N]
  // ..........................................................................    
  JoinStartEndNodes(&(skiplist->_base._startNode), &(skiplist->_base._endNode), 0, skiplist->_base._maxHeight - 1); // joins list 0 & 1    
  
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
  if (skiplist != NULL) {
    TRI_DestroySkipListEx(skiplist);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplist);
  }  
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
// for this internal error. 
// Also note that the ADDRESS of the START (HEAD) and END (TAIL) nodes never 
// change once the skip list is created. (These addresses are static.)
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
  LOG_TRACE("Insertions into a skip list require a key. Elements/items are not currently supported.");
  assert(false);
  return TRI_ERROR_INTERNAL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief inserts (adds) an element to the skip list using a key
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipListEx (TRI_skiplistEx_t* skiplist, // the skiplist we are using
                             void* key,                  // the key used to locate the position of the item within the list 
                             void* element,              // the data stored within the skiplist node 
                             bool overwrite,             // if true, then if the key already exists, the element will be replaced by this one
                             uint64_t thisTransID) {     // the transaction id of the writer which has requested the insertion
  int32_t newHeight;
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  TRI_skiplistEx_node_t* newNode;
  int compareResult;
  int result;
  int casFailures = -1;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  
  // ...........................................................................  
  // Determine the number of levels in which to add the item. That is, determine
  // the height of the node so that it participates in that many lists. 
  // Convert the level to a height
  // ...........................................................................  
  
  newHeight = (RandLevel(&(skiplist->_base))) + 1;

  
  // ...........................................................................  
  // Something wrong since the newHeight must be at least 1
  // ...........................................................................  
  
  if (newHeight < 1) {
    return TRI_ERROR_INTERNAL;
  }  
    

  // ...........................................................................  
  // Grow lists if required by increasing the height of the start and end nodes
  // ...........................................................................  
  
  result = GrowStartEndNodes(&(skiplist->_base), newHeight);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }
  
    
  // ...........................................................................  
  // Create the new node to be inserted. If there is some sort of failure,
  // then we delete the node memory.
  // ...........................................................................  
  
  newNode = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_node_t) + skiplist->_base._elementSize, false);
  if (newNode == NULL) { // out of memory?
    // no necessity to undo the start/end node growth
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  

  // ...........................................................................  
  // Copy the contents of element into the new node to be inserted.
  // If a duplicate has been found, then we destroy the allocated memory.
  // ...........................................................................  

  newNode->_column    = NULL;
  newNode->_colLength = 0;  
  newNode->_extraData = NULL;
  
  result = IndexStaticCopyElementElement(&(skiplist->_base), &(newNode->_element), element);
  result = GrowNewNodeHeight(newNode, newHeight, newHeight, result);
  if (result != TRI_ERROR_NO_ERROR) {
    FreeSkipListExNode(&(skiplist->_base), newNode);
    return result;
  }
    
  
  // ...........................................................................  
  // Assign the deletion transaction id and the insertion transaction id
  // ...........................................................................  
  
  newNode->_delTransID = UINT64_MAX; // since we are inserting this new node it can not be deleted
  newNode->_insTransID = thisTransID; // this is what was given to us

  
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      FreeSkipListExNode(&(skiplist->_base), newNode);
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, lock? The sleep time should be something 
    // needs to be adjusted.
    // ...........................................................................  
    
    if (casFailures > -1) {
      usleep(CAS_FAILURE_SLEEP_TIME);       
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the path where the new item is to be inserted. If the item 
    // already exists either replace it or return false. Recall that this 
    // skip list is used for unique key/value pairs. Use the skiplist-multi 
    // non-unique key/value pairs.
    // ...........................................................................  

    currentLevel = skiplist->_base._startNode._colLength - 1; // NOT current height BUT current level is required here
    currentNode  = &(skiplist->_base._startNode);
    nextNode     = currentNode;
  

    START: {

    
      // .........................................................................
      // The current node (which we have called the nextNode below) should never
      // be null. Protect yourself in case something has gone wrong.
      // .........................................................................  
      
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      nextNode = (TRI_skiplistEx_node_t*)(nextNode->_column[currentLevel]._next);
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // An insert/lookup/removal SEARCH like this, can ONLY ever find 1 glass
      // node when we are very unlucky. (The GC makes the node glass and then
      // goes and unlinks the pointers.) If we skip the glass node, then we 
      // will have the wrong pointers to compare, so we have to CAS_RESTART
      // .........................................................................
      
      if (nextNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto CAS_RESTART;
      }
      
    
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
          newNode->_column[currentLevel]._next = nextNode;                    
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
        nextNode = currentNode;
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
        // The element to be inserted has a key which greater than the next node's 
        // element key. Keep going on this level.
        // .......................................................................    

        if (compareResult > 0) {
          currentNode = nextNode;
          goto START;
        }  
        
        
        // .......................................................................    
        // The element matches the next element. 
        // However since we support transactions some things are different and we
        // we have to tread carefully. Note that any nodes with the same key are
        // ALWAYS inserted to the LEFT of the existing node. This means we need
        // only check the next node.
        // .......................................................................    
        
        if (compareResult == 0) {
        
          // .....................................................................
          // It may happen that this node is NOT deleted and simply there -         
          // check the ins & del transaction numbers.
          // .....................................................................

          
          if (nextNode->_insTransID > thisTransID) { 
          
            // ...................................................................            
            // Something terrible has happened since writers have been serialized,
            // how is that an existing node has a higher transaction number than
            // this transaction
            // ...................................................................            
            
            printf("%s:%s:%d:Can not be here!\n",__FILE__,__FUNCTION__,__LINE__);
            assert(false); // there is no way we can be here  
          }          
          
          
          // .....................................................................            
          // node has been previously inserted 
          // .....................................................................            

            
          if (nextNode->_delTransID > thisTransID) { 
          
            // ...................................................................
            // Node has NOT been deleted (e.g. imagine it will be deleted some 
            // time in the future). Treat this as a duplicate key, overwrite if 
            // possible and return. We do not allow elements with duplicate 'keys'.
            // ...................................................................            
            
            FreeSkipListExNode(&(skiplist->_base), newNode);

            if (overwrite) {
              result = IndexStaticCopyElementElement(&(skiplist->_base), &(nextNode->_element), element);
              return result;
            }
            return TRI_set_errno(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
          }   
            
          // .....................................................................
          // The only case left here is that the node has been deleted by either
          // this transaction (which could happen in an UPDATE) or by some
          // previous write transaction. Treat this case as if the element is 
          // less than the next node element - this ensure that that the
          // most recent revision of the data is always to the LEFT. 
          // Keep going on this level.
          // .....................................................................
        }
                        

        // .......................................................................    
        // The element is less than the next node. Can we drop down the list?
        // Store the current node and level in the path.
        // .......................................................................
        if (currentLevel < newHeight) {
          newNode->_column[currentLevel]._prev = currentNode;
          newNode->_column[currentLevel]._next = nextNode;
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
        nextNode = currentNode;
        --currentLevel;
        
        goto START;
      }  

    } // end of label START
    
  } // end of label CAS_RESTART
    
    
  END: {

    // ..........................................................................
    // Ok finished with the loop and we should have a path with AT MOST
    // SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT number of elements.
    // ..........................................................................
    

    // ..........................................................................
    // this is the tricky part since we have to attempt to do this as 
    // 'lock-free' as possible. This is acheived in three passes:
    // Pass 1: Mark each prev and next node of the new node so that the GC
    //         can not modify it. If this fails goto CAS_RESTART
    // Pass 2: Ensure that each prev and next tower is not glassed.
    // Pass 3: Modify the newnode.prev.next to newnode and newnode.next.prev = newnode
    // ..........................................................................
    
    result = JoinNewNodeCas(newNode);
    if (result == TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE) {
      goto CAS_RESTART;
    }
    return result;
    
  } // end of END label
    
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns greatest node less than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListEx(TRI_skiplistEx_t* skiplist, void* key, uint64_t thisTransID) {
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  int casFailures = -1;
  
  // ...........................................................................  
  // Just in case ...
  // ...........................................................................  
  
  if (skiplist == NULL) {
    LOG_ERROR("Internal Error");
    return NULL;
  }

  
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      return NULL;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, block?
    // ...........................................................................  
    
    if (casFailures > -1) {
      assert(0); // a test to see why it blocks - should not block!
      usleep(CAS_FAILURE_SLEEP_TIME);       
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the starting level and the starting node
    // ...........................................................................  
    
    currentLevel = skiplist->_base._startNode._colLength - 1;
    currentNode  = &(skiplist->_base._startNode);
    nextNode     = currentNode;
                 
  
    START: {

    
      // .........................................................................
      // Find the next node in the current level of the lists. Protect yourself
      // in case something has gone wrong.
      // .........................................................................  
      
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      
      
      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      nextNode = (TRI_skiplistEx_node_t*)(nextNode->_column[currentLevel]._next);
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // Is our next node a glass node? If so we must skip it!
      // Note: since Garbage Collection is performed in TWO passes, it is possible
      // that we have more than one glass node.
      // .........................................................................
      
      if (nextNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto START;
      }
            
      
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
        // yet. The currentNode does NOT compare and the next node is +\infinty.
        // .......................................................................
        
        if (currentLevel == 0) {
          return currentNode;
        }
        
        
        // .......................................................................
        // We have not yet reached the lowest level continue down. Possibly our
        // item we seek is to be found a lower level.
        // .......................................................................

        nextNode = currentNode;        
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
        // the next node element. We treat the comparison by assuming we are 
        // looking for a "key - epsilon". With this assumption we always find the
        // last key to our right if it exists. The reason this is necessary is as 
        // follows: we allow a multiple documents with the same key to be stored
        // here with the proviso that all but the last one is marked as deleted.
        // This is how we cater for multiple revisions.
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
        
        if (compareResult > 0) {
          currentNode = nextNode;
          goto START;
        }  


        // .......................................................................    
        // The element is less than the next node. Can we drop down the list?
        // If have reached the lowest level of the lists -- no such item.
        // .......................................................................    
        
        if (currentLevel == 0) {
          return currentNode;
        }
        
        
        // .......................................................................    
        // Drop down the list
        // .......................................................................    
        
        --currentLevel;
        nextNode = currentNode;
        goto START;
      }  
    
    } // end of label START

  } // end of label CAS_RESTART
    
  assert(false);  
  return NULL;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns node which matches a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipListEx (TRI_skiplistEx_t* skiplist, void* key, uint64_t thisTransID) {
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  int casFailures = -1;
  
  // ...........................................................................  
  // Just in case ...
  // ...........................................................................  
  
  if (skiplist == NULL) {
    LOG_ERROR("Internal Error");
    return NULL;
  }

  
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      return NULL;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, block?
    // ...........................................................................  
    
    if (casFailures > -1) {
      usleep(CAS_FAILURE_SLEEP_TIME);       
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the starting level and the starting node
    // ...........................................................................  
    
    currentLevel = skiplist->_base._startNode._colLength - 1;
    currentNode  = &(skiplist->_base._startNode);
    nextNode     = currentNode;
                 
  
    START: {

    
      // .........................................................................
      // Find the next node in the current level of the lists. Protect yourself
      // in case something has gone wrong.
      // .........................................................................  
      
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      
      
      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      nextNode = (TRI_skiplistEx_node_t*)(nextNode->_column[currentLevel]._next);
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // Is our next node a glass node? If so we must skip it!
      // Note: since Garbage Collection is performed in TWO passes, it is possible
      // that we have more than one glass node.
      // .........................................................................
      
      if (nextNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto START;
      }
            
      
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
        // yet. The currentNode does NOT compare and the next node is +\infinty.
        // .......................................................................
        
        if (currentLevel == 0) {
          return NULL;
        }
        
        
        // .......................................................................
        // We have not yet reached the lowest level continue down. Possibly our
        // item we seek is to be found a lower level.
        // .......................................................................

        nextNode = currentNode;        
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
        // the next node element. We treat the comparison by assuming we are 
        // looking for a "key - epsilon". With this assumption we always find the
        // last key to our right if it exists. The reason this is necessary is as 
        // follows: we allow a multiple documents with the same key to be stored
        // here with the proviso that all but the last one is marked as deleted.
        // This is how we cater for multiple revisions.
        // .......................................................................    
        
        compareResult = IndexStaticCompareKeyElement(skiplist,key,&(nextNode->_element), 0);
        
        // .......................................................................    
        // The element is greater than the next node element. Keep going on this
        // level.
        // .......................................................................    
        
        if (compareResult > 0) {
          currentNode = nextNode;
          goto START;
        }  

        
        
        if (compareResult == 0) {
        
          // .....................................................................
          // It may happen that this node is NOT deleted and simply there -         
          // check the ins & del transaction numbers.
          // .....................................................................

          
          if (nextNode->_insTransID > thisTransID) { 
          
            // ...................................................................            
            // This node has been inserted AFTER the reading starting reading!
            // Treat this as if the node was NEVER there.
            // ...................................................................            
            
            //return NULL;            
          }          
          
          
          // .....................................................................            
          // node has been previously inserted 
          // .....................................................................            

            
          if (nextNode->_delTransID > thisTransID) { 
          
            // ...................................................................
            // Node has NOT been deleted (e.g. imagine it will be deleted some 
            // time in the future). This is the node we want, even though it may 
            // be deleted very very soon.
            // ...................................................................            
            
            return nextNode;
          }   
            
          // .....................................................................
          // The only case left here is that the node has been deleted by either
          // this transaction (which could happen in an UPDATE) or by some
          // previous write transaction. Treat this case as if the element is 
          // less than the next node element - this ensures that that the
          // most recent revision of the data is always to the LEFT. 
          // Keep going on this level.
          // .....................................................................
        }
        
                
        // .......................................................................    
        // The element is less than the next node. Can we drop down the list?
        // If have reached the lowest level of the lists -- no such item.
        // .......................................................................    
        
        if (currentLevel == 0) {
          return NULL;
        }
        
        // .......................................................................    
        // Drop down the list
        // .......................................................................    
        --currentLevel;
        nextNode = currentNode;
        goto START;
      }  
    
    } // end of label START

  } // end of label CAS_RESTART
    
  assert(0);
  
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

int TRI_RemoveElementSkipListEx (TRI_skiplistEx_t* skiplist, void* element, void* old, 
                                 const int passLevel,  const uint64_t thisTransID,
                                 TRI_skiplistEx_node_t** passNode) {
  // ...........................................................................
  // To remove an element from this skip list we have three pass levels:
  // Pass 1: locate (if possible) the exact NODE - must match exactly.
  //         Once located, add the transaction id to the node. Return.
  // Pass 2: locate the node (if not possible report error) - must match exactly.
  //         Once located, attempt to unlink all the pointers and make the
  //         node a Glass Node.
  // Pass 3: Excise the node by destroying it's allocated memory. 
  // ...........................................................................
  
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode = NULL;
  TRI_skiplistEx_node_t* nextNode = NULL;
  int casFailures = -1;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    LOG_ERROR("Internal Error");
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................  
  // Only for pass level 1 do we have a requirement to locate the actual node
  // using the key. For pass levels 2 & 3 we have the pointer to the node. 
  // ...........................................................................  
  
  if (passLevel != 1) { goto END; }
    
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_CAS_FAILURE;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, block?
    // ...........................................................................  
    
    if (casFailures > -1) {
      usleep(CAS_FAILURE_SLEEP_TIME);            
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the starting level and the starting node
    // ...........................................................................  
    
    currentLevel = skiplist->_base._startNode._colLength - 1;
    currentNode  = &(skiplist->_base._startNode);
    nextNode     = currentNode;
                 
  
    START: {

      // .........................................................................
      // Find the next node in the current level of the lists. Protect yourself
      // in case something has gone wrong.
      // .........................................................................  
      
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      
      
      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      nextNode = (TRI_skiplistEx_node_t*)(nextNode->_column[currentLevel]._next);
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // Is our next node a glass node? If so we must skip it. Recall we are in 
      // Phase I here -- meaning that we are searching for a node which has not
      // be removed and previously inserted. 
      // .........................................................................

      if (nextNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto START;
      }
      

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
        
        nextNode = currentNode;
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
        
        compareResult = IndexStaticCompareElementElement(skiplist,element,&(nextNode->_element), -1);

        
        // .......................................................................    
        // The element is greater than the next node element. Keep going on this
        // level.
        // .......................................................................    
        if (compareResult > 0) {
          currentNode = nextNode;
          goto START;
        }  
                
        
        if (compareResult == 0) { // a node matches the key exactly
       
          if (nextNode->_insTransID > thisTransID) { 
          
            // ...................................................................            
            // This node has been inserted AFTER the reader starting reading!
            // An insertion can only have occured if (a) there never was a previous
            // node with the same key or (b) there exists another with the same
            // key but of course now must be marked as deleted.
            // ...................................................................            
            
            return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_POST_INSERTED;
          }          
        

          // .....................................................................            
          // node has been previously inserted 
          // .....................................................................            

            
          if (nextNode->_delTransID > thisTransID) { 
          
            // ...................................................................
            // Node has NOT been deleted (e.g. imagine it will be deleted some 
            // time in the future). This is the node we want.
            // ...................................................................            
            
            currentNode = nextNode;
            goto END;          
          }   
 
          // .....................................................................
          // The only case left here is that the node has been deleted by either
          // this transaction (which could happen in an UPDATE) or by some
          // previous write transaction. Treat this case as if the element is 
          // less than the next node element - this ensure that that the
          // most recent revision of the data is always to the LEFT. 
          // Keep going on this level.
          // .....................................................................
  
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
        nextNode = currentNode;        
        goto START;
      }  
      
    } // end of START label
  } // end of CAS_RESTART label
  
  END: {
  
    switch (passLevel) {
    
      // .........................................................................
      // In this case we simply add the del transaction id with a CAS statement.
      // It should never fail!
      // .........................................................................

      case 1: {
        bool ok;
        
        if (currentNode == NULL) { // something terribly wrong
          assert(0);
          return TRI_ERROR_INTERNAL;
        }

        ok = TRI_CompareAndSwapIntegerUInt64 (&(currentNode->_delTransID), 
                                              UINT64_MAX, thisTransID);
        if (!ok) {
          assert(0);
          return TRI_ERROR_INTERNAL;
        }
        // ....................................................................
        // If requested copy the contents of the element we have located into the
        // storage sent.
        // ....................................................................
  
        if (old != NULL) {
          IndexStaticCopyElementElement(&(skiplist->_base), old, &(currentNode->_element));
        }
        
        *passNode = currentNode;
        
        return TRI_ERROR_NO_ERROR;        
      }
      
      
      // .........................................................................
      // In this case we wish to make the node a glass node and to unjoin all
      // other connected nodes.
      // .........................................................................
      case 2: {
      
        // .......................................................................
        // We can not now rely upon looking up the node using the key, since 
        // we would need to traverse right and attempt to match either then 
        // transaction id and/or the pointer to the doc. Easier to simply
        // send the address of the node back.
        // .......................................................................
        if (*passNode == NULL) {
          return TRI_ERROR_INTERNAL;
        }  
        currentNode  = (TRI_skiplistEx_node_t*)(*passNode);
        
        
        // .......................................................................
        // Only the Garbage Collector can transform a node into a glass node, and
        // since the GC is only operating in one thread safe to do a simple
        // comparison here.
        // .......................................................................
        if (currentNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
          return TRI_ERROR_INTERNAL;
        }  

        // .......................................................................
        // safety check         
        // .......................................................................
        if (currentNode->_delTransID != thisTransID) {
          return TRI_ERROR_INTERNAL;
        }
        
        // .......................................................................
        // The stragey is this:
        // (a) Brick each nearest neighbour on this node. This ensures that NO
        // other nodes can be attached to this node.
        // (b) Mark this node as being glass. This ensures that it is skipped
        // since it is no longer required in the index.
        // (c) Unbrick each of its nearest neighbours on this node. This ensures
        // that an inserted node MAY be allowed to be attached but will later fail.
        // Also allows us to brick other glass nodes.
        // (d) Brick each prev and next nearest neighbour of this node. Irrespective
        // if one of these are glass or not. This ensures that lookups can 
        // proceed unhinded.
        // (e) Unjoin the node from the list.
        // (f) Unbrick each prev/next nearest neigbour
        // .......................................................................
        
        return UnJoinOldNodeCas(currentNode);
      }
      
      
      // .........................................................................
      // In this case since no other reader/writer can be accessing the node,
      // we simply destroy it. we require the node to be glass.
      // .........................................................................
      case 3: {
        // .......................................................................
        // We can not now rely upon looking up the node using the key, since 
        // we would need to traverse right and attempt to match either then 
        // transaction id and/or the pointer to the doc. Easier to simply
        // send the address of the node back.
        // .......................................................................
        if (*passNode == NULL) {
          return TRI_ERROR_INTERNAL;
        }  
        currentNode  = (TRI_skiplistEx_node_t*)(*passNode);
        
        // .......................................................................
        // Only the Garbage Collector can transform a node into a glass node, and
        // since the GC is only operating in one thread safe to do a simple
        // comparison here.
        // .......................................................................
        if (currentNode->_towerFlag != TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
          return TRI_ERROR_INTERNAL;
        }  

        // .......................................................................
        // safety check         
        // .......................................................................
        if (currentNode->_delTransID != thisTransID) {
          return TRI_ERROR_INTERNAL;
        }
  
        FreeSkipListExNode(&(skiplist->_base), currentNode);
        
        break;
        
      }
      
      default: {
        assert(0);
        return TRI_ERROR_INTERNAL;
      }
      
    } // end of switch statement
    
  } // end of END label
  
  return TRI_ERROR_NO_ERROR;
 
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipListEx(TRI_skiplistEx_t* skiplist, void* key, void* old, 
                            const int passLevel, const uint64_t thisTransID,
                            TRI_skiplistEx_node_t** passNode) {
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
  int casFailures = -1;
  
  // ...........................................................................  
  // Just in case ...
  // ...........................................................................  
  
  if (skiplist == NULL) {
    LOG_ERROR("Internal Error");
    return NULL;
  }

  
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      return NULL;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, block?
    // ...........................................................................  
    
    if (casFailures > -1) {
      assert(0); // a test to see why it blocks - should not block!
      usleep(CAS_FAILURE_SLEEP_TIME);       
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the starting level and the starting node
    // ...........................................................................  
    
    currentLevel = skiplist->_base._endNode._colLength - 1;
    currentNode  = &(skiplist->_base._endNode);
    prevNode     = currentNode;
                 
  
    START: {

    
      // .........................................................................
      // Find the next node in the current level of the lists. Protect yourself
      // in case something has gone wrong.
      // .........................................................................  
      
      if (prevNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      
      
      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      prevNode = (TRI_skiplistEx_node_t*)(prevNode->_column[currentLevel]._prev);
      if (prevNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // Is our next node a glass node? If so we must skip it!
      // Note: since Garbage Collection is performed in TWO passes, it is possible
      // that we have more than one glass node.
      // .........................................................................
      
      if (prevNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto START;
      }
            
      
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
        // yet. The currentNode does NOT compare and the next node is +\infinty.
        // .......................................................................
        
        if (currentLevel == 0) {
          return currentNode;
        }
        
        
        // .......................................................................
        // We have not yet reached the lowest level continue down. Possibly our
        // item we seek is to be found a lower level.
        // .......................................................................

        prevNode = currentNode;        
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
        // the next node element. We treat the comparison by assuming we are 
        // looking for a "key - epsilon". With this assumption we always find the
        // last key to our right if it exists. The reason this is necessary is as 
        // follows: we allow a multiple documents with the same key to be stored
        // here with the proviso that all but the last one is marked as deleted.
        // This is how we cater for multiple revisions.
        // .......................................................................    
        
        compareResult = IndexStaticCompareKeyElement(skiplist,key,&(prevNode->_element), 1);
        
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
        
        if (compareResult < 0) {
          currentNode = prevNode;
          goto START;
        }  


        // .......................................................................    
        // The element is less than the next node. Can we drop down the list?
        // If have reached the lowest level of the lists -- no such item.
        // .......................................................................    
        
        if (currentLevel == 0) {
          return currentNode;
        }
        
        
        // .......................................................................    
        // Drop down the list
        // .......................................................................    
        
        --currentLevel;
        prevNode = currentNode;
        goto START;
      }  
    
    } // end of label START

  } // end of label CAS_RESTART
    
  assert(false);  
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

int TRI_InitSkipListExMulti (TRI_skiplistEx_multi_t* skiplist, size_t elementSize,
                             int (*compareElementElement) (TRI_skiplistEx_multi_t*, void*, void*, int),
                             int (*compareKeyElement) (TRI_skiplistEx_multi_t*, void*, void*, int),
                             bool (*equalElementElement) (TRI_skiplistEx_multi_t*, void*, void*),
                             TRI_skiplistEx_prob_e probability,
                             uint32_t maximumHeight,
                             uint64_t lastKnownTransID) {

  int result;
  
  if (skiplist == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  // ..........................................................................  
  // Assign the STATIC comparision call back functions
  // ..........................................................................  
  
  skiplist->compareElementElement = IndexStaticMultiCompareElementElement; //compareElementElement;
  skiplist->compareKeyElement     = IndexStaticMultiCompareKeyElement; // compareKeyElement;
  skiplist->equalElementElement   = IndexStaticMultiEqualElementElement; //equalElementElement;
  
  // ..........................................................................  
  // Assign the maximum height of the skip list. This maximum height must be
  // no greater than the absolute max height defined as a compile time parameter
  // ..........................................................................  
  if (maximumHeight == 0) {
    maximumHeight = SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT;
  }  
  skiplist->_base._maxHeight = maximumHeight;
  if (maximumHeight > SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT) {
    LOG_ERROR("Invalid maximum height for skiplist");
    assert(false);
    return TRI_ERROR_INTERNAL;
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
      LOG_ERROR("Invalid probability assigned to skiplist");
      assert(false);
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
  skiplist->_base._startNode._delTransID      = UINT64_MAX;
  skiplist->_base._startNode._insTransID      = lastKnownTransID;
  
  skiplist->_base._endNode._column            = NULL;
  skiplist->_base._endNode._colLength         = 0; 
  skiplist->_base._endNode._extraData         = NULL;
  skiplist->_base._endNode._element           = NULL;
  skiplist->_base._endNode._delTransID        = UINT64_MAX;
  skiplist->_base._endNode._insTransID        = lastKnownTransID;
  
  
  // ...........................................................................
  // 32 bit integer CAS flag
  // ...........................................................................
  skiplist->_base._growStartEndNodesFlag = TRI_SKIPLIST_EX_FREE_TO_GROW_START_END_NODES_FLAG;

  
  // ..........................................................................  
  // Whenever a probability of 1/2, 1/3, 1/4 is used, on average 
  // each node will have a height of two. So initialise the start and end nodes
  // with this 'average' height
  // ..........................................................................  
  result = GrowNewNodeHeight(&(skiplist->_base._startNode), skiplist->_base._maxHeight, 2, TRI_ERROR_NO_ERROR); // may fail
  result = GrowNewNodeHeight(&(skiplist->_base._endNode), skiplist->_base._maxHeight, 2, result); // may fail
  
  if (result != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)(skiplist->_base._random));
    
    if (skiplist->_base._startNode._column != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)(skiplist->_base._startNode._column));
    }  
    
    if (skiplist->_base._endNode._column != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)(skiplist->_base._endNode._column));
    }  
    
    return result;
  }
  
  // ..........................................................................  
  // Join the empty lists together
  // [N]<----------------------------------->[N]
  // [N]<----------------------------------->[N]
  // ..........................................................................  
  JoinStartEndNodes(&(skiplist->_base._startNode),&(skiplist->_base._endNode),0, skiplist->_base._maxHeight - 1); // joins list 0 & 1  
  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipListExMulti (TRI_skiplistEx_multi_t* skiplist) {
  if (skiplist != NULL) {
    DestroyBaseSkipListEx( (TRI_skiplistEx_base_t*)(skiplist) );
  }  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipListExMulti (TRI_skiplistEx_multi_t* skiplist) {
  if (skiplist != NULL) {
    TRI_DestroySkipListExMulti(skiplist);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplist);
  }    
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
/// @brief adds an element to a multi skip list using an element for searching
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipListExMulti(TRI_skiplistEx_multi_t* skiplist, 
                                     void* element, 
                                     bool overwrite, 
                                     uint64_t thisTransID) {
  int32_t newHeight;
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  TRI_skiplistEx_node_t* newNode;
  int compareResult;
  int result;
  int casFailures = -1;
  
                                     
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
  
  newHeight = RandLevel(&(skiplist->_base)) + 1;

  // ...........................................................................  
  // Something wrong since the newHeight must be non-negative
  // ...........................................................................  
  
  if (newHeight < 1) {
    return TRI_ERROR_INTERNAL;
  }  
  
  
  // ...........................................................................  
  // Grow lists if required by increasing the height of the start and end nodes
  // ...........................................................................  
  
  result = GrowStartEndNodes(&(skiplist->_base), newHeight);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
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
  
  result = IndexStaticCopyElementElement(&(skiplist->_base), &(newNode->_element), element);
  result = GrowNewNodeHeight(newNode, newHeight, newHeight, result);
  if (result != TRI_ERROR_NO_ERROR) {
    FreeSkipListExNode(&(skiplist->_base), newNode);
    return result;
  }
  
  // ...........................................................................  
  // Assign the deletion transaction id and the insertion transaction id
  // ...........................................................................  
  
  newNode->_delTransID = UINT64_MAX; // since we are inserting this new node it can not be deleted
  newNode->_insTransID = thisTransID; // this is what was given to us

  
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      FreeSkipListExNode(&(skiplist->_base), newNode);
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, lock? The sleep time should be something 
    // needs to be adjusted.
    // ...........................................................................  
    
    if (casFailures > -1) {
      usleep(CAS_FAILURE_SLEEP_TIME);       
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the path where the new item is to be inserted. If the item 
    // already exists either replace it or return false. Recall that this 
    // skip list is used for unique key/value pairs. Use the skiplist-multi 
    // non-unique key/value pairs.
    // ...........................................................................  

    currentLevel = skiplist->_base._startNode._colLength - 1; // NOT current height BUT current level is required here
    currentNode  = &(skiplist->_base._startNode);
    nextNode     = currentNode;
  

    START: {

    
      // .........................................................................
      // The current node (which we have called the nextNode below) should never
      // be null. Protect yourself in case something has gone wrong.
      // .........................................................................  
      
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      nextNode = (TRI_skiplistEx_node_t*)(nextNode->_column[currentLevel]._next);
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // An insert/lookup/removal SEARCH like this, can ONLY ever find 1 glass
      // node when we are very unlucky. (The GC makes the node glass and then
      // goes and unlinks the pointers.) If we skip the glass node, then we 
      // will have the wrong pointers to compare, so we have to CAS_RESTART
      // .........................................................................
      
      if (nextNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto CAS_RESTART;
      }
      
    
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
          newNode->_column[currentLevel]._next = nextNode;                    
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
        nextNode = currentNode;
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
        
        compareResult = IndexStaticMultiCompareElementElement(skiplist, element, &(nextNode->_element), -1);


        // .......................................................................    
        // The element matches the next element. Overwrite if possible and return.
        // The only possiblity of obtaining a compareResult equal to 0 is for the
        // the element being the same, NOT the keys being the same.
        // .......................................................................    

        if (compareResult == 0) {
          FreeSkipListExNode(&(skiplist->_base), newNode);
          if (overwrite) {
            // ...................................................................
            // Warning: there is NO check to ensure that this node has not been
            // previously deleted. 
            // ...................................................................
            result = IndexStaticCopyElementElement(&(skiplist->_base), &(nextNode->_element), element);
            return result;
          }
          return TRI_ERROR_ARANGO_INDEX_SKIPLIST_INSERT_ITEM_DUPLICATED;
        }
        
        // .......................................................................    
        // The element to be inserted has a key which is greater than the next node's 
        // element key. Keep going on this level.
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
          newNode->_column[currentLevel]._next = nextNode;
        }
        
        
        // .......................................................................    
        // We have reached the lowest level of the lists. Time to insert item.
        // Note that we will insert this item to the left of all the items with
        // the same key. Note also that the higher transaction numbers are to
        // the left always.
        // .......................................................................    
        
        if (currentLevel == 0) {
          goto END;
        }
        
        
        // .......................................................................    
        // Drop down the list
        // .......................................................................    
        
        nextNode = currentNode;
        --currentLevel;
        
        goto START;
      }  

    } // end of label START
    
  } // end of label CAS_RESTART
    
    
  END: {

    // ..........................................................................
    // Ok finished with the loop and we should have a path with AT MOST
    // SKIPLIST_EX_ABSOLUTE_MAX_HEIGHT number of elements.
    // ..........................................................................
    

    // ..........................................................................
    // this is the tricky part since we have to attempt to do this as 
    // 'lock-free' as possible. This is acheived in three passes:
    // Pass 1: Mark each prev and next node of the new node so that the GC
    //         can not modify it. If this fails goto CAS_RESTART
    // Pass 2: Ensure that each prev and next tower is not glassed.
    // Pass 3: Modify the newnode.prev.next to newnode and newnode.next.prev = newnode
    // ..........................................................................
    
    result = JoinNewNodeCas(newNode);
    if (result == TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE) {
      goto CAS_RESTART;
    }
    return result;
    
  } // end of END label
    
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to a multi skip list 
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* key, void* element, bool overwrite, uint64_t thisTransID) {
  // Use TRI_InsertElementSkipListExMulti instead of calling this method
  assert(false);
  return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns greatest node less than a given key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* key, uint64_t thistransID) {
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode;
  TRI_skiplistEx_node_t* nextNode;
  int casFailures = -1;
  
  // ...........................................................................  
  // Just in case ...
  // ...........................................................................  
  
  if (skiplist == NULL) {
    LOG_ERROR("Internal Error");
    return NULL;
  }

  
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      return NULL;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, block?
    // ...........................................................................  
    
    if (casFailures > -1) {
      assert(0); // a test to see why it blocks - should not block!
      usleep(CAS_FAILURE_SLEEP_TIME);       
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the starting level and the starting node
    // ...........................................................................  
    
    currentLevel = skiplist->_base._startNode._colLength - 1;
    currentNode  = &(skiplist->_base._startNode);
    nextNode     = currentNode;
                 
  
    START: {

    
      // .........................................................................
      // Find the next node in the current level of the lists. Protect yourself
      // in case something has gone wrong.
      // .........................................................................  
      
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      
      
      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      nextNode = (TRI_skiplistEx_node_t*)(nextNode->_column[currentLevel]._next);
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // Is our next node a glass node? If so we must skip it!
      // Note: since Garbage Collection is performed in TWO passes, it is possible
      // that we have more than one glass node.
      // .........................................................................
      
      if (nextNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto START;
      }
            
      
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
        // yet. The currentNode does NOT compare and the next node is +\infinty.
        // .......................................................................
        
        if (currentLevel == 0) {
          return currentNode;
        }
        
        
        // .......................................................................
        // We have not yet reached the lowest level continue down. Possibly our
        // item we seek is to be found a lower level.
        // .......................................................................

        nextNode = currentNode;        
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
        // the next node element. We treat the comparison by assuming we are 
        // looking for a "key - epsilon". With this assumption we always find the
        // last key to our right if it exists. The reason this is necessary is as 
        // follows: we allow a multiple documents with the same key to be stored
        // here with the proviso that all but the last one is marked as deleted.
        // This is how we cater for multiple revisions.
        // .......................................................................    
        
        compareResult = IndexStaticMultiCompareKeyElement(skiplist, key, &(nextNode->_element), -1);
        
        // .......................................................................    
        // -1 is returned if the number of fields (attributes) in the key is LESS
        // than the number of fields in the index definition. This has the effect
        // of being slightly less efficient since we have to proceed to the level
        // 0 list in the set of skip lists. Where we allow duplicates such as this
        // -1 is also returned when all the keys match.
        // .......................................................................

        // .......................................................................
        // We have found the item!
        // .......................................................................

        if (compareResult == 0) {
          assert(false);
        }
        
        if (compareResult > 0) {
          currentNode = nextNode;
          goto START;
        }  


        // .......................................................................    
        // The element is less than the next node. Can we drop down the list?
        // If have reached the lowest level of the lists -- no such item.
        // .......................................................................    
        
        if (currentLevel == 0) {
          return currentNode;
        }
        
        
        // .......................................................................    
        // Drop down the list
        // .......................................................................    
        
        --currentLevel;
        nextNode = currentNode;
        goto START;
      }  
    
    } // end of label START

  } // end of label CAS_RESTART
    
  assert(false);  
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
  // Since this index supports duplicate keys, it makes no sense to lookup an element in the index
  // using a key - if there are such elements - what is returned is undefined (in the sense that a valid
  // element is returned but which one?). Hence lookups can only really make sense to say give me the 
  // first such element and the last such element, so that we can traverse the elements which match the
  // keys.
  assert(false); // there is no way you should be here
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



int TRI_RemoveElementSkipListExMulti (TRI_skiplistEx_multi_t* skiplist, void* element, void* old, 
                                      const int passLevel, const uint64_t thisTransID,
                                      TRI_skiplistEx_node_t** passNode) {
  // ...........................................................................
  // To remove an element from this skip list we have three pass levels:
  // Pass 1: locate (if possible) the exact NODE - must match exactly.
  //         Once located, add the transaction id to the node. Return.
  // Pass 2: locate the node (if not possible report error) - must match exactly.
  //         Once located, attempt to unlink all the pointers and make the
  //         node a Glass Node.
  // Pass 3: Excise the node by destroying it's allocated memory. 
  // ...........................................................................
  
  int32_t currentLevel;
  TRI_skiplistEx_node_t* currentNode = NULL;
  TRI_skiplistEx_node_t* nextNode = NULL;
  int casFailures = -1;
  
  // ...........................................................................  
  // Just in case
  // ...........................................................................  
  
  if (skiplist == NULL) {
    LOG_ERROR("Internal Error");
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................  
  // Only for pass level 1 do we have a requirement to locate the actual node
  // using the key. For pass levels 2 & 3 we have the pointer to the node. 
  // ...........................................................................  
  
  if (passLevel != 1) { goto END; }
    
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_CAS_FAILURE;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, block?
    // ...........................................................................  
    
    if (casFailures > -1) {
      usleep(CAS_FAILURE_SLEEP_TIME);            
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the starting level and the starting node
    // ...........................................................................  
    
    currentLevel = skiplist->_base._startNode._colLength - 1;
    currentNode  = &(skiplist->_base._startNode);
    nextNode     = currentNode;
                 
  
    START: {

      // .........................................................................
      // Find the next node in the current level of the lists. Protect yourself
      // in case something has gone wrong.
      // .........................................................................  
      
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      
      
      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      nextNode = (TRI_skiplistEx_node_t*)(nextNode->_column[currentLevel]._next);
      if (nextNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // Is our next node a glass node? If so we must skip it. Recall we are in 
      // Phase I here -- meaning that we are searching for a node which has not
      // be removed and previously inserted. 
      // .........................................................................

      if (nextNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto START;
      }
      

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
        
        nextNode = currentNode;
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
        
        compareResult = IndexStaticMultiCompareElementElement(skiplist,element,&(nextNode->_element), -1);

        
        // .......................................................................    
        // The element is greater than the next node element. Keep going on this
        // level.
        // .......................................................................    
        if (compareResult > 0) {
          currentNode = nextNode;
          goto START;
        }  
                
        
        if (compareResult == 0) { // a node matches exactly based upon the element
                   
          if (nextNode->_delTransID > thisTransID) { 
          
            // ...................................................................
            // Node has NOT been deleted (e.g. imagine it will be deleted some 
            // time in the future). This is the node we want.
            // ...................................................................            
            
            currentNode = nextNode;
            goto END;          
          }   
 
 
          // .....................................................................
          // In a skiplist supporting duplicate entries, the comparison function
          // test ensures the elements are the same (e.g. same address in memory)
          // it can never be the case that we rely simply on the keys matching.
          // So the question remains: why has the item has been previously 
          // deleted? Has someone tried to remove this item twice?
          // Don't know return error.
          // .....................................................................
          
          return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_PRIOR_REMOVED; 
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
        nextNode = currentNode;        
        goto START;
      }  
      
    } // end of START label
  } // end of CAS_RESTART label
  
  END: {
  
    switch (passLevel) {
    
      // .........................................................................
      // In this case we simply add the del transaction id with a CAS statement.
      // It should never fail!
      // .........................................................................

      case 1: {
        bool ok;
        
        if (currentNode == NULL) { // something terribly wrong
          assert(0);
          return TRI_ERROR_INTERNAL;
        }

        ok = TRI_CompareAndSwapIntegerUInt64 (&(currentNode->_delTransID), 
                                              UINT64_MAX, thisTransID);
        if (!ok) {
          assert(0);
          return TRI_ERROR_INTERNAL;
        }
        // ....................................................................
        // If requested copy the contents of the element we have located into the
        // storage sent.
        // ....................................................................
  
        if (old != NULL) {
          IndexStaticCopyElementElement(&(skiplist->_base), old, &(currentNode->_element));
        }
        
        *passNode = currentNode;
        
        return TRI_ERROR_NO_ERROR;        
      }
      
      
      // .........................................................................
      // In this case we wish to make the node a glass node and to unjoin all
      // other connected nodes.
      // .........................................................................
      case 2: {
      
        // .......................................................................
        // We can not now rely upon looking up the node using the key, since 
        // we would need to traverse right and attempt to match either then 
        // transaction id and/or the pointer to the doc. Easier to simply
        // send the address of the node back.
        // .......................................................................
        if (*passNode == NULL) {
          return TRI_ERROR_INTERNAL;
        }  
        currentNode  = (TRI_skiplistEx_node_t*)(*passNode);
        
        
        // .......................................................................
        // Only the Garbage Collector can transform a node into a glass node, and
        // since the GC is only operating in one thread safe to do a simple
        // comparison here.
        // .......................................................................
        if (currentNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
          return TRI_ERROR_INTERNAL;
        }  

        // .......................................................................
        // safety check         
        // .......................................................................
        if (currentNode->_delTransID != thisTransID) {
          return TRI_ERROR_INTERNAL;
        }
        
        // .......................................................................
        // The stragey is this:
        // (a) Brick each nearest neighbour on this node. This ensures that NO
        // other nodes can be attached to this node.
        // (b) Mark this node as being glass. This ensures that it is skipped
        // since it is no longer required in the index.
        // (c) Unbrick each of its nearest neighbours on this node. This ensures
        // that an inserted node MAY be allowed to be attached but will later fail.
        // Also allows us to brick other glass nodes.
        // (d) Brick each prev and next nearest neighbour of this node. Irrespective
        // if one of these are glass or not. This ensures that lookups can 
        // proceed unhinded.
        // (e) Unjoin the node from the list.
        // (f) Unbrick each prev/next nearest neigbour
        // .......................................................................
        
        return UnJoinOldNodeCas(currentNode);
      }
      
      
      // .........................................................................
      // In this case since no other reader/writer can be accessing the node,
      // we simply destroy it. we require the node to be glass.
      // .........................................................................
      case 3: {
        // .......................................................................
        // We can not now rely upon looking up the node using the key, since 
        // we would need to traverse right and attempt to match either then 
        // transaction id and/or the pointer to the doc. Easier to simply
        // send the address of the node back.
        // .......................................................................
        if (*passNode == NULL) {
          return TRI_ERROR_INTERNAL;
        }  
        currentNode  = (TRI_skiplistEx_node_t*)(*passNode);
        
        // .......................................................................
        // Only the Garbage Collector can transform a node into a glass node, and
        // since the GC is only operating in one thread safe to do a simple
        // comparison here.
        // .......................................................................
        if (currentNode->_towerFlag != TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
          return TRI_ERROR_INTERNAL;
        }  

        // .......................................................................
        // safety check         
        // .......................................................................
        if (currentNode->_delTransID != thisTransID) {
          return TRI_ERROR_INTERNAL;
        }
  
        FreeSkipListExNode(&(skiplist->_base), currentNode);
        
        break;
        
      }
      
      default: {
        assert(0);
        return TRI_ERROR_INTERNAL;
      }
      
    } // end of switch statement
    
  } // end of END label
  
  return TRI_ERROR_NO_ERROR;
 
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes a key/element from a multi skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipListExMulti(TRI_skiplistEx_multi_t* skiplist, void* key, void* old, 
                                 const int passLevel, const  uint64_t thisTransID,
                                 TRI_skiplistEx_node_t** passNode) {
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
  int casFailures = -1;
  
  // ...........................................................................  
  // Just in case ...
  // ...........................................................................  
  
  if (skiplist == NULL) {
    LOG_ERROR("Internal Error");
    return NULL;
  }

  
  // ...........................................................................  
  // Big loop to restart the whole search routine
  // ...........................................................................  
  
  CAS_RESTART: {
  
    // ...........................................................................  
    // To stop this loop CAS_RESTART becomming an infinite loop, use this check
    // ...........................................................................  
    
    if (casFailures == SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS Failure");
      return NULL;
    }
    
    
    // ...........................................................................  
    // Provide a simple non-blocking, block?
    // ...........................................................................  
    
    if (casFailures > -1) {
      assert(0); // a test to see why it blocks - should not block!
      usleep(CAS_FAILURE_SLEEP_TIME);       
    }
  
  
    // ...........................................................................  
    // Increment the cas failures (which should always be hopefully 0).
    // ...........................................................................  
  
    ++casFailures;
  
  
    // ...........................................................................  
    // Determine the starting level and the starting node
    // ...........................................................................  
    
    currentLevel = skiplist->_base._endNode._colLength - 1;
    currentNode  = &(skiplist->_base._endNode);
    prevNode     = currentNode;
                 
  
    START: {

    
      // .........................................................................
      // Find the next node in the current level of the lists. Protect yourself
      // in case something has gone wrong.
      // .........................................................................  
      
      if (prevNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      
      
      // .........................................................................
      // We require the successor of the current node so we can perform a 
      // comparison. It should never be null.
      // .........................................................................
      
      prevNode = (TRI_skiplistEx_node_t*)(prevNode->_column[currentLevel]._prev);
      if (prevNode == NULL) {
        LOG_ERROR("CAS Failure");
        assert(0);
        goto CAS_RESTART;
      }
      

      // .........................................................................
      // Is our next node a glass node? If so we must skip it!
      // Note: since Garbage Collection is performed in TWO passes, it is possible
      // that we have more than one glass node.
      // .........................................................................
      
      if (prevNode->_towerFlag == TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) {
        goto START;
      }
            
      
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
        // yet. The currentNode does NOT compare and the next node is +\infinty.
        // .......................................................................
        
        if (currentLevel == 0) {
          return currentNode;
        }
        
        
        // .......................................................................
        // We have not yet reached the lowest level continue down. Possibly our
        // item we seek is to be found a lower level.
        // .......................................................................

        prevNode = currentNode;        
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
        // the next node element. We treat the comparison by assuming we are 
        // looking for a "key - epsilon". With this assumption we always find the
        // last key to our right if it exists. The reason this is necessary is as 
        // follows: we allow a multiple documents with the same key to be stored
        // here with the proviso that all but the last one is marked as deleted.
        // This is how we cater for multiple revisions.
        // .......................................................................    
        
        compareResult = IndexStaticMultiCompareKeyElement(skiplist, key, &(prevNode->_element), 1);
        
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
        
        if (compareResult < 0) {
          currentNode = prevNode;
          goto START;
        }  


        // .......................................................................    
        // The element is less than the next node. Can we drop down the list?
        // If have reached the lowest level of the lists -- no such item.
        // .......................................................................    
        
        if (currentLevel == 0) {
          return currentNode;
        }
        
        
        // .......................................................................    
        // Drop down the list
        // .......................................................................    
        
        --currentLevel;
        prevNode = currentNode;
        goto START;
      }  
    
    } // end of label START

  } // end of label CAS_RESTART
    
  assert(false);  
  return NULL;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node associated with a multi skip list.
////////////////////////////////////////////////////////////////////////////////

void* TRI_StartNodeSkipListExMulti(TRI_skiplistEx_multi_t* skiplist) {
  if (skiplist != NULL) {
    return &(skiplist->_base._startNode);
  }
  return NULL;  
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

  // ...........................................................................
  // No locking, blocking or CAS here. Someone asked for the index to destroyed.
  // We assume that no further read/write operations are being accepted which
  // require this index. 
  // TODO:
  // Warning: there is a memory leak which requires fixing here. The Garbage
  // collection may be working in the background and if we destroy the
  // skiplist before the Garbage collection thread has been terminated - then
  // bang! 
  // The idea is to send the Garbage collector a signal so that ALL references
  // to this index are expunged, then the same process will call this function.
  // ...........................................................................
  
  fix the GC so that we have an expunge function!
  
  TRI_skiplistEx_node_t* nextNode;
  TRI_skiplistEx_node_t* nextNextNode;

  if (baseSkiplist == NULL) {
    return;
  }  
  
  nextNode = &(baseSkiplist->_startNode);
  
  while (nextNode != NULL) {
    nextNextNode = (TRI_skiplistEx_node_t*)(nextNode->_column[0]._next);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)(nextNode->_column));
    if ((nextNode != &(baseSkiplist->_startNode)) && (nextNode != &(baseSkiplist->_endNode))) {
      IndexStaticDestroyElement(baseSkiplist, &(nextNode->_element));
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, nextNode);
    }
    nextNode = nextNextNode;    
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
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*)(node->_column));
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


// ...............................................................................
// This function is thread safe since the node has just been created and has 
// NOT YET been linked into the skiplist.
// ...............................................................................

static int GrowNewNodeHeight(TRI_skiplistEx_node_t* node, uint32_t height, uint32_t colLength, int result) {
  
  // ............................................................................
  // Don't go any further if we already have a previous error, simply return that error.
  // ............................................................................

  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }

  if (colLength > height) {
    assert(0);
    return TRI_ERROR_INTERNAL;
  }
  
  node->_colLength = colLength;  
  
  node->_column = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_nb_t) * height, false);
  
  if (node->_column == NULL) { // out of memory?
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  
  // ..........................................................................  
  // Ensure that the towers are normal, at least initially for a new node
  // ..........................................................................  
  
  node->_towerFlag = TRI_SKIPLIST_EX_NORMAL_TOWER_NODE_FLAG;

  
  // ...........................................................................
  // Initialise the storage
  // ...........................................................................
  {
    uint32_t j;
    for (j = 0; j < height; ++j) {
      node->_column[j]._prev     = NULL; 
      node->_column[j]._next     = NULL; 
      node->_column[j]._nbFlag   = TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG;
    }
  }
  
  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief joins a left node and right node together
////////////////////////////////////////////////////////////////////////////////



static void JoinStartEndNodes(TRI_skiplistEx_node_t* leftNode, 
                              TRI_skiplistEx_node_t* rightNode, 
                              uint32_t startLevel, uint32_t endLevel) {
  if (startLevel > endLevel) { // something wrong
    assert(false);
    return;
  }  

  // change level to height
  endLevel += 1;

  {  
    uint32_t j;
    for (j = startLevel; j < endLevel; ++j) {
      (leftNode->_column)[j]._next = rightNode;  
      (rightNode->_column)[j]._prev = leftNode;  
    }  
  }    
}


////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void* NextNodeBaseSkipListEx(TRI_skiplistEx_base_t* skiplist, void* currentNode, uint64_t thisTransID) {
  TRI_skiplistEx_node_t* volatile nn = (TRI_skiplistEx_node_t* volatile)(currentNode);
  
  if (nn == NULL) {
    nn = &(skiplist->_startNode);
  }  

  
  // ...........................................................................
  // We are required to skip certain nodes based upon the transaction id
  // ...........................................................................
  
  while (nn != &(skiplist->_endNode)) {
    nn = nn->_column[0]._next;

    if (nn == NULL) { 
      // this should not happen!
      LOG_ERROR("CAS Failure");          
      assert(0);
      return NULL; 
    }
    
    if (nn->_insTransID > thisTransID) { // item was inserted AFTER this transaction started - skip it
      continue;  
    }
    
    if (nn->_delTransID <= thisTransID) { // item has been previously deleted - skip it
      continue;  
    }
     
    return (void*)(nn);
  };
  
  return(NULL);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node (if possible) in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void* PrevNodeBaseSkipListEx(TRI_skiplistEx_base_t* skiplist, void* currentNode, uint64_t thisTransID) {
  TRI_skiplistEx_node_t* volatile pn = (TRI_skiplistEx_node_t*)(currentNode);
  
  if (pn == NULL) {
    return &(skiplist->_endNode);
  }  
  
  
  // ...........................................................................
  // We are required to skip certain nodes based upon the transaction id
  // ...........................................................................
  
  while (pn != &(skiplist->_startNode)) {
    pn = pn->_column[0]._prev;
    
    if (pn == NULL) { 
      // this should not happen!
      LOG_ERROR("CAS Failure");          
      assert(0);
      return NULL; 
    }
    
    if (pn->_insTransID > thisTransID) { // item was inserted AFTER this transaction started - skip it
      continue;  
    }
    
    if (pn->_delTransID <= thisTransID) { // item has been previously deleted - skip it
      continue;  
    }
    
    return (void*)(pn);
    
  };
  
  return(NULL);
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


// .................................................................................
// If we have simultaneous inserts, then this function will keep retrying and 
// attempt to wait until the CAS statement succeed. It is safe for 
// simultaneous inserts.
// .................................................................................

static int GrowStartEndNodes(TRI_skiplistEx_base_t* skiplist, uint32_t newHeight) {
  int result = TRI_ERROR_NO_ERROR;
  int retries = 0;
  uint32_t oldStartHeight, oldEndHeight;
  
  // ................................................................................
  // Is someone else growing the start/end nodes, if so return necessary error.
  // Notice that this loop is only necessary if we assume multiple unordered inserts.
  // ................................................................................

  while (true) {
    if (TRI_CompareAndSwapIntegerUInt32(&(skiplist->_growStartEndNodesFlag), TRI_SKIPLIST_EX_FREE_TO_GROW_START_END_NODES_FLAG, 
                                        TRI_SKIPLIST_EX_NOT_FREE_TO_GROW_START_END_NODES_FLAG) ) {
      break;
    }  
    ++retries;
    if (retries > SKIPLIST_EX_CAS_FAILURES_MAX_LOOP) {
      LOG_ERROR("CAS failed for GrowStartEndNodes");
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE;
    }
    usleep(CAS_FAILURE_SLEEP_TIME);    
  }  
  
  oldStartHeight = skiplist->_startNode._colLength;
  oldEndHeight   = skiplist->_endNode._colLength;
  
  if (oldStartHeight != oldEndHeight) {
    result = TRI_ERROR_INTERNAL;
  }

  if (result == TRI_ERROR_NO_ERROR) {
    if (oldStartHeight < newHeight) {
      // ............................................................................
      // need a CAS statement here since we may have multiple readers busy reading
      // the height of the towers.
      // ............................................................................
      if (!TRI_CompareAndSwapIntegerUInt32(&(skiplist->_startNode._colLength), 
                                           oldStartHeight, newHeight) ) {
        // should never happen
        result = TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE;
      }
      else {
        if (!TRI_CompareAndSwapIntegerUInt32(&(skiplist->_endNode._colLength), 
                                           oldEndHeight, newHeight) ) {
          // should never happen
          result = TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE;
        }      
        if (result != TRI_ERROR_NO_ERROR) {
          TRI_CompareAndSwapIntegerUInt32(&(skiplist->_startNode._colLength), newHeight, oldStartHeight);
        }  
      }  
    }
  }
  
  
  if (!TRI_CompareAndSwapIntegerUInt32(&(skiplist->_growStartEndNodesFlag), 
                                       TRI_SKIPLIST_EX_NOT_FREE_TO_GROW_START_END_NODES_FLAG, 
                                       TRI_SKIPLIST_EX_FREE_TO_GROW_START_END_NODES_FLAG) ) {
    // ..............................................................................
    // not possible - eventually send signal to database to rebuild index
    // ..............................................................................
    LOG_ERROR("CAS failed for GrowStartEndNodes");
    assert(0);
    if (result == TRI_ERROR_NO_ERROR) {
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE;
    }
  }  
   
  return result;  
}

static int UndoBricking (TRI_skiplistEx_node_t* node, int counter) {
  bool ok = true;
  int j;
  TRI_skiplistEx_nb_t* leftNN;
  TRI_skiplistEx_nb_t* rightNN;
  TRI_skiplistEx_node_t* leftNode;
  TRI_skiplistEx_node_t* rightNode;
  
  for (j = 0; j < counter; ++j) {
    leftNode  = (TRI_skiplistEx_node_t*)(node->_column[j]._prev);
    rightNode = (TRI_skiplistEx_node_t*)(node->_column[j]._next);
    leftNN    = &(leftNode->_column[j]);
    rightNN   = &(rightNode->_column[j]);
    
    ok = TRI_CompareAndSwapIntegerUInt32 (&(leftNN->_nbFlag), 
                                          TRI_SKIPLIST_EX_BRICKED_NEAREST_NEIGHBOUR_FLAG, 
                                          TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG) && ok;
    ok = TRI_CompareAndSwapIntegerUInt32 (&(rightNN->_nbFlag), 
                                          TRI_SKIPLIST_EX_BRICKED_NEAREST_NEIGHBOUR_FLAG, 
                                          TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG) && ok;
  }                                          
  
  if (!ok) {
    // should never occur - if it does eventually send signal to database to rebuild index
    LOG_ERROR("CAS failed for UndoBricking");
    assert(0);
    return TRI_ERROR_INTERNAL;              
  }            
    
  return TRI_ERROR_NO_ERROR;  
}          

static int DoBricking (TRI_skiplistEx_node_t* node, int* counter) {
  uint32_t j;
  TRI_skiplistEx_nb_t* leftNN;
  TRI_skiplistEx_nb_t* rightNN;
  TRI_skiplistEx_node_t* leftNode;
  TRI_skiplistEx_node_t* rightNode;
  bool ok = true;
  int result = TRI_ERROR_NO_ERROR;
  
  *counter = 0;
  
  for (j = 0; j < node->_colLength; ++j) {
    leftNode  = (TRI_skiplistEx_node_t*)(node->_column[j]._prev);
    rightNode = (TRI_skiplistEx_node_t*)(node->_column[j]._next);
    leftNN    = &(leftNode->_column[j]);
    rightNN   = &(rightNode->_column[j]);
        
    // left
    ok = TRI_CompareAndSwapIntegerUInt32 (&(leftNN->_nbFlag), 
                                          TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG, 
                                          TRI_SKIPLIST_EX_BRICKED_NEAREST_NEIGHBOUR_FLAG);
    if (!ok) { break; }
        
    // right
    ok = TRI_CompareAndSwapIntegerUInt32 (&(rightNN->_nbFlag), 
                                          TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG, 
                                          TRI_SKIPLIST_EX_BRICKED_NEAREST_NEIGHBOUR_FLAG);
    if (!ok) { 
      if (!TRI_CompareAndSwapIntegerUInt32 (&(leftNN->_nbFlag), 
                                            TRI_SKIPLIST_EX_BRICKED_NEAREST_NEIGHBOUR_FLAG, 
                                            TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG)) {
        // should never occur - if it does, then we need to eventually send signal to database to rebuild index
        abort();              
      }            
      break;
    }
        
    ++(*counter); 
  }
  
  if (ok) {
    return TRI_ERROR_NO_ERROR;
  }

  result = UndoBricking (node, *counter); 
  if (result == TRI_ERROR_NO_ERROR) {
    return TRI_WARNING_ARANGO_INDEX_SKIPLIST_INSERT_CAS_FAILURE;              
  }  
    
  LOG_ERROR("CAS failed for UndoBricking");
  assert(0);
  return result;                  
}  

static int UndoJoinPointers(TRI_skiplistEx_node_t* node, const int counter) {
  int j;
  TRI_skiplistEx_nb_t* leftNN;
  TRI_skiplistEx_nb_t* rightNN;
  TRI_skiplistEx_node_t* leftNode;
  TRI_skiplistEx_node_t* rightNode;
  bool ok = true;
  
  for (j = 0; j < counter; ++j) {
    leftNode  = (TRI_skiplistEx_node_t*)(node->_column[j]._prev);
    rightNode = (TRI_skiplistEx_node_t*)(node->_column[j]._next);
    leftNN    = &(leftNode->_column[j]);
    rightNN   = &(rightNode->_column[j]);
    ok = TRI_CompareAndSwapPointer((void* volatile*)(&(leftNN->_next)), node, rightNode) && ok;
    ok = TRI_CompareAndSwapPointer((void* volatile*)(&(rightNN->_prev)), node, leftNode) && ok;
  }
  
  if (!ok) {
    // should never occur - if it does eventually send signal to database to rebuild index
    LOG_ERROR("CAS failed for UndoBricking");
    assert(0);
    return TRI_ERROR_INTERNAL;              
  }            
    
  return TRI_ERROR_NO_ERROR;  
}

static int DoJoinPointers (TRI_skiplistEx_node_t* node, int* counter) {
  uint32_t j;
  TRI_skiplistEx_nb_t* leftNN;
  TRI_skiplistEx_nb_t* rightNN;
  TRI_skiplistEx_node_t* leftNode;
  TRI_skiplistEx_node_t* rightNode;
  bool ok = true;
  
  
  *counter = 0;
  for (j = 0; j < node->_colLength; ++j) {
    leftNode  = (TRI_skiplistEx_node_t*)(node->_column[j]._prev);
    rightNode = (TRI_skiplistEx_node_t*)(node->_column[j]._next);
    leftNN    = &(leftNode->_column[j]);
    rightNN   = &(rightNode->_column[j]);
    ok = TRI_CompareAndSwapPointer((void* volatile*)(&(leftNN->_next)), rightNode, node);
    if (!ok) { break; }
    ok = TRI_CompareAndSwapPointer((void* volatile*)(&(rightNN->_prev)), leftNode, node);
    if (!ok) {
      ok = TRI_CompareAndSwapPointer((void* volatile*)(&(leftNN->_next)), node, rightNode);
      if (!ok) {
        // should never occur - if it does eventually send signal to database to rebuild index
        abort();              
      }  
      break;
    }
    ++(*counter);
  }
  
  if (ok) {
    return TRI_ERROR_NO_ERROR;
  }
  
  UndoJoinPointers(node, *counter);
  return TRI_ERROR_INTERNAL;
}

static int JoinNewNodeCas (TRI_skiplistEx_node_t* newNode) {
  int brickCounter = 0;
  int pointerCounter = 0;
  int result = TRI_ERROR_NO_ERROR;
  uint32_t j;
  
  // Pass 1: do bricking
  result = DoBricking(newNode, &brickCounter);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }  

  
  // Pass 2: Ensure that each tower node is not glassed - glassing by the GC is NOT
  // possible if Pass 1 above has succeeded.
  for (j = 0; j < newNode->_colLength; ++j) {
    TRI_skiplistEx_node_t* leftNode = (TRI_skiplistEx_node_t*)(newNode->_column[j]._prev);
    TRI_skiplistEx_node_t* rightNode = (TRI_skiplistEx_node_t*)(newNode->_column[j]._next);
    if ( (leftNode->_towerFlag  != TRI_SKIPLIST_EX_NORMAL_TOWER_NODE_FLAG) || 
         (rightNode->_towerFlag != TRI_SKIPLIST_EX_NORMAL_TOWER_NODE_FLAG) ) { 
      result = UndoBricking (newNode, brickCounter);
      return result;      
    }
  }
      
      
  // Pass 3: Join the new node by assigning pointers
  result = DoJoinPointers(newNode, &pointerCounter);
  if (result != TRI_ERROR_NO_ERROR) {
    UndoBricking (newNode, brickCounter);
    return result;
  }  
     
        
  // Now unbrick the left/right nodes so other processes can access them
  result = UndoBricking (newNode, brickCounter);
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
// removal
//////////////////////////////////////////////////////////////////////////////////

static int SelfUndoBricking(TRI_skiplistEx_node_t* node, const int counter) {
  bool ok = true;
  int j;
  TRI_skiplistEx_nb_t* NN;
  
  for (j = 0; j < counter; ++j) {
    NN = &(node->_column[j]);
    ok = TRI_CompareAndSwapIntegerUInt32 (&(NN->_nbFlag), 
                                          TRI_SKIPLIST_EX_BRICKED_NEAREST_NEIGHBOUR_FLAG, 
                                          TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG) && ok;
  }                                          
  
  if (!ok) {
    // should never occur - if it does eventually send signal to database to rebuild index
    LOG_ERROR("CAS failed for UndoBricking");
    assert(0);
    return TRI_ERROR_INTERNAL;              
  }            
    
  return TRI_ERROR_NO_ERROR;  
}


static int SelfBricking(TRI_skiplistEx_node_t* node, int* counter) {
  uint32_t j;
  TRI_skiplistEx_nb_t* NN;
  bool ok = true;
  int result = TRI_ERROR_NO_ERROR;
  
  *counter = 0;
  
  for (j = 0; j < node->_colLength; ++j) {
    NN = &(node->_column[j]);
        
    ok = TRI_CompareAndSwapIntegerUInt32 (&(NN->_nbFlag), 
                                          TRI_SKIPLIST_EX_NORMAL_NEAREST_NEIGHBOUR_FLAG, 
                                          TRI_SKIPLIST_EX_BRICKED_NEAREST_NEIGHBOUR_FLAG);
    if (!ok) { break; }
    ++(*counter); 
  }
  
  if (ok) {
    return TRI_ERROR_NO_ERROR;
  }

  result = SelfUndoBricking(node, *counter); 
  if (result == TRI_ERROR_NO_ERROR) {
    return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_CAS_FAILURE;              
  }  
    
  LOG_ERROR("CAS failed for UndoBricking");
  assert(0);
  return result;                  
  
}


static int UndoUnjoinPointers(TRI_skiplistEx_node_t* node, const int counter) {
  int j;
  TRI_skiplistEx_nb_t* leftNN;
  TRI_skiplistEx_nb_t* rightNN;
  TRI_skiplistEx_node_t* leftNode;
  TRI_skiplistEx_node_t* rightNode;
  bool ok = true;
  
  for (j = 0; j < counter; ++j) {
    leftNode  = (TRI_skiplistEx_node_t*)(node->_column[j]._prev);
    rightNode = (TRI_skiplistEx_node_t*)(node->_column[j]._next);
    leftNN    = &(leftNode->_column[j]);
    rightNN   = &(rightNode->_column[j]);
    ok = TRI_CompareAndSwapPointer((void* volatile*)(&(leftNN->_next)),rightNode, node) && ok;
    ok = TRI_CompareAndSwapPointer((void* volatile*)(&(rightNN->_prev)), leftNode, node) && ok;
  }
  
  if (!ok) {
    // should never occur - if it does eventually send signal to database to rebuild index
    LOG_ERROR("CAS failed for UndoBricking");
    assert(0);
    return TRI_ERROR_INTERNAL;              
  }            
    
  return TRI_ERROR_NO_ERROR;  
}

static int DoUnjoinPointers (TRI_skiplistEx_node_t* node, int* counter) {
  uint32_t j;
  TRI_skiplistEx_nb_t* leftNN;
  TRI_skiplistEx_nb_t* rightNN;
  TRI_skiplistEx_node_t* leftNode;
  TRI_skiplistEx_node_t* rightNode;
  bool ok = true;
  
  
  *counter = 0;
  for (j = 0; j < node->_colLength; ++j) {
    leftNode  = (TRI_skiplistEx_node_t*)(node->_column[j]._prev);
    rightNode = (TRI_skiplistEx_node_t*)(node->_column[j]._next);
    leftNN    = &(leftNode->_column[j]);
    rightNN   = &(rightNode->_column[j]);
    ok = TRI_CompareAndSwapPointer((void* volatile*)(&(leftNN->_next)), node, rightNode);
    if (!ok) { break; }
    ok = TRI_CompareAndSwapPointer((void* volatile*)(&(rightNN->_prev)), node, leftNode);
    if (!ok) {
      ok = TRI_CompareAndSwapPointer((void* volatile*)(&(leftNN->_next)), rightNode, node);
      if (!ok) {
        // should never occur - if it does eventually send signal to database to rebuild index
        abort();              
      }  
      break;
    }
    ++(*counter);
  }
  
  if (ok) {
    return TRI_ERROR_NO_ERROR;
  }
  
  UndoUnjoinPointers(node, *counter);
  return TRI_ERROR_INTERNAL;
}



static int UnJoinOldNodeCas (TRI_skiplistEx_node_t* oldNode) {
  int selfBrickCounter = 0;
  int brickCounter = 0;
  int pointerCounter = 0;
  int result = TRI_ERROR_NO_ERROR;
  
  // Pass 1: brick the nearest neighbours on the node itself.
  result = SelfBricking(oldNode, &selfBrickCounter);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }
  

  // Pass 2: make the node glass  
  if (!TRI_CompareAndSwapIntegerUInt32 (&(oldNode->_towerFlag), 
                                        TRI_SKIPLIST_EX_NORMAL_TOWER_NODE_FLAG,
                                        TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG) ) {
    SelfUndoBricking(oldNode,selfBrickCounter);
    return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_CAS_FAILURE;              
  }

  // Pass 3: unbrick each nearest neigbour node here
  result = SelfUndoBricking(oldNode,selfBrickCounter);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }
  
  
  // Pass 4: brick each of it's nearest neighbours
  result = DoBricking(oldNode, &brickCounter);
  if (result != TRI_ERROR_NO_ERROR) {
    TRI_CompareAndSwapIntegerUInt32 (&(oldNode->_towerFlag), 
                                     TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG,
                                     TRI_SKIPLIST_EX_NORMAL_TOWER_NODE_FLAG);
    if (result != TRI_ERROR_INTERNAL) {
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_CAS_FAILURE;
    }  
    return result;
  }  

  // Pass 5: unjoin the old node from the list by assigning pointers
  result = DoUnjoinPointers(oldNode, &pointerCounter);
  if (result != TRI_ERROR_NO_ERROR) {
    UndoBricking(oldNode,brickCounter);
    TRI_CompareAndSwapIntegerUInt32 (&(oldNode->_towerFlag), 
                                     TRI_SKIPLIST_EX_GLASS_TOWER_NODE_FLAG,
                                     TRI_SKIPLIST_EX_NORMAL_TOWER_NODE_FLAG);
    if (result != TRI_ERROR_INTERNAL) {
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_CAS_FAILURE;
    }  
    return result;
  }  
        
     
        
  // Now unbrick the left/right nodes so other processes can access them
  result = UndoBricking (oldNode, brickCounter);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
