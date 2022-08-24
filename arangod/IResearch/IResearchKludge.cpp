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
#include "IResearchDocument.h"
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

constexpr char kTypeDelimiter = '\0';
constexpr char kAnalyzerDelimiter = '\1';
#ifdef USE_ENTERPRISE
constexpr char kNestedDelimiter = '\2';
#endif

std::string_view constexpr kNullSuffix{"\0_n", 3};
std::string_view constexpr kBoolSuffix{"\0_b", 3};
std::string_view constexpr kNumericSuffix{"\0_d", 3};
std::string_view constexpr kStringSuffix{"\0_s", 3};

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

void mangleType(std::string& name) { name += kTypeDelimiter; }

void mangleAnalyzer(std::string& name) {
  normalizeExpansion(name);
  name += kAnalyzerDelimiter;
}

void mangleNull(std::string& name) {
  normalizeExpansion(name);
  name.append(kNullSuffix);
}

void mangleBool(std::string& name) {
  normalizeExpansion(name);
  name.append(kBoolSuffix);
}

void mangleNumeric(std::string& name) {
  normalizeExpansion(name);
  name.append(kNumericSuffix);
}

void mangleString(std::string& name) {
  normalizeExpansion(name);
  name.append(kStringSuffix);
}

#ifdef USE_ENTERPRISE
void mangleNested(std::string& name) {
  normalizeExpansion(name);
  name += kNestedDelimiter;
}
#endif

bool needTrackPrevDoc(irs::string_ref name, bool nested) noexcept {
#ifdef USE_ENTERPRISE
  return (!name.empty() && name.back() == kNestedDelimiter) ||
         (nested && name == DocumentPrimaryKey::PK());
#else
  return false;
#endif
}

void mangleField(std::string& name, bool isOldMangling,
                 iresearch::FieldMeta::Analyzer const& analyzer) {
  normalizeExpansion(name);
  if (isOldMangling || analyzer._pool->requireMangled()) {
    name += kAnalyzerDelimiter;
    name += analyzer._shortName;
  } else {
    name.append(kStringSuffix);
  }
}

std::string_view demangleType(std::string_view name) noexcept {
  if (name.empty()) {
    return {};
  }

  for (size_t i = name.size() - 1;; --i) {
    if (name[i] <= kAnalyzerDelimiter) {
      return {name.data(), i};
    }
    if (i == 0) {
      break;
    }
  }

  return name;
}

#ifdef USE_ENTERPRISE
std::string_view demangleNested(std::string_view name, std::string& buf) {
  auto const end = std::end(name);
  if (end == std::find(std::begin(name), end, kNestedDelimiter)) {
    return name;
  }

  auto prev = std::begin(name);
  auto cur = prev;

  buf.clear();

  for (auto end = std::end(name); cur != end; ++cur) {
    if (kNestedDelimiter == *cur) {
      buf.append(prev, cur);
      prev = cur + 1;
    }
  }
  buf.append(prev, cur);

  return buf;
}
#endif

}  // namespace arangodb::iresearch::kludge
