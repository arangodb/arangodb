////////////////////////////////////////////////////////////////////////////////
/// @brief document utility functions
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

#ifndef TRIAGENS_UTILS_DOCUMENT_HELPER_H
#define TRIAGENS_UTILS_DOCUMENT_HELPER_H 1

#include "BasicsC/json.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                              class DocumentHelper
// -----------------------------------------------------------------------------

    class DocumentHelper {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

        DocumentHelper ();

        ~DocumentHelper ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a string
////////////////////////////////////////////////////////////////////////////////

        static inline string assembleDocumentId (const string& collectionName,
                                                 const string& key) {
          return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + key;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a char* key
////////////////////////////////////////////////////////////////////////////////

        static inline string assembleDocumentId (const string& collectionName,
                                                 const TRI_voc_key_t key) {
          if (key == 0) {
            return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + "_deleted";
          }

          return collectionName + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + key;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the "_key" attribute from a JSON object
////////////////////////////////////////////////////////////////////////////////

        static int getKey (TRI_json_t const* json, TRI_voc_key_t* key) {
          *key = 0;

          // check type of json
          if (json == 0 || json->_type != TRI_JSON_ARRAY) {
            return TRI_ERROR_NO_ERROR;
          }

          // check _key is there
          const TRI_json_t* k = TRI_LookupArrayJson((TRI_json_t*) json, "_key");

          if (k == 0) {
            return TRI_ERROR_NO_ERROR;
          }

          if (k->_type != TRI_JSON_STRING) {
            // _key is there but not a string
            return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
          }

          // _key is there and a string
          *key = k->_value._string.data;

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
