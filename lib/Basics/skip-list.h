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

#ifndef ARANGODB_BASICS_C_SKIP__LIST_H
#define ARANGODB_BASICS_C_SKIP__LIST_H 1

#include "Basics/Common.h"

// We will probably never see more than 2^48 documents in a skip list
#define TRI_SKIPLIST_MAX_HEIGHT 48

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                         SKIP LIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a skiplist node
////////////////////////////////////////////////////////////////////////////////

    class SkipListNode {
      friend class SkipList;
        SkipListNode** _next;
        SkipListNode* _prev;
        void* _doc;
        int _height;
      public:
        void* document () {
          return _doc;
        }
        SkipListNode* nextNode () {
          return _next[0];
        }
        // Note that the prevNode of the first data node is the artificial
        // _start node not containing data. This is contrary to the prevNode
        // method of the SkipList class, which returns nullptr in that case.
        SkipListNode* prevNode () {
          return _prev;
        }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief two possibilities for comparison, see below
////////////////////////////////////////////////////////////////////////////////

    enum SkipListCmpType {
      SKIPLIST_CMP_PREORDER,
      SKIPLIST_CMP_TOTORDER
    };

////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////

    typedef int (*SkipListCmpElmElm)(void*, void*, void*, SkipListCmpType);
    typedef int (*SkipListCmpKeyElm)(void*, void*, void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Type of a pointer to a function that is called whenever a
/// document is removed from a skiplist.
////////////////////////////////////////////////////////////////////////////////

    typedef void (*SkipListFreeFunc)(void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a skiplist
/// _end always points to the last node in the skiplist, this can be the
/// same as the _start node. If a node does not have a successor on a certain
/// level, then the corresponding _next pointer is a nullptr.
////////////////////////////////////////////////////////////////////////////////

    class SkipList {
        SkipListNode* _start;
        SkipListNode* _end;
        SkipListCmpElmElm _cmp_elm_elm;
        SkipListCmpKeyElm _cmp_key_elm;
        void* _cmpdata;   // will be the first argument
        SkipListFreeFunc _free;
        bool _unique;     // indicates whether multiple entries that
                          // are equal in the preorder are allowed in
        uint64_t _nrUsed;
        size_t _memoryUsed;

      public:

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new skiplist
///
/// Returns nullptr if allocation fails and a pointer to the skiplist
/// otherwise.
////////////////////////////////////////////////////////////////////////////////

        SkipList (SkipListCmpElmElm cmp_elm_elm,
                  SkipListCmpKeyElm cmp_key_elm,
                  void* cmpdata,
                  SkipListFreeFunc freefunc,
                  bool unique);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a skiplist and all its documents
////////////////////////////////////////////////////////////////////////////////

        ~SkipList ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the start node, note that this does not return the first 
/// data node but the (internal) artificial node stored under _start. This
/// is consistent behaviour with the leftLookup method given a key value
/// of -infinity.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* startNode () const {
          return _start;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the end node, note that for formal reasons this always
/// returns a nullptr, which stands for the first value outside, in analogy
/// to startNode(). One has to use prevNode(nullptr) to get the last node
/// containing data.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* endNode () const {
          return nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the successor node or nullptr if last node
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* nextNode (SkipListNode* node) {
          return node->_next[0];
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the predecessor node or _startNode() if first node,
/// it is legal to call this with the nullptr to find the last node
/// containing data, if there is one.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* prevNode (SkipListNode* node) const {
          return nullptr == node ? _end : node->_prev;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document into a skiplist
///
/// Comparison is done using proper order comparison. If the skiplist
/// is unique then no two documents that compare equal in the preorder
/// can be inserted. Returns 0 if all is well, -1 if allocation failed
/// and -2 if the unique constraint would have been violated by the
/// insert. In the latter two cases nothing is inserted.
////////////////////////////////////////////////////////////////////////////////

        int insert (void* doc);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist
///
/// Comparison is done using proper order comparison. Returns 0 if all
/// is well and TRI_ERROR_DOCUMENT_NOT_FOUND if the document was not found.
////////////////////////////////////////////////////////////////////////////////

        int remove (void* doc);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of entries in the skiplist.
////////////////////////////////////////////////////////////////////////////////

        uint64_t getNrUsed () const {
          return _nrUsed;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the memory used by the index
////////////////////////////////////////////////////////////////////////////////

        size_t memoryUsage () const {
          return _memoryUsed;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up doc in the skiplist using the proper order
/// comparison.
///
/// Only comparisons using the proper order are done using cmp_elm_elm.
/// Returns nullptr if doc is not in the skiplist.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* lookup (void* doc) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less to doc in the preorder
/// comparison or the start node if none is.
///
/// Only comparisons using the preorder are done using cmp_elm_elm.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* leftLookup (void* doc) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less or equal to doc in
/// the preorder comparison or the start node if none is.
///
/// Only comparisons using the preorder are done using cmp_elm_elm.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* rightLookup (void* doc) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document whose key is less to key in the preorder
/// comparison or the start node if none is.
///
/// Only comparisons using the preorder are done using cmp_key_elm.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* leftKeyLookup (void* key) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less or equal to doc in
/// the preorder comparison or the start node if none is.
///
/// Only comparisons using the preorder are done using cmp_key_elm.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* rightKeyLookup (void* key) const;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate a new SkipListNode of a certain height. If height is 0,
/// then a random height is taken.
////////////////////////////////////////////////////////////////////////////////

        SkipListNode* allocNode (int height);

////////////////////////////////////////////////////////////////////////////////
///// @brief Free function for a node.
////////////////////////////////////////////////////////////////////////////////

        void freeNode (SkipListNode* node);

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
/// array *pos contains for each level lev in 0..sl->start->height-1
/// at (*pos)[lev] the pointer to the node that contains the largest
/// document that is less than doc amongst those nodes that have height >
/// lev.
////////////////////////////////////////////////////////////////////////////////

        int lookupLess (void* doc,
                        SkipListNode* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                        SkipListNode** next,
                        SkipListCmpType cmptype) const;

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

        int lookupLessOrEq (void* doc,
                            SkipListNode* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                            SkipListNode** next,
                            SkipListCmpType cmptype) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief lookupKeyLess
/// We have two more very similar functions which look up documents if
/// only a key is given. This implies using the cmp_key_elm function
/// and using the preorder only. Otherwise, they behave identically
/// as the two previous ones.
////////////////////////////////////////////////////////////////////////////////

        int lookupKeyLess (void* key,
                           SkipListNode* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                           SkipListNode** next) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief lookupKeyLessOrEq
////////////////////////////////////////////////////////////////////////////////

        int lookupKeyLessOrEq (void* key,
                               SkipListNode* (*pos)[TRI_SKIPLIST_MAX_HEIGHT],
                               SkipListNode** next) const;

    };  // struct SkipList

  }   // namespace triagens::basics
}   // namespace triagens

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
