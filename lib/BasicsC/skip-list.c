////////////////////////////////////////////////////////////////////////////////
/// @brief generic skip list implementation
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
/// @author Max Neunhoeffer
/// @author Copyright 2013-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/random.h"
#include "skip-list.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                         SKIP LIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Select a node height randomly
////////////////////////////////////////////////////////////////////////////////

static int TRI_random_height (void)
{
  uint32_t r;
  int height = 1;
  int count;
  while (true) {   // will be left by return when the right height is found
    r = TRI_UInt32Random();
    for (count = 32; count > 0; count--) {
        if (0 != (r & 1UL) || height == TRI_SKIPLIST_MAX_HEIGHT) return height;
        r = r >> 1;
        height++;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Allocation function for a node. If height is 0, then a
/// random height is taken.
////////////////////////////////////////////////////////////////////////////////

static TRI_skiplist_node_t* TRI_SkipListAllocNode (TRI_skiplist_t* sl, 
                                                   int height) {
  TRI_skiplist_node_t* new;
  new = (TRI_skiplist_node_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                                            sizeof(TRI_skiplist_node_t),
                                            false);
  if (NULL == new) return new;

  new->doc = NULL;

  if (0 == height) {
    new->height = TRI_random_height();
  }
  else {
    new->height = height;
  }

  new->next = (TRI_skiplist_node_t**) 
              TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                           sizeof(TRI_skiplist_node_t*)*new->height,
                           true);
  if (NULL == new->next) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE,new);
    return NULL;
  }
  return new;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Free function for a node.
////////////////////////////////////////////////////////////////////////////////

static void TRI_SkipListFreeNode (TRI_skiplist_node_t* node) {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE,node->next);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE,node);
}

//
// The following function is the main search engine for our skiplists.
// It is used in the insertion and removal functions. See below for
// a tiny variation which is used in the right lookup function.
// This function does the following:
// The skiplist sl is searched for the largest document m that is less
// than doc. It uses preorder comparison if cmp is TRI_CMP_PREORDER
// and proper order comparison if cmp is TRI_CMP_TOTORDER. At the end,
// (*pos)[0] points to the node containing m and *next points to the
// node following (*pos)[0], or is NULL if there is no such node. The
// array *pos contains for each level lev in 0..sl->start->height-1
// at (*pos)[lev] the pointer to the node that contains the largest
// document that is less than doc amongst those nodes that have height >
// lev.
// 

static int LookupLess (TRI_skiplist_t *sl, 
                       void *doc,
                       TRI_skiplist_node_t* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                       TRI_skiplist_node_t** next,
                       TRI_cmp_type_e cmptype) {
  int lev;
  int cmp = 0;  // just in case to avoid undefined values
  TRI_skiplist_node_t *cur;

  cur = sl->start;
  for (lev = sl->start->height-1; lev >= 0; lev--) {
    while (true) {   // will be left by break
      *next = cur->next[lev];
      if (NULL == *next) {
        break;
      }
      cmp = sl->cmp_elm_elm(sl->cmpdata,(*next)->doc,doc,cmptype);
      if (cmp >= 0) {
        break;
      }
      cur = *next;
    }
    (*pos)[lev] = cur;
  }
  // Now cur == (*pos)[0] points to the largest node whose document 
  // is less than doc. *next is the next node and can be NULL if there 
  // is none.
  return cmp;
}

//
// The following function is nearly as LookupScript above, but
// finds the largest document m that is less than or equal to doc.
// It uses preorder comparison if cmp is TRI_CMP_PREORDER
// and proper order comparison if cmp is TRI_CMP_TOTORDER. At the end,
// (*pos)[0] points to the node containing m and *next points to the
// node following (*pos)[0], or is NULL if there is no such node. The
// array *pos contains for each level lev in 0..sl->start->height-1
// at (*pos)[lev] the pointer to the node that contains the largest
// document that is less than or equal to doc amongst those nodes 
// that have height > lev.
// 

