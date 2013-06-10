////////////////////////////////////////////////////////////////////////////////
/// @brief index garbage collector
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "VocBase/index-garbage-collector.h"

#include "BasicsC/locks.h"
#include "BasicsC/logging.h"
#include "BasicsC/skip-list.h"
#include "VocBase/document-collection.h"
#include "VocBase/index.h"
#include "VocBase/transaction.h"



// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief the number of times the Garbage Collector will retry when a CAS statement fails
////////////////////////////////////////////////////////////////////////////////

static int const MAX_INDEX_GC_CAS_RETRIES = 100;


////////////////////////////////////////////////////////////////////////////////
/// @brief the period between garbage collection tries in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const INDEX_GC_INTERVAL = (1 * 1000 * 1000);


////////////////////////////////////////////////////////////////////////////////
/// @brief the amount of time to sleep when a CAS statement fails (in microseconds)
////////////////////////////////////////////////////////////////////////////////

static unsigned int CAS_FAILURE_SLEEP_TIME = 1000;

// .............................................................................
// The rubbish collection operates as a simple linked list. Whenever an index
// requests an item to be added to the collector, we insert a node at the end
// of this linked list. Each item may require 1,..,n passes before we can
// say that the item has been destroyed. Once all passes have been completed
// for an item, the item is excised from the linked list. (The excision 
// occurs using CAS operations so that the amount of 'blocking' is minimised.
// Note that there is no ordering to the list, first come first served.
// .............................................................................


typedef struct linked_list_node_s {
  TRI_index_gc_t* _indexData;
  void* volatile _next; // (struct linked_list_node_s* _next) the value _next is required to be volatile
  void* volatile _prev; // (struct linked_list_node_s* _prev) the value _prev is required to be volatile
  volatile  uint32_t _nodeFlag;
} linked_list_node_t;

typedef struct linked_list_s {
  linked_list_node_t _startNode;
  linked_list_node_t _endNode;  
  volatile uint32_t _listFlag;
  uint64_t  _size;  
} linked_list_t;


enum {
  INDEX_GC_LIST_NORMAL_FLAG,     
  INDEX_GC_LIST_FORBIDDEN_FLAG,     
  INDEX_GC_NODE_NORMAL_FLAG,     
  INDEX_GC_NODE_BRICKED_FLAG,     
  INDEX_GC_NODE_DELETED_FLAG,     
  INDEX_GC_NODE_INSERTED_FLAG     
};  


// .............................................................................
// The linked list has to be stored somewhere. Store it here.
// .............................................................................

static linked_list_t* INDEX_GC_LINKED_LIST = NULL;
static void*          INDEX_GC_DATA        = NULL;

 
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

static int  ExciseNode (linked_list_node_t*); 
static int  ExciseNodeBrick (linked_list_node_t* nodeToExcise, linked_list_node_t* prevNode, linked_list_node_t* nextNode);
static int  ExciseNodeBrickUndo (linked_list_node_t* nodeToExcise, linked_list_node_t* prevNode, linked_list_node_t* nextNode, int bricked);
static int  ExciseNodeSwapPointers (linked_list_node_t* nodeToExcise, linked_list_node_t* prevNode, linked_list_node_t* nextNode);
static int  ExciseNodeSwapPointersUndo(linked_list_node_t* nodeToExcise, linked_list_node_t* prevNode, linked_list_node_t* nextNode, int swaped);

static void InitialiseStaticLinkedList (void);
static void InnerThreadLoop (bool*);

static int  InsertNode (linked_list_node_t*); 
static int  InsertNodeBrick (linked_list_node_t* prevNode, linked_list_node_t* nextNode); 
static int  InsertNodeBrickUndo (linked_list_node_t* prevNode, linked_list_node_t* nextNode, int bricked); 
static int  InsertNodeSwapPointers (linked_list_node_t* nodeToInsert, linked_list_node_t* prevNode, linked_list_node_t* nextNode); 
static int  InsertNodeSwapPointersUndo (linked_list_node_t* nodeToInsert, linked_list_node_t* prevNode, linked_list_node_t* nextNode, int swaped);

