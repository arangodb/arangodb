////////////////////////////////////////////////////////////////////////////////
/// @brief headers
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_HEADERS_H
#define ARANGODB_VOC_BASE_HEADERS_H 1

#include "Basics/Common.h"
#include "Basics/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_t;

// -----------------------------------------------------------------------------
// --SECTION--                                               class TRI_headers_t
// -----------------------------------------------------------------------------

class TRI_headers_t {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors & destructors
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the headers
////////////////////////////////////////////////////////////////////////////////

    TRI_headers_t ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the headers
////////////////////////////////////////////////////////////////////////////////

    ~TRI_headers_t ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief move an existing header to the end of the linked list
////////////////////////////////////////////////////////////////////////////////

    void moveBack (struct TRI_doc_mptr_t*, struct TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlink an existing header from the linked list, without freeing it
////////////////////////////////////////////////////////////////////////////////

    void unlink (struct TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief move an existing header to another position in the linked list
////////////////////////////////////////////////////////////////////////////////

    void move (struct TRI_doc_mptr_t*, struct TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief relink an existing header into the linked list, at its original
/// position
////////////////////////////////////////////////////////////////////////////////

    void relink (struct TRI_doc_mptr_t*, struct TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief request a new header
////////////////////////////////////////////////////////////////////////////////

    struct TRI_doc_mptr_t* request (size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief release/free an existing header, putting it back onto the freelist
////////////////////////////////////////////////////////////////////////////////

    void release (struct TRI_doc_mptr_t*, bool unlink);

////////////////////////////////////////////////////////////////////////////////
/// @brief adjust the total size (called by the collector when changing WAL
/// markers into datafile markers)
////////////////////////////////////////////////////////////////////////////////

    void adjustTotalSize (int64_t, int64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the element at the head of the list
///
/// note: the element returned might be nullptr
////////////////////////////////////////////////////////////////////////////////

    inline struct TRI_doc_mptr_t* front () const {
      return _begin;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the element at the tail of the list
///
/// note: the element returned might be nullptr
////////////////////////////////////////////////////////////////////////////////

    inline struct TRI_doc_mptr_t* back () const {
      return _end;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of active headers
////////////////////////////////////////////////////////////////////////////////

    inline size_t count () const {
      return _nrLinked;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the total size of linked headers
////////////////////////////////////////////////////////////////////////////////

    inline int64_t size () const {
      return _totalSize;
    }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

  private:

    TRI_doc_mptr_t const*  _freelist;    // free headers

    TRI_doc_mptr_t*        _begin;       // start pointer to list of allocated headers
    TRI_doc_mptr_t*        _end;         // end pointer to list of allocated headers
    size_t                 _nrAllocated; // number of allocated headers
    size_t                 _nrLinked;    // number of linked headers
    int64_t                _totalSize;   // total size of markers for linked headers

    TRI_vector_pointer_t   _blocks;
};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