static int LookupLessOrEq (TRI_skiplist_t *sl, 
                           void *doc,
                           TRI_skiplist_node_t* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                           TRI_skiplist_node_t** next,
                           TRI_cmp_type_e cmptype) {
  int lev;
  int cmp = 0;  // just in case to avoid undefined values
  TRI_skiplist_node_t *cur;

  cur = sl->start;
  for (lev = sl->start->height-1; lev >= 0; lev--) {
    while (true) {   // will be left by break
      *next = cur->next[lev];
      if (NULL == *next) {
        break;
      }
      cmp = sl->cmp_elm_elm(sl->cmpdata,(*next)->doc,doc,cmptype);
      if (cmp > 0) {
        break;
      }
      cur = *next;
    }
    (*pos)[lev] = cur;
  }
  // Now cur == (*pos)[0] points to the largest node whose document 
  // is less than or equal to doc. *next is the next node and can be NULL
  // is if there none.
  return cmp;
}

//
// We have two more very similar functions which look up documents if
// only a key is given. This implies using the cmp_key_elm function
// and using the preorder only. Otherwise, they behave identically
// as the two previous ones.
//

static int LookupKeyLess (TRI_skiplist_t *sl, 
                          void *key,
                          TRI_skiplist_node_t* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                          TRI_skiplist_node_t** next) {
  int lev;
  int cmp = 0;  // just in case to avoid undefined values
  TRI_skiplist_node_t *cur;

  cur = sl->start;
  for (lev = sl->start->height-1; lev >= 0; lev--) {
    while (true) {   // will be left by break
      *next = cur->next[lev];
      if (NULL == *next) {
        break;
      }
      cmp = sl->cmp_key_elm(sl->cmpdata,key,(*next)->doc);
      if (cmp <= 0) {
        break;
      }
      cur = *next;
    }
    (*pos)[lev] = cur;
  }
  // Now cur == (*pos)[0] points to the largest node whose document is
  // less than key in the preorder. *next is the next node and can be
  // NULL if there is none.
  return cmp;
}