static void RemoveLinkedList (void);
static void SetForbiddenFlag (void);
static void UnsetForbiddenFlag (void);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief index garbage collection event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_IndexGCVocBase (void* data) {

  bool goToSleep;
  TRI_vocbase_t* vocbase = (TRI_vocbase_t*)(data);
  
  
  LOG_TRACE("attempting to start the index garbage collector ...");
  
  // ..........................................................................
  // Check that the database is in 'normal' operational mode before starting
  // this thread 
  // ..........................................................................
  
  if (vocbase->_state != 1) {
    LOG_FATAL_AND_EXIT("Index garbage collector can not start with when server is in state %d.",vocbase->_state);
  }  

  
  // ..........................................................................
  // Initialise the static linked list: INDEX_GC_LINKED_LIST
  // ..........................................................................
  
  InitialiseStaticLinkedList();
  INDEX_GC_DATA = data;
  
  
  // ..........................................................................
  // The main 'event loop' for this thread
  // ..........................................................................
  
  LOG_TRACE("the index garbage collector event loop has started");
  
  
  while (true) {
  
    // ........................................................................
    // keep initial _state value as vocbase->_state might change during 
    // execution within the loop
    // ........................................................................
    
    int oldState = vocbase->_state;
   
    
    // ........................................................................
    // The loop goes to sleep whenever we are at the end of the linked list.
    // ........................................................................
        
    goToSleep = TRI_ComparePointer(
                    &((INDEX_GC_LINKED_LIST->_startNode)._next), 
                    &(INDEX_GC_LINKED_LIST->_endNode));

    InnerThreadLoop (&goToSleep); 
    
    
    // goToSleep = true;
    //printf("oreste:%s:%d:gotosleep=%d:state=%d\n",__FILE__,__LINE__,goToSleep,vocbase->_state);
    
    if (vocbase->_state == 1 && goToSleep) { // only sleep while server is still running
        
      // ........................................................................
      // Sleep for the specified interval
      // ........................................................................
      
      usleep(INDEX_GC_INTERVAL);
  
    }

    if (oldState == 2) {  // server shutdown, terminate this thread 
      break;
    }
  }

  // ..........................................................................
  // Change the flag of the static linked list so that no more insertions
  // can be made.
  // ..........................................................................
  
  SetForbiddenFlag();
  
  
  // ..........................................................................
  // We need to wait a little while in case there are any other threads which
  // are busy adding things to the collector
  // ..........................................................................
  
  usleep(INDEX_GC_INTERVAL);
  
  
  // ..........................................................................
  // Remove all memory we assigned to any structures
  // ..........................................................................

  RemoveLinkedList();
  
  LOG_TRACE("the index garbage collector event loop has stopped");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Adds a node to the linked list, so that eventually the GC will remove an
// item from the given index.
////////////////////////////////////////////////////////////////////////////////

int TRI_AddToIndexGC(TRI_index_gc_t* indexData) {

  int result = TRI_ERROR_NO_ERROR;
  linked_list_node_t* insertNode; // node to be inserted into our linked list
  TRI_vocbase_t* vocbase = (TRI_vocbase_t*)(INDEX_GC_DATA); 
  
  // ...........................................................................                           
  // Check if the gc has actually started
  // ...........................................................................                           
  
  if (vocbase == NULL) {
    return TRI_ERROR_INTERNAL;
  }  

  // ...........................................................................                           
  // Check if the server has shut down?
  // ...........................................................................                           
  
  if (vocbase->_state == -1) {
    return TRI_WARNING_ARANGO_INDEX_GARBAGE_COLLECTOR_SHUTDOWN;
  }  

  // ...........................................................................                           
  // Check that we have something to add
  // ...........................................................................                           
 
  
  // ...........................................................................                           
  // Check that we have something to add
  // ...........................................................................                           

  if (indexData == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  insertNode = (linked_list_node_t*)(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(linked_list_node_t), true));
  
  if (insertNode == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  insertNode->_indexData = (TRI_index_gc_t*)(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_index_gc_t), true));
  if (insertNode->_indexData == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, insertNode);
  }  
  
  
  insertNode->_indexData->_index          = indexData->_index;
  insertNode->_indexData->_passes         = indexData->_passes;
  insertNode->_indexData->_lastPass       = 0;
  insertNode->_indexData->_data           = indexData->_data;
  insertNode->_indexData->_collectGarbage = indexData->_collectGarbage;
  // TODO: get the current transaction id
  //insertNode->_indexData->_transID        = vocbase->_transactionStuff->_GetGlobalTransactionFigures(0);
  ++(insertNode->_indexData->_transID);
  
  // ...........................................................................
  // the assignment of the _next and _prev pointers must be done in a CAS loop 
  // within the IndexNode(...) function.
  // ...........................................................................
  
  insertNode->_next     = NULL;
  insertNode->_prev     = NULL;
  insertNode->_nodeFlag = INDEX_GC_NODE_NORMAL_FLAG;     

  result = InsertNode(insertNode);
  
  if (result !=  TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, insertNode->_indexData);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, insertNode);      
  }
  
  return result;  
}
                           
  

