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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_MMFILES_TOKEN_H
#define ARANGOD_MMFILES_MMFILES_TOKEN_H 1

#include "StorageEngine/DocumentIdentifierToken.h"

namespace arangodb {

struct MMFilesToken : public DocumentIdentifierToken {
 public:
  MMFilesToken() : DocumentIdentifierToken() {}
  explicit MMFilesToken(TRI_voc_rid_t revisionId)
      : DocumentIdentifierToken(revisionId) {}
  MMFilesToken(MMFilesToken const& other)
      : DocumentIdentifierToken(other._data) {}

  inline TRI_voc_rid_t revisionId() const {
    return static_cast<TRI_voc_rid_t>(_data);
  }
};
static_assert(sizeof(MMFilesToken) == sizeof(uint64_t),
              "invalid MMFilesToken size");

}  // namespace arangodb

#endif
