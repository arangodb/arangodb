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

#include "PrototypeFollowerState.h"
#include "PrototypeLeaderState.h"
#include "PrototypeStateMachine.h"

#include "Basics/application-exit.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"

#include <yaclib/async/make.hpp>
#include <yaclib/async/contract.hpp>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

PrototypeLeaderState::PrototypeLeaderState(std::unique_ptr<PrototypeCore> core)
    : loggerContext(
          core->loggerContext.with<logContextKeyStateComponent>("LeaderState")),
      _guardedData(*this, std::move(core)) {}

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
    -> yaclib::Future<Result> {
  auto [result, action] = _guardedData.doUnderLock(
      [self = shared_from_this(), ptr = std::move(ptr)](
          auto& data) mutable -> std::pair<Result, DeferredAction> {
        if (data.didResign()) {
          return {Result{TRI_ERROR_CLUSTER_NOT_LEADER}, DeferredAction{}};
        }
        auto resolvePromises = data.applyEntries(std::move(ptr));
        return std::make_pair(Result{TRI_ERROR_NO_ERROR},
                              std::move(resolvePromises));
      });
  return yaclib::MakeFuture(std::move(result));
}

auto PrototypeLeaderState::set(
    std::unordered_map<std::string, std::string> entries,
    PrototypeStateMethods::PrototypeWriteOptions options)
    -> yaclib::Future<LogIndex> {
  return executeOp(PrototypeLogEntry::createInsert(std::move(entries)),
                   options);
}

auto PrototypeLeaderState::compareExchange(
    std::string key, std::string oldValue, std::string newValue,
    PrototypeStateMethods::PrototypeWriteOptions options)
    -> yaclib::Future<ResultT<LogIndex>> {
  auto [f, da] = _guardedData.doUnderLock(
      [this, options, key = std::move(key), oldValue = std::move(oldValue),
       newValue = std::move(newValue)](auto& data) mutable
      -> std::pair<yaclib::Future<ResultT<LogIndex>>, DeferredAction> {
        if (data.didResign()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
        }

        if (!data.core->compare(key, oldValue)) {
          return std::make_pair(yaclib::MakeFuture(ResultT<LogIndex>::error(
                                    TRI_ERROR_ARANGO_CONFLICT)),
                                DeferredAction());
        }

        auto entry = PrototypeLogEntry::createCompareExchange(
            std::move(key), std::move(oldValue), std::move(newValue));
        auto [idx, action] = getStream()->insertDeferred(entry);
        data.core->applyToOngoingState(idx, entry);

        if (options.waitForApplied) {
          return std::make_pair(
              std::move(data.waitForApplied(idx)).ThenInline([idx = idx]() {
                return ResultT<LogIndex>::success(idx);
              }),
              std::move(action));
        }
        return std::make_pair(
            yaclib::MakeFuture(ResultT<LogIndex>::success(idx)),
            std::move(action));
      });
  da.fire();
  return std::move(f);
}

auto PrototypeLeaderState::remove(
    std::string key, PrototypeStateMethods::PrototypeWriteOptions options)
    -> yaclib::Future<LogIndex> {
  return remove(std::vector<std::string>{std::move(key)}, options);
}

auto PrototypeLeaderState::remove(
    std::vector<std::string> keys,
    PrototypeStateMethods::PrototypeWriteOptions options)
    -> yaclib::Future<LogIndex> {
  return executeOp(PrototypeLogEntry::createDelete(std::move(keys)), options);
}

auto PrototypeLeaderState::get(std::vector<std::string> keys,
                               LogIndex waitForApplied)
    -> yaclib::Future<ResultT<std::unordered_map<std::string, std::string>>> {
  auto f = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }

    return data.waitForApplied(waitForApplied);
  });

  return std::move(f).ThenInline(
      [keys = std::move(keys), weak = weak_from_this()]()
          -> ResultT<std::unordered_map<std::string, std::string>> {
        auto self = weak.lock();
        if (self == nullptr) {
          return {TRI_ERROR_CLUSTER_NOT_LEADER};
        }

        return self->_guardedData.doUnderLock(
            [&](GuardedData& data)
                -> ResultT<std::unordered_map<std::string, std::string>> {
              if (data.didResign()) {
                return {TRI_ERROR_CLUSTER_NOT_LEADER};
              }

              return {data.core->get(keys)};
            });
      });
}

auto PrototypeLeaderState::get(std::string key, LogIndex waitForApplied)
    -> yaclib::Future<ResultT<std::optional<std::string>>> {
  auto f = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }

    return data.waitForApplied(waitForApplied);
  });

  return std::move(f).ThenInline(
      [key = std::move(key),
       weak = weak_from_this()]() -> ResultT<std::optional<std::string>> {
        auto self = weak.lock();
        if (self == nullptr) {
          return {TRI_ERROR_CLUSTER_NOT_LEADER};
        }

        return self->_guardedData.doUnderLock(
            [&](GuardedData& data) -> ResultT<std::optional<std::string>> {
              if (data.didResign()) {
                return {TRI_ERROR_CLUSTER_NOT_LEADER};
              }

              return {data.core->get(key)};
            });
      });
}