static int LookupKeyLessOrEq (TRI_skiplist_t *sl, 
                        void *key,
                        TRI_skiplist_node_t* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                        TRI_skiplist_node_t** next) {
  int lev;
  int cmp = 0;  // just in case to avoid undefined values
  TRI_skiplist_node_t *cur;

  cur = sl->start;
  for (lev = sl->start->height-1; lev >= 0; lev--) {
    while (true) {   // will be left by break
      *next = cur->next[lev];
      if (NULL == *next) {
        break;
      }
      cmp = sl->cmp_key_elm(sl->cmpdata,key,(*next)->doc);
      if (cmp < 0) {
        break;
      }
      cur = *next;
    }
    (*pos)[lev] = cur;
  }
  // Now cur == (*pos)[0] points to the largest node whose document is
  // less than or equal to key in the preorder. *next is the next node
  // and can be NULL is if there none.
  return cmp;
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
/// @brief creates a new skiplist
///
/// Returns NULL if allocation fails and a pointer to the skiplist
/// otherwise.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_t* TRI_InitSkipList (TRI_skiplist_cmp_elm_elm_t cmp_elm_elm,
                                  TRI_skiplist_cmp_key_elm_t cmp_key_elm,
                                  void *cmpdata,
                                  TRI_skiplist_free_func_t freefunc,
                                  bool unique) {
  TRI_skiplist_t* sl;

  sl = (TRI_skiplist_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                                      sizeof(TRI_skiplist_t),false);
  if (NULL == sl) {
    return NULL;
  }

  sl->start = TRI_SkipListAllocNode(sl,TRI_SKIPLIST_MAX_HEIGHT);
  if (NULL == sl->start) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE,sl);
    return NULL;
  }
  sl->start->height = 1;
  sl->start->next[0] = NULL;

  sl->cmp_elm_elm = cmp_elm_elm;
  sl->cmp_key_elm = cmp_key_elm;
  sl->cmpdata = cmpdata;
  sl->free = freefunc;
  sl->unique = unique;
  sl->nrUsed = 0;

  return sl;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief frees a skiplist and all its documents
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipList (TRI_skiplist_t* sl) {
  TRI_skiplist_node_t* p;
  TRI_skiplist_node_t* next;

  // First call free for all documents and free all nodes other than start:
  p = sl->start->next[0];
  while (NULL != p) {
    if (NULL != sl->free) {
      sl->free(p->doc);
    }
    next = p->next[0];
    TRI_SkipListFreeNode(p);
    p = next;
  }
  TRI_SkipListFreeNode(sl->start);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE,sl);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the start node
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListStartNode (TRI_skiplist_t* sl) {
  return sl->start;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the successor node or NULL if last node
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListNextNode (TRI_skiplist_node_t* node) {
  return node->next[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document into a skiplist
///
/// Comparison is done using proper order comparison. If the skiplist
/// is unique then no two documents that compare equal in the
/// preorder can be inserted. Returns TRI_ERROR_NO_ERROR if all
/// is well, TRI_ERROR_OUT_OF_MEMORY if allocation failed and
/// TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED if the unique constraint
/// would have been violated by the insert or if there is already a
/// document in the skip list that compares equal to doc in the proper
/// total order. In the latter two cases nothing is inserted.
////////////////////////////////////////////////////////////////////////////////

int TRI_SkipListInsert (TRI_skiplist_t *sl, void *doc) {
  int lev;
  TRI_skiplist_node_t* pos[TRI_SKIPLIST_MAX_HEIGHT];
  TRI_skiplist_node_t* next = NULL;  // to please the compiler
  TRI_skiplist_node_t* new;
  int cmp;

  cmp = LookupLess(sl,doc,&pos,&next,TRI_CMP_TOTORDER);
  // Now pos[0] points to the largest node whose document is less than
  // doc. next is the next node and can be NULL if there is none. doc is
  // in the skiplist iff next != NULL and cmp == 0 and in this case it
  // is stored at the node next.
  if (NULL != next && 0 == cmp) {
    // We have found a duplicate in the proper total order!
    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }
  
  // Uniqueness test if wanted:
  if (sl->unique) {
    if ((pos[0] != sl->start && 
         0 == sl->cmp_elm_elm(sl->cmpdata,doc,pos[0]->doc,TRI_CMP_PREORDER)) ||
        (NULL != next && 
         0 == sl->cmp_elm_elm(sl->cmpdata,doc,next->doc,TRI_CMP_PREORDER))) {
      return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
    }
  }

  new = TRI_SkipListAllocNode(sl,0);
  if (NULL == new) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (new->height > sl->start->height) {
    // The new levels where not considered in the above search,
    // therefore pos is not set on these levels.
    for (lev = sl->start->height; lev < new->height; lev++) {
      pos[lev] = sl->start;
    }
    // Note that sl->start is already initialised with NULL to the top!
    sl->start->height = new->height;
  }

  new->doc = doc;

  // Now insert between new and next:
  for (lev = 0; lev < new->height; lev++) {
    // Note the order from bottom to top. The element is inserted as soon
    // as it is inserted at level 0, the rest is performance optimisation.
    new->next[lev] = pos[lev]->next[lev];
    pos[lev]->next[lev] = new;
  }

  sl->nrUsed++;

  return TRI_ERROR_NO_ERROR;
}
 

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist
///
/// Comparison is done using proper order comparison.
/// Returns TRI_ERROR_NO_ERROR if all is well and
/// TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND if the document was not found.
/// In the latter two cases nothing is removed.
////////////////////////////////////////////////////////////////////////////////

int TRI_SkipListRemove (TRI_skiplist_t *sl, void *doc) {
  int lev;
  TRI_skiplist_node_t* pos[TRI_SKIPLIST_MAX_HEIGHT];
  TRI_skiplist_node_t* next = NULL;  // to please the compiler
  int cmp;

  cmp = LookupLess(sl,doc,&pos,&next,TRI_CMP_TOTORDER);
  // Now pos[0] points to the largest node whose document is less than
  // doc. next points to the next node and can be NULL if there is none.
  // doc is in the skiplist iff next != NULL and cmp == 0 and in this
  // case it is stored at the node next.
  
  if (NULL == next || 0 != cmp) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  if (NULL != sl->free) {
    sl->free(next->doc);
  }

  // Now delete where next points to:
  for (lev = next->height-1; lev >= 0; lev--) {
      // Note the order from top to bottom. The element remains in the
      // skiplist as long as we are at a level > 0, only some optimisations
      // in performance vanish before that. Only when we have removed it at 
      // level 0, it is really gone.
      pos[lev]->next[lev] = next->next[lev];
  }

  TRI_SkipListFreeNode(next);

  sl->nrUsed--;

  return TRI_ERROR_NO_ERROR;
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of entries in the skiplist.
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_SkipListGetNrUsed (TRI_skiplist_t *sl) {
  return sl->nrUsed;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief looks up doc in the skiplist using the proper order
/// comparison. 
///
/// Only comparisons using the proper order are done. Returns NULL
/// if doc is not in the skiplist.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListLookup (TRI_skiplist_t *sl, void *doc) {
  TRI_skiplist_node_t* pos[TRI_SKIPLIST_MAX_HEIGHT];
  TRI_skiplist_node_t* next = NULL; // to please the compiler
  int cmp;

  cmp = LookupLess(sl,doc,&pos,&next,TRI_CMP_TOTORDER);
  // Now pos[0] points to the largest node whose document is less than
  // doc. next points to the next node and can be NULL if there is none.
  // doc is in the skiplist iff next != NULL and cmp == 0 and in this
  // case it is stored at the node next.
  if (NULL == next || 0 != cmp) {
    return NULL;
  }
  return next;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less to doc in the preorder
/// comparison or the start node if none is.
///
/// Only comparisons using the preorder are done.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListLeftLookup (TRI_skiplist_t *sl, void *doc) {
  TRI_skiplist_node_t* pos[TRI_SKIPLIST_MAX_HEIGHT];
  TRI_skiplist_node_t* next;

  LookupLess(sl,doc,&pos,&next,TRI_CMP_PREORDER);
  // Now pos[0] points to the largest node whose document is less than
  // doc in the preorder. next points to the next node and can be NULL
  // if there is none. doc is in the skiplist iff next != NULL and cmp
  // == 0 and in this case it is stored at the node next.
  return pos[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less or equal to doc in
/// the preorder comparison or the start node if none is.
///
/// Only comparisons using the preorder are done.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListRightLookup (TRI_skiplist_t *sl, void *doc) {
  TRI_skiplist_node_t* pos[TRI_SKIPLIST_MAX_HEIGHT];
  TRI_skiplist_node_t* next;

  LookupLessOrEq(sl,doc,&pos,&next,TRI_CMP_PREORDER);
  // Now pos[0] points to the largest node whose document is less than
  // or equal to doc in the preorder. next points to the next node and
  // can be NULL if there is none. doc is in the skiplist iff next !=
  // NULL and cmp == 0 and in this case it is stored at the node next.
  return pos[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document whose key is less to key in the preorder
/// comparison or the start node if none is.
///
/// Only comparisons using the preorder are done using cmp_key_elm.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListLeftKeyLookup (TRI_skiplist_t *sl, void *key) {
  TRI_skiplist_node_t* pos[TRI_SKIPLIST_MAX_HEIGHT];
  TRI_skiplist_node_t* next;

  LookupKeyLess(sl,key,&pos,&next);
  // Now pos[0] points to the largest node whose document is less than
  // key in the preorder. next points to the next node and can be NULL
  // if there is none. doc is in the skiplist iff next != NULL and cmp
  // == 0 and in this case it is stored at the node next.
  return pos[0];

}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less or equal to doc in
/// the preorder comparison or the start node if none is.
///
/// Only comparisons using the preorder are done using cmp_key_elm.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListRightKeyLookup (TRI_skiplist_t *sl, 
                                                 void *key) {
  TRI_skiplist_node_t* pos[TRI_SKIPLIST_MAX_HEIGHT];
  TRI_skiplist_node_t* next;

  LookupKeyLessOrEq(sl,key,&pos,&next);
  // Now pos[0] points to the largest node whose document is less than
  // or equal to key in the preorder. next points to the next node and
  // can be NULL if there is none. doc is in the skiplist iff next !=
  // NULL and cmp == 0 and in this case it is stored at the node next.
  return pos[0];
}


