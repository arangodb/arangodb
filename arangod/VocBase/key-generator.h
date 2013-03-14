////////////////////////////////////////////////////////////////////////////////
/// @brief collection key generator
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_KEY_GENERATOR_H
#define TRIAGENS_VOC_BASE_KEY_GENERATOR_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    KEY GENERATORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              FORWARD DECLARATIONS
// -----------------------------------------------------------------------------

struct TRI_doc_document_key_marker_s;
struct TRI_json_s;
struct TRI_primary_collection_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief key validation regex
///
/// this regex alone is not sufficient as we additionally need to perform a
/// UTF-8 validation. this is done in TRI_IsAllowedKey()
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_KEY_REGEX "[a-zA-Z0-9_:-]+"

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum length of a key in a collection
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_KEY_MAX_LENGTH (254)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_key_generator_s {
  struct TRI_json_s*               _parameters;
  struct TRI_primary_collection_s* _collection;
  void*                            _data;

  int (*init)(struct TRI_key_generator_s* const, const struct TRI_json_s* const);
  int (*generate)(struct TRI_key_generator_s* const, const size_t, const struct TRI_doc_document_key_marker_s* const, const char* const, char* const, size_t* const);
  void (*free)(struct TRI_key_generator_s* const);
}
TRI_key_generator_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a key generator and attach it to the collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateKeyGenerator (const struct TRI_json_s* const,
                            struct TRI_primary_collection_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a key generator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeKeyGenerator (TRI_key_generator_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a key is allowed
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
