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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "RestServer/arangod.h"
#include "Pregel3/GlobalSettings.h"
#include "Query.h"

#include <cstdint>

namespace arangodb::pregel3 {
class Pregel3Feature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Pregel3"; }
  explicit Pregel3Feature(Server& server);
  ~Pregel3Feature() override;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;
  void prepare() override;

  void createQuery(TRI_vocbase_t& vocbase, std::string queryId,
                   pregel3::GraphSpecification const& graph);

  /**
   * Generate the id from the next number not already in use.
   * @return
   */
  std::string generateQueryId() {
    while (_queries.contains(std::to_string(nextFreeQueryId))) {
      ++nextFreeQueryId;
    }
    if (nextFreeQueryId == std::numeric_limits<uint64_t>::max()) {
      // actually should never happen
      // todo throw an appropriate error
    }
    return std::to_string(nextFreeQueryId++);
  }

  bool hasQueryId(std::string const& queryId) {
    return _queries.contains(queryId);
  }

  auto getQuery(std::string const& queryId) -> std::shared_ptr<Query> {
    auto const& it = _queries.find(queryId);
    if (it == _queries.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::unordered_map<pregel3::QueryId, std::shared_ptr<pregel3::Query>>
      _queries;
  uint64_t nextFreeQueryId = 0;
  pregel3::GlobalSettings _settings;
};

}  // namespace arangodb::pregel3
