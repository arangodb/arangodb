////////////////////////////////////////////////////////////////////////////////
/// @brief full text search, common types
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_FULLTEXT_INDEX_FULLTEXT__COMMON_H
#define ARANGODB_FULLTEXT_INDEX_FULLTEXT__COMMON_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    public defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief enable debugging
////////////////////////////////////////////////////////////////////////////////

#undef TRI_FULLTEXT_DEBUG

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for the fulltext index
/// this is just a void* for users, and the actual index definitions is only
/// used internally in file fulltext-index.c
/// the reason is the index does some binary stuff its users should not bother
/// with
////////////////////////////////////////////////////////////////////////////////

typedef void TRI_fts_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a document indexed. this must be big enough to contain
/// a TRI_doc_mptr_t*
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_fulltext_doc_t;

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