auto PrototypeLeaderState::getSnapshot(LogIndex waitForIndex)
    -> yaclib::Future<ResultT<std::unordered_map<std::string, std::string>>> {
  auto f = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE);
    }

    return data.waitForApplied(waitForIndex);
  });

  return std::move(f).ThenInline(
      [weak = weak_from_this()]()
          -> ResultT<std::unordered_map<std::string, std::string>> {
        auto self = weak.lock();
        if (self == nullptr) {
          return {TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE};
        }

        return self->_guardedData.doUnderLock(
            [&](GuardedData& data)
                -> ResultT<std::unordered_map<std::string, std::string>> {
              if (data.didResign()) {
                return {TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE};
              }

              return {data.core->getSnapshot()};
            });
      });
}

auto PrototypeLeaderState::executeOp(
    PrototypeLogEntry const& entry,
    PrototypeStateMethods::PrototypeWriteOptions options)
    -> yaclib::Future<LogIndex> {
  auto [f, da] = _guardedData.doUnderLock(
      [&](auto& data) -> std::pair<yaclib::Future<LogIndex>, DeferredAction> {
        if (data.didResign()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
        }

        auto [idx, action] = getStream()->insertDeferred(entry);
        data.core->applyToOngoingState(idx, entry);

        if (options.waitForApplied) {
          return std::make_pair(
              std::move(data.waitForApplied(idx)).ThenInline([idx = idx] {
                return idx;
              }),
              std::move(action));
        }
        return std::make_pair(yaclib::MakeFuture<LogIndex>(idx),
                              std::move(action));
      });
  da.fire();
  return std::move(f);
}

auto PrototypeLeaderState::pollNewEntries() {
  auto stream = getStream();
  return _guardedData.doUnderLock([&](auto& data) {
    return stream->waitForIterator(data.nextWaitForIndex);
  });
}

void PrototypeLeaderState::handlePollResult(
    yaclib::Future<std::unique_ptr<EntryIterator>>&& pollFuture) {
  std::move(pollFuture)
      .ThenInline([weak = weak_from_this()](auto&& tryResult) {
        auto self = weak.lock();
        if (self == nullptr) {
          return;
        }

        auto result =
            basics::catchToResultT([&] { return std::move(tryResult).Ok(); });
        if (result.fail()) {
          THROW_ARANGO_EXCEPTION(result.result());
        }

        auto resolvePromises =
            self->_guardedData.getLockedGuard()->applyEntries(
                std::move(result.get()));
        resolvePromises.fire();

        self->handlePollResult(self->pollNewEntries());
      })
      .DetachInline([](auto&& tryResult) {
        if (!tryResult) {
          // Note that this means that this state silently stops working.
          // Production-grade implementations should probably find a better way
          // of handling this, but this is just for tests and experiments.
          try {
            std::ignore = std::move(tryResult).Ok();
          } catch (basics::Exception const& e) {
            LOG_TOPIC("0e2b8", ERR, Logger::REPLICATED_STATE)
                << "PrototypeLeaderState stops due to: [" << e.code() << "] "
                << e.message();
          } catch (...) {
            FATAL_ERROR_ABORT();
          }
        }
      });
}

auto PrototypeLeaderState::GuardedData::applyEntries(
    std::unique_ptr<EntryIterator> ptr) -> DeferredAction {
  if (didResign()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
  }
  auto toIndex = ptr->range().to;
  core->update(std::move(ptr));
  nextWaitForIndex = toIndex;

  if (core->flush()) {
    auto stream = self.getStream();
    stream->release(core->getLastPersistedIndex());
  }

  auto resolveQueue = std::make_unique<WaitForAppliedQueue>();

  auto const end = waitForAppliedQueue.lower_bound(nextWaitForIndex);
  for (auto it = waitForAppliedQueue.begin(); it != end;) {
    resolveQueue->insert(waitForAppliedQueue.extract(it++));
  }

  return DeferredAction([resolveQueue = std::move(resolveQueue)]() noexcept {
    for (auto& p : *resolveQueue) {
      TRI_ASSERT(p.second.Valid());
      std::move(p.second).Set();
    }
  });
}

auto PrototypeLeaderState::GuardedData::waitForApplied(LogIndex index)
    -> yaclib::Future<> {
  if (index < nextWaitForIndex) {
    return yaclib::MakeFuture();
  }
  auto [future, promise] = yaclib::MakeContract();
  waitForAppliedQueue.emplace(index, std::move(promise));
  return std::move(future);
}

void PrototypeLeaderState::onSnapshotCompleted() noexcept try {
  handlePollResult(pollNewEntries());
} catch (replicated_log::ParticipantResignedException const&) {
  // We're obsolete now, so just ignore this exception.
  LOG_TOPIC("5375a", TRACE, Logger::REPLICATED_STATE)
      << ARANGODB_PRETTY_FUNCTION
      << ": Caught ParticipantResignedException, will stop working.";
  return;
}

auto PrototypeLeaderState::waitForApplied(LogIndex waitForIndex)
    -> yaclib::Future<> {
  return _guardedData.getLockedGuard()->waitForApplied(waitForIndex);
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
