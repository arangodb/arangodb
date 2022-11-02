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
#include <optional>

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/SupervisionAction.h"

namespace arangodb::replication2::replicated_state {

struct SupervisionContext {
  SupervisionContext() = default;
  SupervisionContext(SupervisionContext const&) = delete;
  SupervisionContext(SupervisionContext&&) noexcept = delete;
  SupervisionContext& operator=(SupervisionContext const&) = delete;
  SupervisionContext& operator=(SupervisionContext&&) noexcept = delete;

  template<typename ActionType, typename... Args>
  void createAction(Args&&... args) {
    if (std::holds_alternative<EmptyAction>(_action)) {
      _action.emplace<ActionType>(ActionType{std::forward<Args>(args)...});
    }
  }

  void reportStatus(agency::StatusCode code,
                    std::optional<ParticipantId> participant) {
    if (_isErrorReportingEnabled) {
      _reports.emplace_back(code, std::move(participant));
    }
  }

  void enableErrorReporting() noexcept { _isErrorReportingEnabled = true; }

  auto getAction() noexcept -> Action& { return _action; }
  auto getReport() noexcept -> agency::StatusReport& { return _reports; }

  auto hasUpdates() noexcept -> bool {
    return !std::holds_alternative<EmptyAction>(_action) || !_reports.empty();
  }

  auto isErrorReportingEnabled() const noexcept {
    return _isErrorReportingEnabled;
  }

  std::size_t numberServersInTarget;
  std::size_t numberServersOk;

 private:
  bool _isErrorReportingEnabled{false};
  Action _action;
  agency::StatusReport _reports;
};

void checkReplicatedState(
    SupervisionContext& ctx,
    std::optional<arangodb::replication2::agency::Log> const& log,
    agency::State const& state);

auto executeCheckReplicatedState(
    DatabaseID const& database, agency::State state,
    std::optional<arangodb::replication2::agency::Log> log,
    arangodb::agency::envelope) noexcept -> arangodb::agency::envelope;

}  // namespace arangodb::replication2::replicated_state
