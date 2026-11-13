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

#pragma once

#include "Basics/Result.h"
#include "Indexes/IndexDefinition.h"
#include "Indexes/IndexType.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arangodb {

namespace application_features {
class ApplicationServer;
}  // namespace application_features

struct Database;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

// lets _byName be looked up by string_view without allocating a std::string
struct TransparentStringHash {
  using is_transparent = void;
  size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

// Index type names and equal/normalize rules, shared by both engines.
class IndexTypeCatalog final {
 public:
  IndexTypeCatalog(application_features::ApplicationServer& server);

  [[nodiscard]] IndexType resolve(std::string_view name) const noexcept;

  // the definition registered under this name, or nullptr if unknown; used
  // by IndexFactory::prepareIndexFromSlice to dispatch create() without a
  // second, independent name->IndexType mapping
  [[nodiscard]] IndexDefinition const* definitionFor(
      std::string_view name) const noexcept;

  [[nodiscard]] Result normalizeType(std::string_view name,
                                     velocypack::Builder& normalized,
                                     velocypack::Slice definition,
                                     bool isCreation,
                                     Database const& vocbase) const;

  [[nodiscard]] bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
                           std::string const& dbname) const;

  [[nodiscard]] std::vector<std::pair<std::string_view, std::string_view>>
  aliases() const;

  // the catalog takes ownership of definition
  [[nodiscard]] Result add(std::string_view name,
                           std::unique_ptr<IndexDefinition> definition);

 private:
  // owns the built-ins plus anything registered via add()
  std::unordered_map<std::string, std::unique_ptr<IndexDefinition>>
      _definitions;

  // includes aliases (e.g. "rocksdb" for persistent)
  std::unordered_map<std::string, IndexDefinition const*, TransparentStringHash,
                     std::equal_to<>>
      _byName;
};

}  // namespace arangodb
