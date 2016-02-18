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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_DOCUMENT_HELPER_H
#define ARANGOD_UTILS_DOCUMENT_HELPER_H 1

#include "Basics/Common.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/voc-types.h"

struct TRI_json_t;

namespace arangodb {

class DocumentHelper {
 private:
  DocumentHelper() = delete;

  ~DocumentHelper() = delete;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief assemble a document id from a string and a string
  //////////////////////////////////////////////////////////////////////////////

  static std::string assembleDocumentId(std::string const&,
                                        std::string const& key,
                                        bool urlEncode = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief assemble a document id from a string and a char* key
  //////////////////////////////////////////////////////////////////////////////

  static std::string assembleDocumentId(std::string const&, const TRI_voc_key_t,
                                        bool urlEncode = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract the collection id and document key from an id
  //////////////////////////////////////////////////////////////////////////////

  static bool parseDocumentId(arangodb::CollectionNameResolver const&,
                              char const*, TRI_voc_cid_t&, char**);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract the "_key" attribute from a JSON object
  //////////////////////////////////////////////////////////////////////////////

  static int getKey(struct TRI_json_t const*, TRI_voc_key_t*);
};
}

#endif
