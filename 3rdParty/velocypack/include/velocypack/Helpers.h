////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_HELPERS_H
#define VELOCYPACK_HELPERS_H 1

#include <string>
#include <cstdint>
#include <unordered_set>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {

struct TopLevelAttributeExcludeHandler final : AttributeExcludeHandler {
  TopLevelAttributeExcludeHandler (std::unordered_set<std::string> const& attributes)
    : attributes(attributes) {
  }

  bool shouldExclude(Slice const& key, int nesting) override final {
    return (nesting == 1 && attributes.find(key.copyString()) != attributes.end());
  }

  std::unordered_set<std::string> attributes;
};

static inline Slice buildNullValue(char* dst, size_t length) {
  if (length < 1) {
    throw Exception(Exception::InternalError, "supplied buffer is too small");
  }

  *dst = 0x18;
  return Slice(dst);
}

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
