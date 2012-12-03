////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext query functionality 
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012 triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_FULLTEXT_QUERY_H
#define TRIAGENS_VOC_BASE_FULLTEXT_QUERY_H 1

#include "FulltextIndex/FTS_index.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalise a word for a fulltext search query
////////////////////////////////////////////////////////////////////////////////

char* TRI_NormaliseWordFulltextIndex (const char*, const size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief free fulltext search query options
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryFulltextIndex (FTS_query_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief query the fulltext index
////////////////////////////////////////////////////////////////////////////////

FTS_document_ids_t* TRI_FindDocumentsFulltextIndex (FTS_index_t*,
                                                    FTS_query_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free the results of a fulltext query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeResultsFulltextIndex (FTS_document_ids_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
