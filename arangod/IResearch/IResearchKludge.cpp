////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchKludge.h"
#include "IResearchRocksDBLink.h"
#include "IResearchRocksDBInvertedIndex.h"
#include "Basics/DownCast.h"

#include <string>
#include <string_view>

namespace {

inline void normalizeExpansion(std::string& name) {
  // remove the last expansion as it could be omitted accodring to our
  // indicies behaviour
  if (name.ends_with("[*]")) {
    name.resize(name.size() - 3);
  }
}

}  // namespace

namespace arangodb {
void syncIndexOnCreate(Index& index) {
  iresearch::IResearchDataStore* store{nullptr};
  switch (index.type()) {
    case Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK:
      store = &basics::downCast<iresearch::IResearchRocksDBLink>(index);
      break;
    case Index::IndexType::TRI_IDX_TYPE_INVERTED_INDEX:
      store =
          &basics::downCast<iresearch::IResearchRocksDBInvertedIndex>(index);
      break;
    default:
      break;
  }
  if (store) {
    store->commit();
  }
}
}  // namespace arangodb

namespace arangodb::iresearch::kludge {

const char TYPE_DELIMITER = '\0';
const char ANALYZER_DELIMITER = '\1';

std::string_view constexpr NULL_SUFFIX("\0_n", 3);
std::string_view constexpr BOOL_SUFFIX("\0_b", 3);
std::string_view constexpr NUMERIC_SUFFIX("\0_d", 3);
std::string_view constexpr STRING_SUFFIX("\0_s", 3);

void mangleType(std::string& name) { name += TYPE_DELIMITER; }

void mangleAnalyzer(std::string& name) {
  normalizeExpansion(name);
  name += ANALYZER_DELIMITER;
}

void mangleNull(std::string& name) {
  normalizeExpansion(name);
  name.append(NULL_SUFFIX);
}

void mangleBool(std::string& name) {
  normalizeExpansion(name);
  name.append(BOOL_SUFFIX);
}

void mangleNumeric(std::string& name) {
  normalizeExpansion(name);
  name.append(NUMERIC_SUFFIX);
}

void mangleString(std::string& name) {
  normalizeExpansion(name);
  name.append(STRING_SUFFIX);
}

void mangleField(std::string& name, bool isSearchFilter,
                 iresearch::FieldMeta::Analyzer const& analyzer) {
  normalizeExpansion(name);
  if (isSearchFilter || analyzer._pool->requireMangled()) {
    name += ANALYZER_DELIMITER;
    name += analyzer._shortName;
  } else {
    mangleString(name);
  }
}

}  // namespace arangodb::iresearch::kludge