////////////////////////////////////////////////////////////////////////////////
// For the given index, all nodes which match the index will be excised from
// the linked list.
////////////////////////////////////////////////////////////////////////////////
  
int TRI_ExpungeIndexGC (TRI_index_gc_t* indexData) {
  int result = TRI_ERROR_NO_ERROR;
  linked_list_node_t* currentNode;
  bool finished = true;
  int casCounter = 0;
  
  LOG_TRACE("the index garbage collector has commenced expunging all nodes for a given index");
  
  CAS_LOOP: {
  
    result = TRI_ERROR_NO_ERROR;	
    currentNode = &(INDEX_GC_LINKED_LIST->_startNode);
      
    if (casCounter > MAX_INDEX_GC_CAS_RETRIES) {
      LOG_ERROR("max cas loop exceeded");
      return TRI_ERROR_INTERNAL;
    }
	
    ++casCounter;
	
    while (currentNode != NULL) {
      linked_list_node_t* tempNode = currentNode->_next;     
        
      if (currentNode->_indexData == NULL) {
        currentNode = tempNode;    
        continue;
      }		
	  
      if (indexData->_index != currentNode->_indexData->_index) {
        currentNode = tempNode;    
        continue;
      }

      // .......................................................................
      // Just before we remove the data and associated data, go to the index
      // and indicate that we are about to remove the node from the linked list
      // .......................................................................
          
      indexData->_lastPass = 254;
      result = indexData->_collectGarbage(indexData);
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector called the callback which returend error %d", result);
      }
          
          
      // .......................................................................
      // Actually remove the node from the linked list here
      // .......................................................................
          
      result = ExciseNode(currentNode);  
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector function ExcisENode returned with error %d", result);
		finished = false; 
        currentNode = tempNode;    
        continue;
      }
          
          
      // .......................................................................
      // Inform the index that the node has been removed from the linked list
      // .......................................................................

      indexData->_lastPass = 255;
      result = indexData->_collectGarbage(indexData);
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector called the callback which returend error %d", result);
      }
	  
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, currentNode);        
      currentNode = tempNode;    
	  
    } // end of while loop
      
	if (!finished) {
      goto CAS_LOOP;
    }
	
  } // end of CAS_LOOP
  
  LOG_TRACE("the index garbage collector has completed expunging nodes for a given index");
  
  return result;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Creates and initialises the linked list used by the garbage collector
