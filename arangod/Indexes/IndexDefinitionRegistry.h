////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arangodb {

// Definition-only counterpart to IndexFactory; deliberately not derived
// from it, since IndexFactory::emplace() is typed to IndexTypeFactory
class IndexDefinitionRegistry {
 public:
  explicit IndexDefinitionRegistry(
      application_features::ApplicationServer& server);
  virtual ~IndexDefinitionRegistry() = default;

  /// @brief returns if 'definition' for 'type' was added successfully
  Result emplace(std::string const& type, IndexDefinition const& definition);

  /// @brief returns the definition for the specified type, or a failing
  ///        placeholder if no such type
  IndexDefinition const& definition(std::string const& type) const noexcept;

  Result enhanceIndexDefinition(velocypack::Slice definition,
                                velocypack::Builder& normalized,
                                bool isCreation, Database const& vocbase) const;

  /// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" =>
  /// "hash") used to display storage engine capabilities
  virtual std::vector<std::pair<std::string_view, std::string_view>>
  indexAliases(uint32_t apiVersion) const;

 protected:
  application_features::ApplicationServer& _server;
  std::unordered_map<std::string, IndexDefinition const*> _definitions;
  std::unique_ptr<IndexDefinition> _invalid;
};

}  // namespace arangodb
