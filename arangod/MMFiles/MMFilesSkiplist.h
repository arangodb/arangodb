////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_SKIP_LIST_H
#define ARANGOD_MMFILES_SKIP_LIST_H 1

#include "Basics/Common.h"
#include "Random/RandomGenerator.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

// We will probably never see more than 2^48 documents in a skip list
#define TRI_SKIPLIST_MAX_HEIGHT 48

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a skiplist node
////////////////////////////////////////////////////////////////////////////////

template <class Key, class Element>
class MMFilesSkiplist;

template <class Key, class Element>
class MMFilesSkiplistNode {
  friend class MMFilesSkiplist<Key, Element>;
  MMFilesSkiplistNode<Key, Element>** _next;
  MMFilesSkiplistNode<Key, Element>* _prev;
  Element* _doc;
  int _height;

 public:
  MMFilesSkiplistNode<Key, Element>(int height, char* ptr)
      : _next(reinterpret_cast<MMFilesSkiplistNode<Key, Element>**>(
            ptr + sizeof(MMFilesSkiplistNode<Key, Element>))),
        _prev(nullptr),
        _doc(nullptr),
        _height(height) {
    for (int i = 0; i < _height; i++) {
      _next[i] = nullptr;
    }
  }

  Element* document() const { return _doc; }

  MMFilesSkiplistNode<Key, Element>* nextNode() const {
    if (_height == 0) {
      // _next[0] is uninitialized
      return nullptr;
    }
    return _next[0];
  }

  // Note that the prevNode of the first data node is the artificial
  // _start node not containing data. This is contrary to the prevNode
  // method of the MMFilesSkiplist class, which returns nullptr in that case.
  MMFilesSkiplistNode<Key, Element>* prevNode() const { return _prev; }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief two possibilities for comparison, see below
////////////////////////////////////////////////////////////////////////////////

enum MMFilesSkiplistCmpType { SKIPLIST_CMP_PREORDER, SKIPLIST_CMP_TOTORDER };

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a skiplist
/// _end always points to the last node in the skiplist, this can be the
/// same as the _start node. If a node does not have a successor on a certain
/// level, then the corresponding _next pointer is a nullptr.
////////////////////////////////////////////////////////////////////////////////

template <class Key, class Element>
class MMFilesSkiplist {
  typedef MMFilesSkiplistNode<Key, Element> Node;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief type of a function pointer to a comparison function for a skiplist.
  ///
  /// The last argument is called preorder. If true then the comparison
  /// function must implement a preorder (reflexive and transitive).
  /// If preorder is false, then a proper total order must be
  /// implemented by the comparison function. The proper total order
  /// must refine the preorder in the sense that a < b in the proper order
  /// implies a <= b in the preorder.
  /// The first argument is a data pointer which may contain arbitrary
  /// infrastructure needed for the comparison. See cmpdata field in the
  /// skiplist type.
  /// The cmp_key_elm variant compares a key with an element using the preorder.
  /// The first argument is a data pointer as above, the second is a pointer
  /// to the key and the third is a pointer to an element.
  //////////////////////////////////////////////////////////////////////////////

  typedef std::function<int(void*, Element const*, Element const*, MMFilesSkiplistCmpType)>
      CmpElmElmFuncType;
  typedef std::function<int(void*, Key const*, Element const*)> CmpKeyElmFuncType;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Type of a pointer to a function that is called whenever a
  /// document is removed from a skiplist.
  //////////////////////////////////////////////////////////////////////////////

  typedef std::function<void(Element*)> FreeElementFuncType;

 private:
  Node* _start;
  Node* _end;
  CmpElmElmFuncType _cmp_elm_elm;
  CmpKeyElmFuncType _cmp_key_elm;
  FreeElementFuncType _free;
  bool _unique;  // indicates whether multiple entries that
                 // are equal in the preorder are allowed in
  uint64_t _nrUsed;
  bool _isArray;  // indicates whether this index is used to
                  // index arrays.
  size_t _memoryUsed;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new skiplist
  ///
  /// Returns nullptr if allocation fails and a pointer to the skiplist
  /// otherwise.
  //////////////////////////////////////////////////////////////////////////////