////////////////////////////////////////////////////////////////////////////////

void InitialiseStaticLinkedList(void) {

  // ..........................................................................
  // Assign memory to the static linked list structure
  // ..........................................................................
  
  INDEX_GC_LINKED_LIST = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(linked_list_t), true);
  if (INDEX_GC_LINKED_LIST == NULL) { // out of memory?
    LOG_FATAL_AND_EXIT("Index garbage collector can not start - out of memory");
  }
  
  
  // ..........................................................................
  // 'Lock' the list so that no other process can interfere with it
  // ..........................................................................
  
  INDEX_GC_LINKED_LIST->_listFlag = INDEX_GC_LIST_FORBIDDEN_FLAG;
  INDEX_GC_LINKED_LIST->_size     = 0;
  
     
  (INDEX_GC_LINKED_LIST->_startNode)._indexData = NULL; 
  (INDEX_GC_LINKED_LIST->_startNode)._next      = &(INDEX_GC_LINKED_LIST->_endNode); 
  (INDEX_GC_LINKED_LIST->_startNode)._prev      = NULL; 
  (INDEX_GC_LINKED_LIST->_startNode)._nodeFlag  = INDEX_GC_NODE_NORMAL_FLAG; 
  
  (INDEX_GC_LINKED_LIST->_endNode)._indexData = NULL; 
  (INDEX_GC_LINKED_LIST->_endNode)._next      = NULL; 
  (INDEX_GC_LINKED_LIST->_endNode)._prev      = &(INDEX_GC_LINKED_LIST->_startNode); 
  (INDEX_GC_LINKED_LIST->_endNode)._nodeFlag  = INDEX_GC_NODE_NORMAL_FLAG; 


  // ..........................................................................
  // 'Unlock' the list so that other process can use it
  // ..........................................................................
  
  if (!TRI_CompareAndSwapIntegerUInt32 (&(INDEX_GC_LINKED_LIST->_listFlag), 
                                      INDEX_GC_LIST_FORBIDDEN_FLAG, 
                                      INDEX_GC_LIST_NORMAL_FLAG) ) {
    LOG_FATAL_AND_EXIT("Index garbage collector can not start - CAS failure");
  }                                      
  
}  


