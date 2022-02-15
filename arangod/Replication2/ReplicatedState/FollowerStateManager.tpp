////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "FollowerStateManager.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Basics/Exceptions.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
void FollowerStateManager<S>::applyEntries(
    std::unique_ptr<Iterator> iter) noexcept {
  TRI_ASSERT(state != nullptr);
  TRI_ASSERT(iter != nullptr);
  auto range = iter->range();
  LOG_TOPIC("3678e", TRACE, Logger::REPLICATED_STATE)
      << "apply entries in range " << range;
  updateInternalState(FollowerInternalState::kApplyRecentEntries, range);
  state->applyEntries(std::move(iter))
      .thenFinal([weak = this->weak_from_this(),
                  range](futures::Try<Result> tryResult) noexcept {
        auto self = weak.lock();
        if (self == nullptr) {
          LOG_TOPIC("a87aa", TRACE, Logger::REPLICATED_STATE)
              << "replicated state already gone";
          return;
        }
        try {
          auto& result = tryResult.get();
          if (result.ok()) {
            LOG_TOPIC("6e9bb", TRACE, Logger::REPLICATED_STATE)
                << "follower state applied range " << range;
            self->nextEntry = range.to;
            return self->pollNewEntries();
          } else {
            LOG_TOPIC("335f0", ERR, Logger::REPLICATED_STATE)
                << "follower failed to apply range " << range
                << " and returned error " << result;
          }
        } catch (std::exception const& e) {
          LOG_TOPIC("2fbae", ERR, Logger::REPLICATED_STATE)
              << "follower failed to apply range " << range
              << " with exception: " << e.what();
        } catch (...) {
          LOG_TOPIC("1a737", ERR, Logger::REPLICATED_STATE)
              << "follower failed to apply range " << range
              << " with unknown exception";
        }

        LOG_TOPIC("c89c8", DEBUG, Logger::REPLICATED_STATE)
            << "trigger retry for polling";
        // TODO retry
        std::abort();
      });
}

template<typename S>
void FollowerStateManager<S>::pollNewEntries() {
  TRI_ASSERT(stream != nullptr);
  LOG_TOPIC("a1462", TRACE, Logger::REPLICATED_STATE)
      << "polling for new entries nextEntry = " << nextEntry;
  updateInternalState(FollowerInternalState::kNothingToApply);
  stream->waitForIterator(nextEntry).thenFinal(
      [weak = this->weak_from_this()](
          futures::Try<std::unique_ptr<Iterator>> result) noexcept {
        auto self = weak.lock();
        if (self == nullptr) {
          return;
        }
        try {
          self->applyEntries(std::move(result).get());
        } catch (replicated_log::ParticipantResignedException const&) {
          if (auto ptr = self->parent.lock(); ptr) {
            LOG_TOPIC("654fb", TRACE, Logger::REPLICATED_STATE)
                << "forcing rebuild because participant resigned";
            ptr->forceRebuild();
          } else {
            LOG_TOPIC("15cb4", TRACE, Logger::REPLICATED_STATE)
                << "LogFollower resigned, but Replicated State already gone";
          }
        } catch (basics::Exception const& e) {
          LOG_TOPIC("f2188", FATAL, Logger::REPLICATED_STATE)
              << "waiting for leader ack failed with unexpected exception: "
              << e.message();
        }
      });
}

template<typename S>
void FollowerStateManager<S>::tryTransferSnapshot(
    std::shared_ptr<IReplicatedFollowerState<S>> hiddenState) {
  auto& leader = logFollower->getLeader();
  TRI_ASSERT(leader.has_value()) << "leader established it's leadership. There "
                                    "has to be a leader in the current term";
  auto f = hiddenState->acquireSnapshot(*leader, logFollower->getCommitIndex());
  std::move(f).thenFinal([weak = this->weak_from_this(), hiddenState](
                             futures::Try<Result>&& tryResult) noexcept {
    auto self = weak.lock();
    if (self == nullptr) {
      return;
    }
    try {
      auto& result = tryResult.get();
      if (result.ok()) {
        LOG_TOPIC("44d58", DEBUG, Logger::REPLICATED_STATE)
            << "snapshot transfer successfully completed";
        self->token->snapshot.updateStatus(SnapshotStatus::kCompleted);
        return self->startService(hiddenState);
      }
    } catch (...) {
    }
    TRI_ASSERT(false) << "error handling not implemented";
    FATAL_ERROR_EXIT();
  });
}

template<typename S>
void FollowerStateManager<S>::checkSnapshot(
    std::shared_ptr<IReplicatedFollowerState<S>> hiddenState) {
  LOG_TOPIC("aee5b", TRACE, Logger::REPLICATED_STATE)
      << "snapshot status is " << token->snapshot.status << ", generation is "
      << token->generation;
  bool needsSnapshot = token->snapshot.status != SnapshotStatus::kCompleted;
  if (needsSnapshot) {
    LOG_TOPIC("3d0fc", DEBUG, Logger::REPLICATED_STATE)
        << "new snapshot is required";
    tryTransferSnapshot(hiddenState);
  } else {
    LOG_TOPIC("9cd75", DEBUG, Logger::REPLICATED_STATE)
        << "no snapshot transfer required";
    startService(hiddenState);
  }
}

template<typename S>
void FollowerStateManager<S>::startService(
    std::shared_ptr<IReplicatedFollowerState<S>> hiddenState) {
  LOG_TOPIC("26c55", TRACE, Logger::REPLICATED_STATE)
      << "starting service as follower";
  state = hiddenState;
  state->_stream = stream;
  pollNewEntries();
}

