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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Inspection/VPack.h"

#include "PrototypeCore.h"
#include "PrototypeStateMachine.h"
#include "PrototypeStateMethods.h"

#include <memory>

namespace arangodb::replication2::replicated_state::prototype {
struct PrototypeLeaderState
    : IReplicatedLeaderState<PrototypeState>,
      std::enable_shared_from_this<PrototypeLeaderState> {
  using WaitForAppliedPromise = futures::Promise<futures::Unit>;
  using WaitForAppliedQueue = std::multimap<LogIndex, WaitForAppliedPromise>;

  explicit PrototypeLeaderState(std::unique_ptr<PrototypeCore> core);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<PrototypeCore> override;

  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override;

  void onSnapshotCompleted() noexcept override;

  auto set(std::unordered_map<std::string, std::string> entries,
           PrototypeStateMethods::PrototypeWriteOptions)
      -> futures::Future<LogIndex>;

  auto compareExchange(std::string key, std::string oldValue,
                       std::string newValue,
                       PrototypeStateMethods::PrototypeWriteOptions options)
      -> futures::Future<ResultT<LogIndex>>;

  auto remove(std::string key, PrototypeStateMethods::PrototypeWriteOptions)
      -> futures::Future<LogIndex>;
  auto remove(std::vector<std::string> keys,
              PrototypeStateMethods::PrototypeWriteOptions)
      -> futures::Future<LogIndex>;

  auto get(std::string key, LogIndex waitForApplied)
      -> futures::Future<ResultT<std::optional<std::string>>>;
  auto get(std::vector<std::string> keys, LogIndex waitForApplied)
      -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>>;

  auto getSnapshot(LogIndex waitForIndex)
      -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>>;

  auto waitForApplied(LogIndex waitForIndex) -> futures::Future<futures::Unit>;

  LoggerContext const loggerContext;

 private:
  auto executeOp(PrototypeLogEntry const&,
                 PrototypeStateMethods::PrototypeWriteOptions)
      -> futures::Future<LogIndex>;
  auto pollNewEntries();
  void handlePollResult(
      futures::Future<std::unique_ptr<EntryIterator>>&& pollFuture);

  struct GuardedData {
    explicit GuardedData(PrototypeLeaderState& self,
                         std::unique_ptr<PrototypeCore> core)
        : self(self), core(std::move(core)), nextWaitForIndex{1} {};

    [[nodiscard]] auto applyEntries(std::unique_ptr<EntryIterator> ptr)
        -> DeferredAction;

    auto waitForApplied(LogIndex index) -> futures::Future<futures::Unit>;

    [[nodiscard]] bool didResign() const noexcept { return core == nullptr; }

    PrototypeLeaderState& self;
    std::unique_ptr<PrototypeCore> core;
    WaitForAppliedQueue waitForAppliedQueue;
    LogIndex nextWaitForIndex;
  };

  Guarded<GuardedData, basics::UnshackledMutex> _guardedData;
};
}  // namespace arangodb::replication2::replicated_state::prototype