void InnerThreadLoop (bool* goToSleep) {
  TRI_index_gc_t* indexData;
  linked_list_node_t* currentNode = &(INDEX_GC_LINKED_LIST->_startNode);
  linked_list_node_t* tempNode    = NULL;
  uint64_t lastCompleteGlobalTransID = 0;
  TRI_transaction_global_stats_t* stats = NULL;
  int result;
  TRI_vocbase_t* vocbase = (TRI_vocbase_t*)(INDEX_GC_DATA); 

  stats = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_global_stats_t), true);
  if (stats == NULL) {
    LOG_TRACE("the index garbage collector inner loop failed due to lack of memory");
    *goToSleep = true;
    return;
  }
  
    
  result = TRI_GetGlobalTransactionFigures(stats);
  if (result != TRI_ERROR_NO_ERROR) {
    LOG_TRACE("the index garbage collector inner loop failed due transactions figures being unavailable");
    *goToSleep = true;
    return;
  }
  
  // todo: fill in the lastCompletedGlobalTransID from the stats returned above  
  
  currentNode = currentNode->_next;
  
  while (! *goToSleep) {
    
    // ........................................................................
    // The currentNode->_next may be null because it is the end node,
    // or it may be null because we have stepped onto a node which is being
    // excised from the list.
    // ........................................................................

    if (currentNode == NULL) {
      *goToSleep = true;
       return;
    }

      
    // ........................................................................
    // Get the next node and check that we can operate on it
    // ........................................................................

    if (!TRI_CompareIntegerUInt32 (&(currentNode->_nodeFlag), 
                                   INDEX_GC_NODE_NORMAL_FLAG) ) {
      *goToSleep = true;
      return;
    }    
      
    // ........................................................................
    // Have we reached the end of the list, if so sleep a little while
    // ........................................................................
      
    if (currentNode == &(INDEX_GC_LINKED_LIST->_endNode)) {
      *goToSleep = true;
      break;
    }

      
    // ........................................................................
    // Operate on this node. Observe that ONLY this thread can actually
    // destroy the memory associated with this node.
    // ........................................................................
      
    indexData = currentNode->_indexData;
      

    // ........................................................................
    // Check whether or not we can actually execute the call back for that
    // particular pass.      
    // ........................................................................

    /* TODO: this needs to be fixed with the transaction handling stuff
    if (stats->oldestGlobalTransID <= indexData->_transID) {
      currentNode = currentNode->_next;
      continue;
    }
      */
      
      
    // ........................................................................
    // First lets check whether we have actually finished with this node.
    // ........................................................................

    if (indexData->_lastPass < indexData->_passes) {
      ++(indexData->_lastPass);
      result = indexData->_collectGarbage(indexData);
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector called the callback which returend error %d", result);
        if (result == TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_CAS_FAILURE) { 
          // no harm done we simply try again later
          --(indexData->_lastPass);
        }        
      }
      currentNode = currentNode->_next;
    }

    
    else if (indexData->_passes == indexData->_lastPass) {
    
      // .......................................................................
      // We have finished essentially finished with the node and are about it
      // to remove the node from the linked list here. 
      // .......................................................................

      
      // .......................................................................
      // Just before we remove the data and associated data, go to the index
      // and indicate that we are about to remove the node from the linked list
      // .......................................................................
      
      indexData->_lastPass = 254;
      result = indexData->_collectGarbage(indexData);
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector called the callback which returend error %d", result);
      }
      
      
      // .......................................................................
      // Actually remove the node from the linked list here
      // .......................................................................
      
      result = ExciseNode(currentNode);  
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector function ExcisENode returned with error %d", result);
      }
      
      
      // .......................................................................
      // Inform the index that the node has been removed from the linked list
      // .......................................................................

      indexData->_lastPass = 255;
      result = indexData->_collectGarbage(indexData);
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector called the callback which returend error %d", result);
      }

      
      tempNode = currentNode->_next;
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, currentNode);
      currentNode = tempNode;
    }    
    
  } // end of while loop
  
}


void RemoveLinkedList(void) {
  linked_list_node_t* currentNode;
  
  LOG_TRACE("the index garbage collector has commenced removing all allocated memory");
  currentNode = &(INDEX_GC_LINKED_LIST->_startNode);
  
  while (currentNode != NULL) {
    int result = TRI_ERROR_NO_ERROR;
    linked_list_node_t* tempNode = currentNode->_next;     
    
    if (currentNode->_indexData != NULL) {
      currentNode->_indexData->_lastPass = 255;
      result = currentNode->_indexData->_collectGarbage(currentNode->_indexData);    
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector executed the callback and has returned error code %d",result);
      }  
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, currentNode);
    }
    
    currentNode = tempNode;    
  }
  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, INDEX_GC_LINKED_LIST);
  LOG_TRACE("the index garbage collector has completed removing all allocated memory");
}


void SetForbiddenFlag(void) {
  int counter = 0;  
  //LOG_TRACE("the index garbage collector is attempting to block insertions");
  
  while (counter < MAX_INDEX_GC_CAS_RETRIES) {
    if (TRI_CompareAndSwapIntegerUInt32 (&(INDEX_GC_LINKED_LIST->_listFlag), 
                                         INDEX_GC_LIST_NORMAL_FLAG, 
                                         INDEX_GC_LIST_FORBIDDEN_FLAG) ) {
      counter = -1;
      break;
    }
    usleep(CAS_FAILURE_SLEEP_TIME);
  }  
  
  if (counter == -1) {
    //LOG_TRACE("the index garbage collector has succeeded in blocking insertions");
  }
  else {
    LOG_TRACE("the index garbage collector has failed in blocking insertions");
  }    
}                                      