template<typename S>
void FollowerStateManager<S>::ingestLogData() {
  updateInternalState(FollowerInternalState::kTransferSnapshot);
  auto demux = Demultiplexer::construct(logFollower);
  demux->listen();
  stream = demux->template getStreamById<1>();

  LOG_TOPIC("1d843", TRACE, Logger::REPLICATED_STATE)
      << "creating follower state instance";
  auto hiddenState = factory->constructFollower(std::move(core));

  LOG_TOPIC("ea777", TRACE, Logger::REPLICATED_STATE)
      << "check if new snapshot is required";
  checkSnapshot(hiddenState);
}

template<typename S>
void FollowerStateManager<S>::awaitLeaderShip() {
  updateInternalState(FollowerInternalState::kWaitForLeaderConfirmation);
  try {
    logFollower->waitForLeaderAcked().thenFinal(
        [weak = this->weak_from_this()](
            futures::Try<replicated_log::WaitForResult>&& result) noexcept {
          auto self = weak.lock();
          if (self == nullptr) {
            return;
          }
          try {
            try {
              result.throwIfFailed();
              LOG_TOPIC("53ba1", TRACE, Logger::REPLICATED_STATE)
                  << "leadership acknowledged - ingesting log data";
              self->ingestLogData();
            } catch (replicated_log::ParticipantResignedException const&) {
              if (auto ptr = self->parent.lock(); ptr) {
                LOG_TOPIC("79e37", DEBUG, Logger::REPLICATED_STATE)
                    << "participant resigned before leadership - force rebuild";
                ptr->forceRebuild();
              } else {
                LOG_TOPIC("15cb4", DEBUG, Logger::REPLICATED_STATE)
                    << "LogFollower resigned, but Replicated State already "
                       "gone";
              }
            } catch (basics::Exception const& e) {
              LOG_TOPIC("f2188", FATAL, Logger::REPLICATED_STATE)
                  << "waiting for leader ack failed with unexpected exception: "
                  << e.message();
              FATAL_ERROR_EXIT();
            }
          } catch (std::exception const& ex) {
            LOG_TOPIC("c7787", FATAL, Logger::REPLICATED_STATE)
                << "waiting for leader ack failed with unexpected exception: "
                << ex.what();
            FATAL_ERROR_EXIT();
          } catch (...) {
            LOG_TOPIC("43456", FATAL, Logger::REPLICATED_STATE)
                << "waiting for leader ack failed with unexpected exception";
            FATAL_ERROR_EXIT();
          }
        });
  } catch (replicated_log::ParticipantResignedException const&) {
    if (auto p = parent.lock(); p) {
      LOG_TOPIC("1cb5c", TRACE, Logger::REPLICATED_STATE)
          << "forcing rebuild because participant resigned";
      return p->forceRebuild();
    } else {
      LOG_TOPIC("a62cb", TRACE, Logger::REPLICATED_STATE)
          << "replicated state already gone";
    }
  }
}

template<typename S>
void FollowerStateManager<S>::run() {
  // 1. wait for log follower to have committed at least one entry
  // 2. receive a new snapshot (if required)
  //    if (old_generation != new_generation || snapshot_status != Completed)
  // 3. start polling for new entries
  awaitLeaderShip();
}

template<typename S>
FollowerStateManager<S>::FollowerStateManager(
    std::shared_ptr<ReplicatedStateBase> parent,
    std::shared_ptr<replicated_log::ILogFollower> logFollower,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token,
    std::shared_ptr<Factory> factory) noexcept
    : parent(parent),
      logFollower(std::move(logFollower)),
      core(std::move(core)),
      token(std::move(token)),
      factory(std::move(factory)) {
  TRI_ASSERT(this->core != nullptr);
  TRI_ASSERT(this->token != nullptr);
}

template<typename S>
auto FollowerStateManager<S>::getStatus() const -> StateStatus {
  if (_didResign) {
    TRI_ASSERT(core == nullptr && token == nullptr);
    throw replicated_log::ParticipantResignedException(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE);
  } else {
    // Note that `this->core` is passed into the state when the follower is
    // started (more particularly, when the follower state is created), but
    // `this->state` is only set after replaying the log has finished.
    // Thus both can be null here.
    TRI_ASSERT(token != nullptr);
  }
  FollowerStatus status;
  status.managerState.state = internalState;
  status.managerState.lastChange = lastInternalStateChange;
  status.managerState.detail = std::nullopt;
  status.generation = token->generation;
  status.snapshot = token->snapshot;
  return StateStatus{.variant = std::move(status)};
}

template<typename S>
auto FollowerStateManager<S>::getFollowerState() const
    -> std::shared_ptr<IReplicatedFollowerState<S>> {
  return state;
}

template<typename S>
auto FollowerStateManager<S>::resign() && noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<ReplicatedStateToken>> {
  LOG_TOPIC("63622", TRACE, Logger::REPLICATED_STATE)
      << "Follower manager resigning";
  auto core = std::invoke([&] {
    if (state != nullptr) {
      TRI_ASSERT(this->core == nullptr);
      return std::move(*state).resign();
    } else {
      return std::move(this->core);
    }
  });
  TRI_ASSERT(core != nullptr);
  TRI_ASSERT(token != nullptr);
  TRI_ASSERT(!_didResign);
  _didResign = true;
  return {std::move(core), std::move(token)};
}
}  // namespace arangodb::replication2::replicated_state
