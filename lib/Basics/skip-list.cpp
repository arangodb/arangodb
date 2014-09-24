////////////////////////////////////////////////////////////////////////////////
/// @brief generic skip list implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/random.h"
#include "skip-list.h"

using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                         SKIP LIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief randomHeight, select a node height randomly
////////////////////////////////////////////////////////////////////////////////

static int randomHeight (void) {
  int height = 1;
  int count;
  while (true) {   // will be left by return when the right height is found
    uint32_t r = TRI_UInt32Random();
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

SkipListNode* SkipList::allocNode (int height) {
  SkipListNode* newNode = new SkipListNode();

  newNode->_doc = nullptr;

  if (0 == height) {
    newNode->_height = randomHeight();
  }
  else {
    newNode->_height = height;
  }

  try {
    newNode->_next = new SkipListNode*[newNode->_height];
  }
  catch (...) {
    delete newNode;
    throw;
  }
  for (int i = 0; i < newNode->_height; i++) {
    newNode->_next[i] = nullptr;
  }
  newNode->_prev = nullptr;

  _memoryUsed += sizeof(SkipListNode) +
                 sizeof(SkipListNode*) * newNode->_height;

  return newNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free function for a node.
////////////////////////////////////////////////////////////////////////////////

void SkipList::freeNode (SkipListNode* node) {
  // update memory usage
  _memoryUsed -= sizeof(SkipListNode) +
                 sizeof(SkipListNode*) * node->_height;
  delete[] node->_next;
  delete node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookupLess
/// The following function is the main search engine for our skiplists.
/// It is used in the insertion and removal functions. See below for
/// a tiny variation which is used in the right lookup function.
/// This function does the following:
/// The skiplist sl is searched for the largest document m that is less
/// than doc. It uses preorder comparison if cmp is SKIPLIST_CMP_PREORDER
/// and proper order comparison if cmp is SKIPLIST_CMP_TOTORDER. At the end,
/// (*pos)[0] points to the node containing m and *next points to the
/// node following (*pos)[0], or is nullptr if there is no such node. The
/// array *pos contains for each level lev in 0.._start->_height-1
/// at (*pos)[lev] the pointer to the node that contains the largest
/// document that is less than doc amongst those nodes that have height >
/// lev.
////////////////////////////////////////////////////////////////////////////////

int SkipList::lookupLess (void* doc,
                          SkipListNode* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                          SkipListNode** next,
                          SkipListCmpType cmptype) const {
  int lev;
  int cmp = 0;  // just in case to avoid undefined values
  SkipListNode* cur;

  cur = _start;
  for (lev = _start->_height-1; lev >= 0; lev--) {
    while (true) {   // will be left by break
      *next = cur->_next[lev];
      if (nullptr == *next) {
        break;
      }
      cmp = _cmp_elm_elm(_cmpdata, (*next)->_doc, doc, cmptype);
      if (cmp >= 0) {
        break;
      }
      cur = *next;
    }
    (*pos)[lev] = cur;
  }
  // Now cur == (*pos)[0] points to the largest node whose document
  // is less than doc. *next is the next node and can be nullptr if there
  // is none.
  return cmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookupLessOrEq
/// The following function is nearly as LookupScript above, but
/// finds the largest document m that is less than or equal to doc.
/// It uses preorder comparison if cmp is SKIPLIST_CMP_PREORDER
/// and proper order comparison if cmp is SKIPLIST_CMP_TOTORDER. At the end,
/// (*pos)[0] points to the node containing m and *next points to the
/// node following (*pos)[0], or is nullptr if there is no such node. The
/// array *pos contains for each level lev in 0.._start->_height-1
/// at (*pos)[lev] the pointer to the node that contains the largest
/// document that is less than or equal to doc amongst those nodes
/// that have height > lev.
////////////////////////////////////////////////////////////////////////////////

int SkipList::lookupLessOrEq (void* doc,
                              SkipListNode* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                              SkipListNode** next,
                              SkipListCmpType cmptype) const {
  int lev;
  int cmp = 0;  // just in case to avoid undefined values
  SkipListNode* cur;

  cur = _start;
  for (lev = _start->_height-1; lev >= 0; lev--) {
    while (true) {   // will be left by break
      *next = cur->_next[lev];
      if (nullptr == *next) {
        break;
      }
      cmp = _cmp_elm_elm(_cmpdata, (*next)->_doc, doc, cmptype);
      if (cmp > 0) {
        break;
      }
      cur = *next;
    }
    (*pos)[lev] = cur;
  }
  // Now cur == (*pos)[0] points to the largest node whose document
  // is less than or equal to doc. *next is the next node and can be nullptr
  // is if there none.
  return cmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookupKeyLess
/// We have two more very similar functions which look up documents if
/// only a key is given. This implies using the cmp_key_elm function
/// and using the preorder only. Otherwise, they behave identically
/// as the two previous ones.
////////////////////////////////////////////////////////////////////////////////

int SkipList::lookupKeyLess (void* key,
                             SkipListNode* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                             SkipListNode** next) const {
  int lev;
  int cmp = 0;  // just in case to avoid undefined values
  SkipListNode* cur;

  cur = _start;
  for (lev = _start->_height-1; lev >= 0; lev--) {
    while (true) {   // will be left by break
      *next = cur->_next[lev];
      if (nullptr == *next) {
        break;
      }
      cmp = _cmp_key_elm(_cmpdata, key, (*next)->_doc);
      if (cmp <= 0) {
        break;
      }
      cur = *next;
    }
    (*pos)[lev] = cur;
  }
  // Now cur == (*pos)[0] points to the largest node whose document is
  // less than key in the preorder. *next is the next node and can be
  // nullptr if there is none.
  return cmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookupKeyLessOrEq
////////////////////////////////////////////////////////////////////////////////

int SkipList::lookupKeyLessOrEq (void* key,
                                 SkipListNode* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                                 SkipListNode** next) const {
  int lev;
  int cmp = 0;  // just in case to avoid undefined values
  SkipListNode* cur;

  cur = _start;
  for (lev = _start->_height-1; lev >= 0; lev--) {
    while (true) {   // will be left by break
      *next = cur->_next[lev];
      if (nullptr == *next) {
        break;
      }
      cmp = _cmp_key_elm(_cmpdata, key, (*next)->_doc);
      if (cmp < 0) {
        break;
      }
      cur = *next;
    }
    (*pos)[lev] = cur;
  }
  // Now cur == (*pos)[0] points to the largest node whose document is
  // less than or equal to key in the preorder. *next is the next node
  // and can be nullptr is if there none.
  return cmp;
}


// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new skiplist
///
/// Returns nullptr if allocation fails and a pointer to the skiplist
/// otherwise.
////////////////////////////////////////////////////////////////////////////////

SkipList::SkipList (SkipListCmpElmElm cmp_elm_elm,
                    SkipListCmpKeyElm cmp_key_elm,
                    void* cmpdata,
                    SkipListFreeFunc freefunc,
                    bool unique) 
    : _cmp_elm_elm(cmp_elm_elm), _cmp_key_elm(cmp_key_elm), _cmpdata(cmpdata),
      _free(freefunc), _unique(unique), _nrUsed(0) {
  
  // set initial memory usage
  _memoryUsed = sizeof(SkipList);

  _start = allocNode(TRI_SKIPLIST_MAX_HEIGHT);
    // Note that this can throw
  _end = _start;

  _start->_height = 1;
  _start->_next[0] = nullptr;
  _start->_prev = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a skiplist and all its documents
////////////////////////////////////////////////////////////////////////////////

SkipList::~SkipList () {
  SkipListNode* p;
  SkipListNode* next;

  // First call free for all documents and free all nodes other than start:
  p = _start->_next[0];
  while (nullptr != p) {
    if (nullptr != _free) {
      _free(p->_doc);
    }
    next = p->_next[0];
    freeNode(p);
    p = next;
  }

  freeNode(_start);
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

int SkipList::insert (void* doc) {
  int lev;
  SkipListNode* pos[TRI_SKIPLIST_MAX_HEIGHT];
  SkipListNode* next = nullptr;  // to please the compiler
  SkipListNode* newNode;
  int cmp;

  cmp = lookupLess(doc,&pos,&next,SKIPLIST_CMP_TOTORDER);
  // Now pos[0] points to the largest node whose document is less than
  // doc. next is the next node and can be nullptr if there is none. doc is
  // in the skiplist iff next != nullptr and cmp == 0 and in this case it
  // is stored at the node next.
  if (nullptr != next && 0 == cmp) {
    // We have found a duplicate in the proper total order!
    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  // Uniqueness test if wanted:
  if (_unique) {
    if ((pos[0] != _start &&
         0 == _cmp_elm_elm(_cmpdata,doc,pos[0]->_doc,SKIPLIST_CMP_PREORDER)) ||
        (nullptr != next &&
         0 == _cmp_elm_elm(_cmpdata,doc,next->_doc,SKIPLIST_CMP_PREORDER))) {
      return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
    }
  }

  try {
    newNode = allocNode(0);
  }
  catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (newNode->_height > _start->_height) {
    // The new levels where not considered in the above search,
    // therefore pos is not set on these levels.
    for (lev = _start->_height; lev < newNode->_height; lev++) {
      pos[lev] = _start;
    }
    // Note that _start is already initialised with nullptr to the top!
    _start->_height = newNode->_height;
  }

  newNode->_doc = doc;

  // Now insert between newNode and next:
  newNode->_next[0] = pos[0]->_next[0];
  pos[0]->_next[0] = newNode;
  newNode->_prev = pos[0];
  if (newNode->_next[0] == nullptr) {
    // a new last node
    _end = newNode;
  }
  else {
    newNode->_next[0]->_prev = newNode;
  }

  // Now the element is successfully inserted, the rest is performance
  // optimisation:
  for (lev = 1; lev < newNode->_height; lev++) {
    newNode->_next[lev] = pos[lev]->_next[lev];
    pos[lev]->_next[lev] = newNode;
  }

  _nrUsed++;

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

int SkipList::remove (void* doc) {
  int lev;
  SkipListNode* pos[TRI_SKIPLIST_MAX_HEIGHT];
  SkipListNode* next = nullptr;  // to please the compiler
  int cmp;

  cmp = lookupLess(doc,&pos,&next,SKIPLIST_CMP_TOTORDER);
  // Now pos[0] points to the largest node whose document is less than
  // doc. next points to the next node and can be nullptr if there is none.
  // doc is in the skiplist iff next != nullptr and cmp == 0 and in this
  // case it is stored at the node next.

  if (nullptr == next || 0 != cmp) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  if (nullptr != _free) {
    _free(next->_doc);
  }

  // Now delete where next points to:
  for (lev = next->_height-1; lev >= 0; lev--) {
    // Note the order from top to bottom. The element remains in the
    // skiplist as long as we are at a level > 0, only some optimisations
    // in performance vanish before that. Only when we have removed it at
    // level 0, it is really gone.
    pos[lev]->_next[lev] = next->_next[lev];
  }
  if (next->_next[0] == nullptr) {
    // We were the last, so adjust _end
    _end = next->_prev;
  }
  else {
    next->_next[0]->_prev = next->_prev;
  }

  freeNode(next);

  _nrUsed--;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up doc in the skiplist using the proper order
/// comparison.
///
/// Only comparisons using the proper order are done. Returns nullptr
/// if doc is not in the skiplist.
////////////////////////////////////////////////////////////////////////////////

SkipListNode* SkipList::lookup (void* doc) const {
  SkipListNode* pos[TRI_SKIPLIST_MAX_HEIGHT];
  SkipListNode* next = nullptr; // to please the compiler
  int cmp;

  cmp = lookupLess(doc,&pos,&next,SKIPLIST_CMP_TOTORDER);
  // Now pos[0] points to the largest node whose document is less than
  // doc. next points to the next node and can be nullptr if there is none.
  // doc is in the skiplist iff next != nullptr and cmp == 0 and in this
  // case it is stored at the node next.
  if (nullptr == next || 0 != cmp) {
    return nullptr;
  }
  return next;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less to doc in the preorder
/// comparison or the start node if none is.
///
/// Only comparisons using the preorder are done.
////////////////////////////////////////////////////////////////////////////////

SkipListNode* SkipList::leftLookup (void* doc) const {
  SkipListNode* pos[TRI_SKIPLIST_MAX_HEIGHT];
  SkipListNode* next;

  lookupLess(doc,&pos,&next,SKIPLIST_CMP_PREORDER);
  // Now pos[0] points to the largest node whose document is less than
  // doc in the preorder. next points to the next node and can be nullptr
  // if there is none. doc is in the skiplist iff next != nullptr and cmp
  // == 0 and in this case it is stored at the node next.
  return pos[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less or equal to doc in
/// the preorder comparison or the start node if none is.
///
/// Only comparisons using the preorder are done.
////////////////////////////////////////////////////////////////////////////////

SkipListNode* SkipList::rightLookup (void* doc) const {
  SkipListNode* pos[TRI_SKIPLIST_MAX_HEIGHT];
  SkipListNode* next;

  lookupLessOrEq(doc,&pos,&next,SKIPLIST_CMP_PREORDER);
  // Now pos[0] points to the largest node whose document is less than
  // or equal to doc in the preorder. next points to the next node and
  // can be nullptr if there is none. doc is in the skiplist iff next !=
  // nullptr and cmp == 0 and in this case it is stored at the node next.
  return pos[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document whose key is less to key in the preorder
/// comparison or the start node if none is.
///
/// Only comparisons using the preorder are done using cmp_key_elm.
////////////////////////////////////////////////////////////////////////////////

SkipListNode* SkipList::leftKeyLookup (void* key) const {
  SkipListNode* pos[TRI_SKIPLIST_MAX_HEIGHT];
  SkipListNode* next;

  lookupKeyLess(key,&pos,&next);
  // Now pos[0] points to the largest node whose document is less than
  // key in the preorder. next points to the next node and can be nullptr
  // if there is none. doc is in the skiplist iff next != nullptr and cmp
  // == 0 and in this case it is stored at the node next.
  return pos[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less or equal to doc in
/// the preorder comparison or the start node if none is.
///
/// Only comparisons using the preorder are done using cmp_key_elm.
////////////////////////////////////////////////////////////////////////////////

SkipListNode* SkipList::rightKeyLookup (void* key) const {
  SkipListNode* pos[TRI_SKIPLIST_MAX_HEIGHT];
  SkipListNode* next;

  lookupKeyLessOrEq(key,&pos,&next);
  // Now pos[0] points to the largest node whose document is less than
  // or equal to key in the preorder. next points to the next node and
  // can be nullptr if there is none. doc is in the skiplist iff next !=
  // nullptr and cmp == 0 and in this case it is stored at the node next.
  return pos[0];
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