void UnsetForbiddenFlag(void) {
  int counter = 0;  
  
  //LOG_TRACE("the index garbage collector is attempting to unblock insertions");
  
  while (counter < MAX_INDEX_GC_CAS_RETRIES) {
    if (TRI_CompareAndSwapIntegerUInt32 (&(INDEX_GC_LINKED_LIST->_listFlag), 
                                         INDEX_GC_LIST_FORBIDDEN_FLAG, 
                                         INDEX_GC_LIST_NORMAL_FLAG) ) {
      counter = -1;
      break;
    }
    usleep(CAS_FAILURE_SLEEP_TIME);
  }  
  
  if (counter == -1) {
    //LOG_TRACE("the index garbage collector has succeeded in unblocking insertions");
  }
  else {
    LOG_TRACE("the index garbage collector has failed in unblocking insertions\n");
  }    
}                                      


////////////////////////////////////////////////////////////////////////////////////
// Implementation of static functions for insertion of a node
////////////////////////////////////////////////////////////////////////////////////

static int InsertNode(linked_list_node_t* insertNode) {
  int casCounter = 0;
  int bricked    = 0;
  int swaped     = 0;
  int result;  
  linked_list_node_t* nextNode;
  linked_list_node_t* prevNode;
  
  CAS_LOOP: {
    
    // ..........................................................................
    // We can not assign these pointers outside this loop, since these may change
    // any time with threads busy inserting entries into the list.
    // ..........................................................................
    
    insertNode->_next = &(INDEX_GC_LINKED_LIST->_endNode);
    nextNode = (linked_list_node_t*)(insertNode->_next);
	
    insertNode->_prev = (linked_list_node_t*)(nextNode->_prev);    
    prevNode = (linked_list_node_t*)(insertNode->_prev);  

    
    if (casCounter > 1) {
      usleep(CAS_FAILURE_SLEEP_TIME);
    }

    if (casCounter > MAX_INDEX_GC_CAS_RETRIES) {
      LOG_ERROR("max cas loop exceeded");
      return TRI_ERROR_INTERNAL;
    }
    
    bricked = InsertNodeBrick(prevNode, nextNode);
    if (bricked != 2) {
      int tempResult = InsertNodeBrickUndo(prevNode, nextNode, bricked);
      if (tempResult != TRI_ERROR_NO_ERROR) { 
        return TRI_ERROR_INTERNAL;
      }  
      ++casCounter;
      goto CAS_LOOP;
    }    
    
    swaped = InsertNodeSwapPointers(insertNode, prevNode, nextNode);
    if (swaped != 2) {
      int tempResult1 = InsertNodeBrickUndo(prevNode, nextNode, bricked);
      int tempResult2 = InsertNodeSwapPointersUndo(insertNode, prevNode, nextNode, swaped);
      if ((tempResult1 != TRI_ERROR_NO_ERROR) || (tempResult2 != TRI_ERROR_NO_ERROR)) { 
        return TRI_ERROR_INTERNAL;
      }  
      ++casCounter;
      goto CAS_LOOP;
    }
    
    result = InsertNodeBrickUndo(prevNode, nextNode, bricked);
    if (result != TRI_ERROR_NO_ERROR) { 
      return TRI_ERROR_INTERNAL;
    }
    ++INDEX_GC_LINKED_LIST->_size;
    
  } // end of CAS_LOOP
  
  return TRI_ERROR_NO_ERROR;
}

