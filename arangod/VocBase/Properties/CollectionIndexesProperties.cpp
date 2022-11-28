////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "CollectionIndexesProperties.h"
#include "Basics/debugging.h"
#include "Basics/StaticStrings.h"

#include <velocypack/Builder.h>
using namespace arangodb;

CollectionIndexesProperties
CollectionIndexesProperties::defaultIndexesForCollectionType(
    TRI_col_type_e type) {
  // TODO: This is temporary and should be replaced by static index Property
  // generators
  CollectionIndexesProperties result;
  TRI_ASSERT(type == TRI_col_type_e::TRI_COL_TYPE_DOCUMENT ||
             type == TRI_col_type_e::TRI_COL_TYPE_EDGE);

  {
    // Primary Index
    VPackBuilder primary;
    {
      VPackObjectBuilder guard(&primary);
      primary.add(arangodb::StaticStrings::IndexId,
                  arangodb::velocypack::Value(std::to_string(0)));
      primary.add(arangodb::StaticStrings::IndexType,
                  arangodb::velocypack::Value("primary"));
      primary.add(arangodb::StaticStrings::IndexName,
                  arangodb::velocypack::Value("primary"));

      primary.add(
          arangodb::velocypack::Value(arangodb::StaticStrings::IndexFields));
      {
        VPackArrayBuilder keysArray(&primary);
        primary.add(VPackValue(StaticStrings::KeyString));
      }

      primary.add(arangodb::StaticStrings::IndexSparse,
                  arangodb::velocypack::Value(false));
      primary.add(arangodb::StaticStrings::IndexUnique,
                  arangodb::velocypack::Value(true));
    }
    result.indexes.emplace_back(std::move(primary));
  }
  if (type == TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    VPackBuilder fromIndex;
    {
      VPackObjectBuilder guard(&fromIndex);
      fromIndex.add(arangodb::StaticStrings::IndexId,
                    arangodb::velocypack::Value(std::to_string(1)));
      fromIndex.add(arangodb::StaticStrings::IndexType,
                    arangodb::velocypack::Value("edge"));
      fromIndex.add(arangodb::StaticStrings::IndexName,
                    arangodb::velocypack::Value("edge"));

      fromIndex.add(
          arangodb::velocypack::Value(arangodb::StaticStrings::IndexFields));
      {
        VPackArrayBuilder keysArray(&fromIndex);
        fromIndex.add(VPackValue(StaticStrings::FromString));
      }

      fromIndex.add(arangodb::StaticStrings::IndexSparse,
                    arangodb::velocypack::Value(false));
      fromIndex.add(arangodb::StaticStrings::IndexUnique,
                    arangodb::velocypack::Value(false));
    }
    result.indexes.emplace_back(std::move(fromIndex));

    VPackBuilder toIndex;
    {
      VPackObjectBuilder guard(&toIndex);
      toIndex.add(arangodb::StaticStrings::IndexId,
                  arangodb::velocypack::Value(std::to_string(2)));
      toIndex.add(arangodb::StaticStrings::IndexType,
                  arangodb::velocypack::Value("edge"));
      toIndex.add(arangodb::StaticStrings::IndexName,
                  arangodb::velocypack::Value("edge"));

      toIndex.add(
          arangodb::velocypack::Value(arangodb::StaticStrings::IndexFields));
      {
        VPackArrayBuilder keysArray(&toIndex);
        toIndex.add(VPackValue(StaticStrings::ToString));
      }

      toIndex.add(arangodb::StaticStrings::IndexSparse,
                  arangodb::velocypack::Value(false));
      toIndex.add(arangodb::StaticStrings::IndexUnique,
                  arangodb::velocypack::Value(false));
    }
    result.indexes.emplace_back(std::move(toIndex));
  }

  return result;
}