  MMFilesSkiplist(CmpElmElmFuncType cmp_elm_elm, CmpKeyElmFuncType cmp_key_elm,
           FreeElementFuncType freefunc, bool unique, bool isArray)
      : _cmp_elm_elm(cmp_elm_elm),
        _cmp_key_elm(cmp_key_elm),
        _free(freefunc),
        _unique(unique),
        _nrUsed(0),
        _isArray(isArray) {
    // Set the initial memory
    _memoryUsed = sizeof(MMFilesSkiplist);

    _start = allocNode(TRI_SKIPLIST_MAX_HEIGHT);
    // Note that this can throw
    _end = _start;

    _start->_height = 1;
    _start->_next[0] = nullptr;
    _start->_prev = nullptr;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief frees a skiplist and all its documents
  //////////////////////////////////////////////////////////////////////////////

  ~MMFilesSkiplist() {
    try {
      truncate(false);
    } catch (...) {
    }
  }

  void truncate(bool createStartNode) {
    Node* p;
    Node* next;

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
    
    _memoryUsed = sizeof(MMFilesSkiplist);
    _nrUsed = 0;

    if (createStartNode) {
      _start = allocNode(TRI_SKIPLIST_MAX_HEIGHT);
      // Note that this can throw
      _end = _start;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the start node, note that this does not return the first
  /// data node but the (internal) artificial node stored under _start. This
  /// is consistent behavior with the leftLookup method given a key value
  /// of -infinity.
  //////////////////////////////////////////////////////////////////////////////

  Node* startNode() const { return _start; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the end node, note that for formal reasons this always
  /// returns a nullptr, which stands for the first value outside, in analogy
  /// to startNode(). One has to use prevNode(nullptr) to get the last node
  /// containing data.
  //////////////////////////////////////////////////////////////////////////////

  Node* endNode() const { return nullptr; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the successor node or nullptr if last node
  //////////////////////////////////////////////////////////////////////////////

  Node* nextNode(Node* node) const { return node->_next[0]; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the predecessor node or _startNode() if first node,
  /// it is legal to call this with the nullptr to find the last node
  /// containing data, if there is one.
  //////////////////////////////////////////////////////////////////////////////

  Node* prevNode(Node* node) const {
    return nullptr == node ? _end : node->_prev;
  }

  //////////////////////////////////////////////////////////////////////////////
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
  //////////////////////////////////////////////////////////////////////////////

  int insert(void* userData, Element* doc) {
    int lev;
    Node* pos[TRI_SKIPLIST_MAX_HEIGHT];
    Node* next = nullptr;  // to please the compiler
    Node* newNode;
    int cmp;

    cmp = lookupLess(userData, doc, &pos, &next, SKIPLIST_CMP_TOTORDER);
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
           0 == _cmp_elm_elm(userData, doc, pos[0]->_doc, SKIPLIST_CMP_PREORDER)) ||
          (nullptr != next &&
           0 == _cmp_elm_elm(userData, doc, next->_doc, SKIPLIST_CMP_PREORDER))) {
        return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
      }
    }

    try {
      newNode = allocNode(0);
    } catch (...) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    if (newNode->_height > _start->_height) {
      // The new levels where not considered in the above search,
      // therefore pos is not set on these levels.
      for (lev = _start->_height; lev < newNode->_height; lev++) {
        pos[lev] = _start;
      }
      // Note that _start is already initialized with nullptr to the top!
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
    } else {
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes a document from a skiplist
  ///
  /// Comparison is done using proper order comparison.
  /// Returns TRI_ERROR_NO_ERROR if all is well and
  /// TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND if the document was not found.
  /// In the latter two cases nothing is removed.
  //////////////////////////////////////////////////////////////////////////////

  int remove(void* userData, Element* doc) {
    int lev;
    Node* pos[TRI_SKIPLIST_MAX_HEIGHT];
    Node* next = nullptr;  // to please the compiler
    int cmp;

    cmp = lookupLess(userData, doc, &pos, &next, SKIPLIST_CMP_TOTORDER);
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
    for (lev = next->_height - 1; lev >= 0; lev--) {
      // Note the order from top to bottom. The element remains in the
      // skiplist as long as we are at a level > 0, only some optimisations
      // in performance vanish before that. Only when we have removed it at
      // level 0, it is really gone.
      pos[lev]->_next[lev] = next->_next[lev];
    }
    if (next->_next[0] == nullptr) {
      // We were the last, so adjust _end
      _end = next->_prev;
    } else {
      next->_next[0]->_prev = next->_prev;
    }

    freeNode(next);

    _nrUsed--;

    return TRI_ERROR_NO_ERROR;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the number of entries in the skiplist.
  //////////////////////////////////////////////////////////////////////////////

  uint64_t getNrUsed() const { return _nrUsed; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the memory used by the index
  //////////////////////////////////////////////////////////////////////////////

  size_t memoryUsage() const { return _memoryUsed; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns if this indexed is used for arrays
  //////////////////////////////////////////////////////////////////////////////

  bool isArray() const { return _isArray; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Appends information about statistics in the given VelocyPack.
  //////////////////////////////////////////////////////////////////////////////

  void appendToVelocyPack(VPackBuilder& builder) {
    builder.add("nrUsed", VPackValue(_nrUsed));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up doc in the skiplist using the proper order
  /// comparison.
  ///
  /// Only comparisons using the proper order are done using cmp_elm_elm.
  /// Returns nullptr if doc is not in the skiplist.
  //////////////////////////////////////////////////////////////////////////////

  Node* lookup(void* userData, Element const* doc) const {
    Node* pos[TRI_SKIPLIST_MAX_HEIGHT];
    Node* next = nullptr;  // to please the compiler
    int cmp;

    cmp = lookupLess(userData, doc, &pos, &next, SKIPLIST_CMP_TOTORDER);
    // Now pos[0] points to the largest node whose document is less than
    // doc. next points to the next node and can be nullptr if there is none.
    // doc is in the skiplist iff next != nullptr and cmp == 0 and in this
    // case it is stored at the node next.
    if (nullptr == next || 0 != cmp) {
      return nullptr;
    }
    return next;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds the last document that is less to doc in the preorder
  /// comparison or the start node if none is.
  ///
  /// Only comparisons using the preorder are done using cmp_elm_elm.
  //////////////////////////////////////////////////////////////////////////////

  Node* leftLookup(void* userData, Element const* doc) const {
    Node* pos[TRI_SKIPLIST_MAX_HEIGHT];
    Node* next;

    pos[0] = nullptr;  // initialize to satisfy scan-build
    lookupLess(userData, doc, &pos, &next, SKIPLIST_CMP_PREORDER);
    // Now pos[0] points to the largest node whose document is less than
    // doc in the preorder. next points to the next node and can be nullptr
    // if there is none. doc is in the skiplist iff next != nullptr and cmp
    // == 0 and in this case it is stored at the node next.
    return pos[0];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds the last document that is less or equal to doc in
  /// the preorder comparison or the start node if none is.
  ///
  /// Only comparisons using the preorder are done using cmp_elm_elm.
  //////////////////////////////////////////////////////////////////////////////

  Node* rightLookup(void* userData, Element const* doc) const {
    Node* pos[TRI_SKIPLIST_MAX_HEIGHT];
    Node* next;

    pos[0] = nullptr;  // initialize to satisfy scan-build
    lookupLessOrEq(userData, doc, &pos, &next, SKIPLIST_CMP_PREORDER);
    // Now pos[0] points to the largest node whose document is less than
    // or equal to doc in the preorder. next points to the next node and
    // can be nullptr if there is none. doc is in the skiplist iff next !=
    // nullptr and cmp == 0 and in this case it is stored at the node next.
    return pos[0];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds the last document whose key is less to key in the preorder
  /// comparison or the start node if none is.
  ///
  /// Only comparisons using the preorder are done using cmp_key_elm.
  //////////////////////////////////////////////////////////////////////////////

  Node* leftKeyLookup(void* userData, Key const* key) const {
    Node* pos[TRI_SKIPLIST_MAX_HEIGHT];
    Node* next;

    pos[0] = nullptr;  // initialize to satisfy scan-build
    lookupKeyLess(userData, key, &pos, &next);
    // Now pos[0] points to the largest node whose document is less than
    // key in the preorder. next points to the next node and can be nullptr
    // if there is none. doc is in the skiplist iff next != nullptr and cmp
    // == 0 and in this case it is stored at the node next.
    return pos[0];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds the last document that is less or equal to doc in
  /// the preorder comparison or the start node if none is.
  ///
  /// Only comparisons using the preorder are done using cmp_key_elm.
  //////////////////////////////////////////////////////////////////////////////

  Node* rightKeyLookup(void* userData, Key const* key) const {
    Node* pos[TRI_SKIPLIST_MAX_HEIGHT];
    Node* next;

    pos[0] = nullptr;  // initialize to satisfy scan-build
    lookupKeyLessOrEq(userData, key, &pos, &next);
    // Now pos[0] points to the largest node whose document is less than
    // or equal to key in the preorder. next points to the next node and
    // can be nullptr if there is none. doc is in the skiplist iff next !=
    // nullptr and cmp == 0 and in this case it is stored at the node next.
    return pos[0];
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief allocate a new MMFilesSkiplistNode of a certain height. If height is 0,
  /// then a random height is taken.
  //////////////////////////////////////////////////////////////////////////////

  Node* allocNode(int height) {
    if (0 == height) {
      height = RandomHeight();
    }

    // allocate enough memory for skiplist node plus all the next nodes in one
    // go
    void* ptr = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                             sizeof(Node) + sizeof(Node*) * height);

    if (ptr == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    Node* newNode;

    try {
      // use placement new
      newNode = new (ptr) Node(height, static_cast<char*>(ptr));
    } catch (...) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ptr);
      throw;
    }

    _memoryUsed += sizeof(Node) + sizeof(Node*) * newNode->_height;

    return newNode;
  }

  //////////////////////////////////////////////////////////////////////////////
  ///// @brief Free function for a node.
  //////////////////////////////////////////////////////////////////////////////

  void freeNode(Node* node) {
    // update memory usage
    _memoryUsed -= sizeof(Node) + sizeof(Node*) * node->_height;

    // we have used placement new to construct the skiplist node,
    // so now we have to manually call its dtor and free the underlying memory
    node->~Node();
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, node);
  }

  //////////////////////////////////////////////////////////////////////////////
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
  /// array *pos contains for each level lev in 0..sl->start->height-1
  /// at (*pos)[lev] the pointer to the node that contains the largest
  /// document that is less than doc amongst those nodes that have height >
  /// lev.
  //////////////////////////////////////////////////////////////////////////////

  int lookupLess(void* userData,
                 Element const* doc, Node* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                 Node** next, MMFilesSkiplistCmpType cmptype) const {
    int lev;
    int cmp = 0;  // just in case to avoid undefined values

    Node* cur = _start;
    for (lev = _start->_height - 1; lev >= 0; lev--) {
      while (true) {  // will be left by break
        *next = cur->_next[lev];
        if (nullptr == *next) {
          break;
        }
        cmp = _cmp_elm_elm(userData, (*next)->_doc, doc, cmptype);
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

  //////////////////////////////////////////////////////////////////////////////
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
  //////////////////////////////////////////////////////////////////////////////

  int lookupLessOrEq(void* userData,
                     Element const* doc, Node* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                     Node** next, MMFilesSkiplistCmpType cmptype) const {
    int lev;
    int cmp = 0;  // just in case to avoid undefined values

    Node* cur = _start;
    for (lev = _start->_height - 1; lev >= 0; lev--) {
      while (true) {  // will be left by break
        *next = cur->_next[lev];
        if (nullptr == *next) {
          break;
        }
        cmp = _cmp_elm_elm(userData, (*next)->_doc, doc, cmptype);
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lookupKeyLess
  /// We have two more very similar functions which look up documents if
  /// only a key is given. This implies using the cmp_key_elm function
  /// and using the preorder only. Otherwise, they behave identically
  /// as the two previous ones.
  //////////////////////////////////////////////////////////////////////////////

  int lookupKeyLess(void* userData,
                    Key const* key, Node* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                    Node** next) const {
    int lev;
    int cmp = 0;  // just in case to avoid undefined values

    Node* cur = _start;
    for (lev = _start->_height - 1; lev >= 0; lev--) {
      while (true) {  // will be left by break
        *next = cur->_next[lev];
        if (nullptr == *next) {
          break;
        }
        cmp = _cmp_key_elm(userData, key, (*next)->_doc);
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lookupKeyLessOrEq
  //////////////////////////////////////////////////////////////////////////////

  int lookupKeyLessOrEq(void* userData,
                        Key const* key, Node* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                        Node** next) const {
    int lev;
    int cmp = 0;  // just in case to avoid undefined values

    Node* cur = _start;
    for (lev = _start->_height - 1; lev >= 0; lev--) {
      while (true) {  // will be left by break
        *next = cur->_next[lev];
        if (nullptr == *next) {
          break;
        }
        cmp = _cmp_key_elm(userData, key, (*next)->_doc);
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief randomHeight, select a node height randomly
  //////////////////////////////////////////////////////////////////////////////

  int RandomHeight() {
    int height = 1;
    int count;
    while (true) {  // will be left by return when the right height is found
      uint32_t r = RandomGenerator::interval(UINT32_MAX);

      for (count = 32; count > 0; count--) {
        if (0 != (r & 1UL) || height == TRI_SKIPLIST_MAX_HEIGHT) {
          return height;
        }
        r = r >> 1;
        height++;
      }
    }
  }

};  // class MMFilesSkiplist

}  // namespace arangodb

#endif
