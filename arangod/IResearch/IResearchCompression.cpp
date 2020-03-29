////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchCompression.h"
#include "Basics/debugging.h"
#include <unordered_map>

namespace {
const std::unordered_map<
  std::string,
  arangodb::iresearch::ColumnCompression> COMPRESSION_CONVERT_MAP = {
{ "lz4", arangodb::iresearch::ColumnCompression::LZ4 },
{ "none", arangodb::iresearch::ColumnCompression::NONE },
#ifdef ARANGODB_USE_GOOGLE_TESTS
{ "test", arangodb::iresearch::ColumnCompression::TEST },
#endif
};
}


namespace arangodb {
namespace iresearch {

irs::string_ref columnCompressionToString(ColumnCompression c) {
  for (auto const&it : COMPRESSION_CONVERT_MAP) {
    if (it.second == c) {
      return it.first;
    }
  }
  TRI_ASSERT(false);
  return irs::string_ref::NIL;
}

ColumnCompression columnCompressionFromString(irs::string_ref const& c) {
  TRI_ASSERT(!c.null());
  auto it = COMPRESSION_CONVERT_MAP.find(c);
  if (it != COMPRESSION_CONVERT_MAP.end()) {
    return it->second;
  }
  return ColumnCompression::INVALID;
}

}  // namespace iresearch
}  // namespace arangodb

