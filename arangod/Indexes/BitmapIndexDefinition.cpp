////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "BitmapIndexDefinition.h"

#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "RocksDBEngine/RocksDBBitmapIndex.h"
#include "RocksDBEngine/RocksDBBitmapIndexFormat.h"

#include <absl/strings/str_cat.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

Result BitmapIndexDefinition::normalize(velocypack::Builder& normalized,
                                        velocypack::Slice definition,
                                        bool isCreation,
                                        Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(_type)));

  if (isCreation) {
    normalized.add("shardBits", velocypack::Value(bitmap_index::kShardBits));
  } else if (auto bits = definition.get("shardBits"); bits.isNumber()) {
    normalized.add("shardBits", bits);
  }

  auto values = definition.get(RocksDBBitmapIndex::kValuesAttribute);
  if (values.isNone()) {
    normalized.add(RocksDBBitmapIndex::kValuesAttribute,
                   velocypack::Value(bitmap_index::AnyValues::kName));
  } else if (values.isEqualString(bitmap_index::AnyValues::kName) ||
             values.isEqualString(bitmap_index::ScalarValues::kName)) {
    normalized.add(RocksDBBitmapIndex::kValuesAttribute, values);
  } else {
    return Result(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("<", RocksDBBitmapIndex::kValuesAttribute, "> must be \"",
                     bitmap_index::AnyValues::kName, "\" or \"",
                     bitmap_index::ScalarValues::kName, "\""));
  }

  return IndexFactory::enhanceJsonIndexBitmap(definition, normalized,
                                              isCreation);
}

}  // namespace arangodb
