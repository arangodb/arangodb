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

#include "PrototypeStateMachine.h"
#include "PrototypeLeaderState.h"

#include "Logger/LogContextKeys.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

PrototypeLeaderState::PrototypeLeaderState(std::unique_ptr<PrototypeCore> core)
    : loggerContext(
          core->loggerContext.with<logContextKeyStateComponent>("LeaderState")),
      _guardedData(std::move(core)) {}

auto PrototypeLeaderState::resign() && noexcept
    -> std::unique_ptr<PrototypeCore> {
  return _guardedData.doUnderLock([](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }
    return std::move(data.core);
  });
}

auto PrototypeLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  return _guardedData.doUnderLock(
      [self = shared_from_this(), ptr = std::move(ptr)](auto& data) mutable {
        if (data.didResign()) {
          return Result{TRI_ERROR_CLUSTER_NOT_LEADER};
        }
        auto resolvePromises = self->applyEntries(std::move(ptr));
        resolvePromises.fire();
        return Result{TRI_ERROR_NO_ERROR};
      });
}

auto PrototypeLeaderState::set(
    std::unordered_map<std::string, std::string> entries)
    -> futures::Future<ResultT<LogIndex>> {
  auto stream = getStream();
  PrototypeLogEntry entry{
      PrototypeLogEntry::InsertOperation{std::move(entries)}};
  return stream->insert(entry);
}

auto PrototypeLeaderState::remove(std::string key)
    -> futures::Future<ResultT<LogIndex>> {
  auto stream = getStream();
  PrototypeLogEntry entry{PrototypeLogEntry::DeleteOperation{.keys = {key}}};
  return stream->insert(entry);
}

auto PrototypeLeaderState::remove(std::vector<std::string> keys)
    -> futures::Future<ResultT<LogIndex>> {
  auto stream = getStream();
  PrototypeLogEntry entry{
      PrototypeLogEntry::DeleteOperation{.keys = std::move(keys)}};
  return stream->insert(entry);
}

// TODO return result
auto PrototypeLeaderState::get(std::vector<std::string> keys)
    -> std::unordered_map<std::string, std::string> {
  return _guardedData.doUnderLock([keys = std::move(keys)](auto& data) {
    if (data.didResign()) {
      return std::unordered_map<std::string, std::string>{};
    }
    return data.core->get(keys);
  });
}

auto PrototypeLeaderState::get(std::string key) -> std::optional<std::string> {
  return _guardedData.doUnderLock(
      [key = std::move(key)](auto& data) -> std::optional<std::string> {
        if (data.didResign()) {
          return std::nullopt;
        }
        return data.core->get(key);
      });
}

auto PrototypeLeaderState::getSnapshot(LogIndex waitForIndex)
    -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>> {
  return _guardedData.doUnderLock([waitForIndex, self = shared_from_this()](
                                      auto& data) mutable
                                  -> futures::Future<ResultT<std::unordered_map<
                                      std::string, std::string>>> {
    if (data.didResign()) {
      return ResultT<std::unordered_map<std::string, std::string>>::error(
          TRI_ERROR_CLUSTER_NOT_LEADER);
    }

    return self->waitForApplied(waitForIndex)
        .then([&data,
               self = std::move(self)](futures::Try<futures::Unit>&& tryResult)
                  -> ResultT<std::unordered_map<std::string, std::string>> {
          if (data.didResign()) {
            return ResultT<std::unordered_map<std::string, std::string>>::error(
                TRI_ERROR_CLUSTER_NOT_LEADER);
          }

          auto result = basics::catchToResultT([&] { return tryResult.get(); });
          if (result.fail()) {
            return result.result();
          }
          return ResultT<std::unordered_map<std::string, std::string>>::success(
              data.core->getSnapshot());
        });
  });
}

auto PrototypeLeaderState::pollNewEntries() {
  auto stream = getStream();
  return _guardedData.doUnderLock([&](auto& data) {
    return stream->waitForIterator(data.nextWaitForIndex);
  });
}

void PrototypeLeaderState::handlePollResult(
    futures::Future<std::unique_ptr<EntryIterator>>&& pollFuture) {
  std::move(pollFuture)
      .then([weak = weak_from_this()](
                futures::Try<std::unique_ptr<EntryIterator>> tryResult) {
        auto self = weak.lock();
        if (self == nullptr) {
          return;
        }

        auto result =
            basics::catchToResultT([&] { return std::move(tryResult).get(); });
        if (result.fail()) {
          THROW_ARANGO_EXCEPTION(result.result());
        }

        auto resolvePromises = self->applyEntries(std::move(result.get()));
        resolvePromises.fire();

        self->handlePollResult(self->pollNewEntries());
      });
}

auto PrototypeLeaderState::applyEntries(std::unique_ptr<EntryIterator> ptr)
    -> DeferredAction {
  return _guardedData.doUnderLock([self = shared_from_this(),
                                   ptr = std::move(ptr)](auto& data) mutable {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }
    auto nextWaitForIndex = ptr->range().to;
    data.core->applyEntries(std::move(ptr));
    data.nextWaitForIndex = nextWaitForIndex;

    if (data.core->flush()) {
      auto stream = self->getStream();
      stream->release(data.core->getLastPersistedIndex());
    }

    auto resolveQueue = std::make_unique<WaitForAppliedQueue>();

    auto const end =
        data.waitForAppliedQueue.lower_bound(data.nextWaitForIndex);
    for (auto it = data.waitForAppliedQueue.begin(); it != end; ++it) {
      resolveQueue->insert(data.waitForAppliedQueue.extract(it));
    }

    return DeferredAction([resolveQueue = std::move(resolveQueue)]() noexcept {
      for (auto& p : *resolveQueue) {
        p.second.setValue();
      }
    });
  });
}

auto PrototypeLeaderState::waitForApplied(LogIndex index)
    -> futures::Future<futures::Unit> {
  return _guardedData.doUnderLock([index](auto& data) {
    if (index < data.nextWaitForIndex) {
      return futures::Future<futures::Unit>{std::in_place};
    }
    auto it = data.waitForAppliedQueue.emplace(index, WaitForAppliedPromise{});
    auto f = it->second.getFuture();
    TRI_ASSERT(f.valid());
    return f;
  });
}

void PrototypeLeaderState::start() { handlePollResult(pollNewEntries()); }

#include "Replication2/ReplicatedState/ReplicatedState.tpp"