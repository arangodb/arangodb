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
    -> futures::Future<Result> {
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
  return std::move(result);
}

auto PrototypeLeaderState::set(
    std::unordered_map<std::string, std::string> entries,
    PrototypeStateMethods::PrototypeWriteOptions options)
    -> futures::Future<LogIndex> {
  return executeOp(PrototypeLogEntry::createInsert(std::move(entries)),
                   options);
}

auto PrototypeLeaderState::compareExchange(
    std::string key, std::string oldValue, std::string newValue,
    PrototypeStateMethods::PrototypeWriteOptions options)
    -> futures::Future<ResultT<LogIndex>> {
  auto [f, da] = _guardedData.doUnderLock(
      [this, options, key = std::move(key), oldValue = std::move(oldValue),
       newValue = std::move(newValue)](auto& data) mutable
      -> std::pair<futures::Future<ResultT<LogIndex>>, DeferredAction> {
        if (data.didResign()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
        }

        if (!data.core->compare(key, oldValue)) {
          return std::make_pair(
              ResultT<LogIndex>::error(TRI_ERROR_ARANGO_CONFLICT),
              DeferredAction());
        }

        auto entry = PrototypeLogEntry::createCompareExchange(
            std::move(key), std::move(oldValue), std::move(newValue));
        auto [idx, action] = getStream()->insertDeferred(entry);
        data.core->applyToOngoingState(idx, entry);

        if (options.waitForApplied) {
          return std::make_pair(std::move(data.waitForApplied(idx))
                                    .thenValue([idx = idx](auto&&) {
                                      return ResultT<LogIndex>::success(idx);
                                    }),
                                std::move(action));
        }
        return std::make_pair(ResultT<LogIndex>::success(idx),
                              std::move(action));
      });
  da.fire();
  return std::move(f);
}

auto PrototypeLeaderState::remove(
    std::string key, PrototypeStateMethods::PrototypeWriteOptions options)
    -> futures::Future<LogIndex> {
  return remove(std::vector<std::string>{std::move(key)}, options);
}

auto PrototypeLeaderState::remove(
    std::vector<std::string> keys,
    PrototypeStateMethods::PrototypeWriteOptions options)
    -> futures::Future<LogIndex> {
  return executeOp(PrototypeLogEntry::createDelete(std::move(keys)), options);
}

auto PrototypeLeaderState::get(std::vector<std::string> keys,
                               LogIndex waitForApplied)
    -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>> {
  auto f = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }

    return data.waitForApplied(waitForApplied);
  });

  return std::move(f).thenValue(
      [keys = std::move(keys), weak = weak_from_this()](
          auto&&) -> ResultT<std::unordered_map<std::string, std::string>> {
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
    -> futures::Future<ResultT<std::optional<std::string>>> {
  auto f = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }

    return data.waitForApplied(waitForApplied);
  });

  return std::move(f).thenValue(
      [key = std::move(key),
       weak = weak_from_this()](auto&&) -> ResultT<std::optional<std::string>> {
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
    -> futures::Future<ResultT<std::unordered_map<std::string, std::string>>> {
  auto f = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE);
    }

    return data.waitForApplied(waitForIndex);
  });

  return std::move(f).thenValue(
      [weak = weak_from_this()](
          auto&&) -> ResultT<std::unordered_map<std::string, std::string>> {
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
    -> futures::Future<LogIndex> {
  auto [f, da] = _guardedData.doUnderLock(
      [&](auto& data) -> std::pair<futures::Future<LogIndex>, DeferredAction> {
        if (data.didResign()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
        }

        auto [idx, action] = getStream()->insertDeferred(entry);
        data.core->applyToOngoingState(idx, entry);

        if (options.waitForApplied) {
          return std::make_pair(
              std::move(data.waitForApplied(idx))
                  .thenValue([idx = idx](auto&&) { return idx; }),
              std::move(action));
        }
        return std::make_pair(idx, std::move(action));
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

        auto resolvePromises =
            self->_guardedData.getLockedGuard()->applyEntries(
                std::move(result.get()));
        resolvePromises.fire();

        self->handlePollResult(self->pollNewEntries());
      })
      .thenFinal([](futures::Try<futures::Unit> tryResult) {
        if (tryResult.hasException()) {
          // Note that this means that this state silently stops working.
          // Production-grade implementations should probably find a better way
          // of handling this, but this is just for tests and experiments.
          try {
            std::rethrow_exception(tryResult.exception());
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
      p.second.setValue();
    }
  });
}

auto PrototypeLeaderState::GuardedData::waitForApplied(LogIndex index)
    -> futures::Future<futures::Unit> {
  if (index < nextWaitForIndex) {
    return futures::Future<futures::Unit>{std::in_place};
  }
  auto it = waitForAppliedQueue.emplace(index, WaitForAppliedPromise{});
  auto f = it->second.getFuture();
  TRI_ASSERT(f.valid());
  return f;
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
    -> futures::Future<futures::Unit> {
  return _guardedData.getLockedGuard()->waitForApplied(waitForIndex);
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