static int InsertNodeBrick(linked_list_node_t* prevNode, linked_list_node_t* nextNode) {
  bool ok;
  
  ok = TRI_CompareAndSwapIntegerUInt32 (&(prevNode->_nodeFlag), INDEX_GC_NODE_NORMAL_FLAG, INDEX_GC_NODE_BRICKED_FLAG);
  if (!ok) { return 0; }
  
  ok = TRI_CompareAndSwapIntegerUInt32 (&(nextNode->_nodeFlag), INDEX_GC_NODE_NORMAL_FLAG, INDEX_GC_NODE_BRICKED_FLAG);    
  if (!ok) { return 1; }
  
  return 2;
}

static int InsertNodeBrickUndo(linked_list_node_t* prevNode, linked_list_node_t* nextNode, int bricked) {
  bool ok;

  if (bricked > 0) {
    ok = TRI_CompareAndSwapIntegerUInt32 (&(prevNode->_nodeFlag), INDEX_GC_NODE_BRICKED_FLAG, INDEX_GC_NODE_NORMAL_FLAG);
    if (bricked > 1) {
      ok = (TRI_CompareAndSwapIntegerUInt32 (&(nextNode->_nodeFlag), INDEX_GC_NODE_BRICKED_FLAG, INDEX_GC_NODE_NORMAL_FLAG)) && (ok);    
    }
    if (!ok) {
      LOG_ERROR("InsertNodeBrickUndo failed here");
      return TRI_ERROR_INTERNAL;
    }  
  }    
  return TRI_ERROR_NO_ERROR;
}


static int InsertNodeSwapPointers(linked_list_node_t* nodeToInsert, linked_list_node_t* prevNode, linked_list_node_t* nextNode) {
  bool ok;

  ok = TRI_CompareAndSwapPointer(&(prevNode->_next), nextNode, nodeToInsert);
  if (!ok) { return 0; }

  ok = TRI_CompareAndSwapPointer(&(nextNode->_prev), prevNode, nodeToInsert);
  if (!ok) { return 1; }
  
  return 2;
}

