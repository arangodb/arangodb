////////////////////////////////////////////////////////////////////////////////
/// @brief full text search, result list handling
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

#ifndef ARANGODB_FULLTEXT_INDEX_FULLTEXT__RESULT_H
#define ARANGODB_FULLTEXT_INDEX_FULLTEXT__RESULT_H 1

#include "fulltext-common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a fulltext result list
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_fulltext_result_s {
  uint32_t             _numDocuments;
  TRI_fulltext_doc_t*  _documents;
}
TRI_fulltext_result_t;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// @brief create a result
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_result_t* TRI_CreateResultFulltextIndex (const uint32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a result
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyResultFulltextIndex (TRI_fulltext_result_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a result
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeResultFulltextIndex (TRI_fulltext_result_t*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
