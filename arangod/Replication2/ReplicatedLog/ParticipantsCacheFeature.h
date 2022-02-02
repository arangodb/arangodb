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
/// @author Alex Petenchea
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestServer/arangod.h"

#include <memory>
#include <unordered_map>

namespace arangodb::replication2 {

struct FailureOracle;
class ParticipantsCache;

class ParticipantsCacheFeature final : public ArangodFeature {
 public:
  static const std::string_view kParticipantsHealthPath;

  static constexpr std::string_view name() noexcept {
    return "ParticipantsCache";
  }

  explicit ParticipantsCacheFeature(Server& server);

  void prepare() override;

  void start() override;
  void stop() override;

  auto getFailureOracle() -> std::shared_ptr<FailureOracle>;

 private:
  static auto createHealthCache(Server& server)
      -> std::shared_ptr<ParticipantsCache>;

  std::shared_ptr<ParticipantsCache> cache;
};

}  // namespace arangodb::replication2