static int InsertNodeSwapPointersUndo(linked_list_node_t* nodeToInsert, linked_list_node_t* prevNode, linked_list_node_t* nextNode, int swaped) {
  bool ok;
  
  if (swaped > 0) {
    ok = TRI_CompareAndSwapPointer(&(prevNode->_next), nodeToInsert, nextNode);
    if (swaped > 1) {
      ok = ok && TRI_CompareAndSwapPointer(&(nextNode->_prev), nodeToInsert, prevNode);
    }
    if (!ok) {
      LOG_ERROR("InsertNodeSwapPointersUndo failed here");
      return TRI_ERROR_INTERNAL;
    }  
  }    
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////////
// Implementation of static functions for removal of a node
////////////////////////////////////////////////////////////////////////////////////

static int ExciseNode(linked_list_node_t* nodeToExcise) {
  int result         = TRI_ERROR_NO_ERROR;
  int casCounter     = 0;
  int bricked        = 0;
  int swaped         = 0;
  linked_list_node_t* nextNode;
  linked_list_node_t* prevNode;
  
  
  SetForbiddenFlag();
  
  CAS_LOOP: {
  
    result   = TRI_ERROR_NO_ERROR;
    nextNode = nodeToExcise->_next;
    prevNode = nodeToExcise->_prev;
 
    if (casCounter > 1) {
      usleep(CAS_FAILURE_SLEEP_TIME);
    }

    if (casCounter > MAX_INDEX_GC_CAS_RETRIES) {
      LOG_ERROR("max cas loop exceeded");
      return TRI_ERROR_INTERNAL;
    }
    
    bricked = ExciseNodeBrick(nodeToExcise, prevNode, nextNode);
    
    if (bricked != 3) {
      result = ExciseNodeBrickUndo(nodeToExcise, prevNode, nextNode, bricked);
      if (result != TRI_ERROR_NO_ERROR) {
        return result;
      }  
      ++casCounter;
      goto CAS_LOOP;
    }    


    swaped = ExciseNodeSwapPointers(nodeToExcise, prevNode, nextNode);
    if (swaped != 2) {
      ExciseNodeBrickUndo(nodeToExcise, prevNode, nextNode, bricked);
      ExciseNodeSwapPointersUndo(nodeToExcise, prevNode, nextNode, swaped);
      ++casCounter;
      goto CAS_LOOP;
    }
    
    --INDEX_GC_LINKED_LIST->_size;
    ExciseNodeBrickUndo(nodeToExcise, prevNode, nextNode, bricked);
    
  } // end of CAS_LOOP
  
  UnsetForbiddenFlag();
    
  return result;
}

static int ExciseNodeBrick(linked_list_node_t* nodeToExcise, linked_list_node_t* prevNode, linked_list_node_t* nextNode) {
  bool ok;
  
  ok = TRI_CompareAndSwapIntegerUInt32 (&(nodeToExcise->_nodeFlag), INDEX_GC_NODE_NORMAL_FLAG, INDEX_GC_NODE_BRICKED_FLAG);
  if (!ok) { return 0; }

  ok = TRI_CompareAndSwapIntegerUInt32 (&(prevNode->_nodeFlag), INDEX_GC_NODE_NORMAL_FLAG, INDEX_GC_NODE_BRICKED_FLAG);
  if (!ok) { return 1; }
  
  ok = TRI_CompareAndSwapIntegerUInt32 (&(nextNode->_nodeFlag), INDEX_GC_NODE_NORMAL_FLAG, INDEX_GC_NODE_BRICKED_FLAG);    
  if (!ok) { return 2; }
  
  return 3;
}

static int ExciseNodeBrickUndo(linked_list_node_t* nodeToExcise, linked_list_node_t* prevNode, linked_list_node_t* nextNode, int bricked) {
  bool ok;

  if (bricked > 0) {
    ok = TRI_CompareAndSwapIntegerUInt32 (&(nodeToExcise->_nodeFlag), INDEX_GC_NODE_BRICKED_FLAG, INDEX_GC_NODE_NORMAL_FLAG);
    if (bricked > 1) {
      ok = ok && TRI_CompareAndSwapIntegerUInt32 (&(prevNode->_nodeFlag), INDEX_GC_NODE_BRICKED_FLAG, INDEX_GC_NODE_NORMAL_FLAG);
      if (bricked > 2) {
        ok = TRI_CompareAndSwapIntegerUInt32 (&(nextNode->_nodeFlag), INDEX_GC_NODE_BRICKED_FLAG, INDEX_GC_NODE_NORMAL_FLAG);    
      }
    }
    if (!ok) {
      LOG_ERROR("ExciseNodeBrickUndo failed here");
      return TRI_ERROR_INTERNAL;
    }  
  }    
  return TRI_ERROR_NO_ERROR;
}

static int ExciseNodeSwapPointers(linked_list_node_t* nodeToExcise, linked_list_node_t* prevNode, linked_list_node_t* nextNode) {
  bool ok;

  ok = TRI_CompareAndSwapPointer(&(prevNode->_next), nodeToExcise, nextNode);
  if (!ok) { return 0; }

  ok = TRI_CompareAndSwapPointer(&(nextNode->_prev), nodeToExcise, prevNode);
  if (!ok) { return 1; }
  
  return 2;
}

static int ExciseNodeSwapPointersUndo(linked_list_node_t* nodeToExcise, linked_list_node_t* prevNode, linked_list_node_t* nextNode, int swaped) {
  bool ok;
  
  if (swaped > 0) {
    ok = TRI_CompareAndSwapPointer(&(prevNode->_next), nextNode, nodeToExcise);
    if (swaped > 1) {
      ok = ok && TRI_CompareAndSwapPointer(&(nextNode->_prev), prevNode, nodeToExcise);
    }
    if (!ok) {
      LOG_ERROR("ExciseNodeSwapPointersUndo failed here");
      return TRI_ERROR_INTERNAL;
    }  
  }    
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
