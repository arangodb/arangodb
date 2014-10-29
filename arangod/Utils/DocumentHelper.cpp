////////////////////////////////////////////////////////////////////////////////
/// @brief document utility functions
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

#include "DocumentHelper.h"

#include "Basics/json.h"
#include "Basics/StringUtils.h"
#include "VocBase/vocbase.h"

using namespace triagens::arango;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                              class DocumentHelper
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a string
////////////////////////////////////////////////////////////////////////////////

std::string DocumentHelper::assembleDocumentId (std::string const& collectionName,
                                                std::string const& key) {
  return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a char* key
////////////////////////////////////////////////////////////////////////////////

std::string DocumentHelper::assembleDocumentId (std::string const& collectionName,
                                                const TRI_voc_key_t key) {
  if (key == 0) {
    return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + "_unknown";
  }

  return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id and document key from an id
////////////////////////////////////////////////////////////////////////////////

bool DocumentHelper::parseDocumentId (CollectionNameResolver const& resolver,
                                      char const* input,
                                      TRI_voc_cid_t& cid,
                                      char** key) {

  if (input == nullptr) {
    return false;
  }

  char const* pos = strchr(input, TRI_DOCUMENT_HANDLE_SEPARATOR_CHR);

  if (pos == nullptr) {
    return false;
  }

  cid = resolver.getCollectionIdCluster(std::string(input, pos - input));
  *key = (char*) (pos + 1);

  if (cid == 0 || **key == '\0') {
    // unknown collection or empty key
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the "_key" attribute from a JSON object
////////////////////////////////////////////////////////////////////////////////

int DocumentHelper::getKey (TRI_json_t const* json,
                            TRI_voc_key_t* key) {
  *key = 0;

  // check type of json
  if (! TRI_IsArrayJson(json)) {
    return TRI_ERROR_NO_ERROR;
  }

  // check if _key is there
  TRI_json_t const* k = TRI_LookupArrayJson(json, TRI_VOC_ATTRIBUTE_KEY);

  if (k == nullptr) {
    return TRI_ERROR_NO_ERROR;
  }

  if (! TRI_IsStringJson(k)) {
    // _key is there but not a string
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  // _key is there and a string
  *key = k->_value._string.data;

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
