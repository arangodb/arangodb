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


// @@@@@@@@ TODO: TRI_addToIOndexGC & ExciseNode 

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

#define MAX_INDEX_GC_CAS_RETRIES 100

////////////////////////////////////////////////////////////////////////////////
/// @brief the period between garbage collection tries in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const INDEX_GC_INTERVAL = (1 * 1000 * 1000);

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
} linked_list_t;

enum {
  INDEX_GC_LIST_NORMAL_FLAG,     
  INDEX_GC_LIST_FORBIDDEN_FLAG,     
  INDEX_GC_NODE_NORMAL_FLAG,     
  INDEX_GC_NODE_DELETED_FLAG,     
};  


// .............................................................................
// The linked list has to be stored somewhere. Store it here.
// .............................................................................

static linked_list_t* INDEX_GC_LINKED_LIST = NULL;
 
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
static void InitialiseStaticLinkedList (void);
static void InnerThreadLoop (bool*);
static void RemoveLinkedList (void);
static void SetForbiddenFlag (void);

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
  
  // InitialiseStaticLinkedList();
  
  
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
   
    /*
    // ........................................................................
    // The loop goes to sleep whenever we are at the end of the linked list.
    // ........................................................................
        
    goToSleep = TRI_ComparePointer(
                    &((INDEX_GC_LINKED_LIST->_startNode)._next), 
                    &(INDEX_GC_LINKED_LIST->_endNode));

    InnerThreadLoop (&goToSleep); 
    */
    
    goToSleep = true;
    
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
  
  //SetForbiddenFlag();
  
  // ..........................................................................
  // Remove all memory we assigned to any structures
  // ..........................................................................

  //RemoveLinkedList();
  
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

int TRI_AddToIndexGC(TRI_index_gc_t* indexData) {

  int result = TRI_ERROR_NO_ERROR;
  bool ok;
  
  // ...........................................................................                           
  // Check that we have something to add
  // ...........................................................................                           

  if (indexData == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................                           
  // Check that the rubbish collector is accepting rubbish.
  // Generally this means that the server has been shut down. In this case we
  // will not accept anymore rubbish.
  // ...........................................................................                           
  
  
  // ...........................................................................                           
  // The indexData structure whose memory has been allocated by the INDEX 
  // (and not this function) will also be removed  by the INDEX which called 
  // this function. When indexData._lastPass = 254, then the collectGarbage 
  // callback will be alerted to the fact that the excision of the item from the 
  // rubbish collector will be imminent. When indexData._lastPass = 255, then the
  // collectGarbage callback will be alerted that the excision has occured and
  // that any memory allocated must be deallocated.  
  // ...........................................................................                           
  
  /*
  INDEX_GC_LIST_NORMAL_FLAG
  ok = TRI_CompareAndSwapIntegerUInt32 (&(leftNN->_nbFlag), INDEX_GC_LIST_NORMAL_FLAG, INDEX_GC_LIST_NORMAL_FLAG);
*/
                             
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


int ExciseNode(linked_list_node_t* nodeToExcise) {
  int result = TRI_ERROR_NO_ERROR;
  return result;
}

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
  
  if (TRI_CompareAndSwapIntegerUInt32 (&(INDEX_GC_LINKED_LIST->_listFlag), 
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
    // First lets check whether we have actually finished with this node.
    // ........................................................................

    if (indexData->_passes == indexData->_lastPass) {
      indexData->_lastPass = 254;
      result = indexData->_collectGarbage(indexData);
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector called the callback which returend error %d", result);
      }
      result = ExciseNode(currentNode);  
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector function ExcisENode returned with error %d", result);
      }
      indexData->_lastPass = 255;
      result = indexData->_collectGarbage(indexData);
      if (result != TRI_ERROR_NO_ERROR) {
        LOG_TRACE("the index garbage collector called the callback which returend error %d", result);
      }
      tempNode = currentNode->_next;
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, currentNode);
      currentNode = tempNode;
      continue;
    }

    
    // ........................................................................
    // Check whether or not we can actually execute the call back for that
    // particular pass.      
    // ........................................................................

    if (lastCompleteGlobalTransID <= indexData->_transID) {
      currentNode = currentNode->_next;
      continue;
    }
  }
}


void RemoveLinkedList(void) {
  linked_list_node_t* currentNode;
  
  LOG_TRACE("the index garbage collector has commenced removing all allocated memory");
  currentNode = &(INDEX_GC_LINKED_LIST->_startNode);
  
  if ( currentNode->_next != NULL ) {
    currentNode = currentNode->_next;
  }
  else {
    currentNode = NULL;
  } 
  
  while (currentNode != NULL) {
    int result;
    
    if (currentNode->_indexData == NULL) {
      continue;
    }  

    currentNode->_indexData->_lastPass = 255;
    
    result = currentNode->_indexData->_collectGarbage(currentNode->_indexData);    
    if (result != TRI_ERROR_NO_ERROR) {
      LOG_TRACE("the index garbage collector executed the callback and has returned error code %d",result);
    }  
        
    if ( currentNode->_next != NULL ) {
      linked_list_node_t* tempNode = currentNode->_next;     
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, currentNode);
      currentNode = tempNode;
    }
    else {
      currentNode = NULL;
    }
  }
  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, INDEX_GC_LINKED_LIST);
  LOG_TRACE("the index garbage collector has completed removing all allocated memory");
}


void SetForbiddenFlag(void) {
  int counter = 0;  
  LOG_TRACE("the index garbage collector is attempting to block insertions");
  
   while (counter < MAX_INDEX_GC_CAS_RETRIES) {
    if (TRI_CompareAndSwapIntegerUInt32 (&(INDEX_GC_LINKED_LIST->_listFlag), 
                                         INDEX_GC_LIST_NORMAL_FLAG, 
                                         INDEX_GC_LIST_FORBIDDEN_FLAG) ) {
      counter = -1;
      break;
    }
    usleep(1000);
  }  
  
  if (counter == -1) {
    LOG_TRACE("the index garbage collector has succeeded in blocking insertions");
  }
  else {
    LOG_TRACE("the index garbage collector has failed in blocking insertions");
  }    
}                                      

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
