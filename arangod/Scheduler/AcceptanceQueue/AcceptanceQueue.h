////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Johann Listunov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>
#include <utility>

#include "GeneralServer/RequestLane.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {

class AcceptanceQueue {
 public:
  struct AcceptanceQueueWorkItemBase : Scheduler::WorkItemBase {
    explicit AcceptanceQueueWorkItemBase(AcceptanceQueue* queue,
                                         RequestLane const lane)
        : _queue(queue), _lane(lane) {}
    ~AcceptanceQueueWorkItemBase() override = default;

    bool queued() override {
      if (!_isQueued) {
        _isQueued = true;
      }
      return true;
    }

    bool dequeued() override {
      if (_isQueued) {
        _isQueued = false;
        _queue->trackWorkItemRemovedFromQueue(_lane);
        return true;
      }
      return false;
    }

    [[nodiscard]] RequestLane lane() const { return _lane; }

   private:
    AcceptanceQueue* _queue;
    RequestLane const _lane;
    bool _isQueued{false};
  };

  template<typename F>
  struct AcceptanceQueueWorkItem final : AcceptanceQueueWorkItemBase, F {
    explicit AcceptanceQueueWorkItem(F f, AcceptanceQueue* queue,
                                     RequestLane const lane)
        : AcceptanceQueueWorkItemBase(queue, lane),
          F(std::move(f)),
          _queue(queue),
          _logContext(LogContext::current()) {
      _queue->trackQueueItemSize(static_cast<int64_t>(sizeof(*this)));
    }
    ~AcceptanceQueueWorkItem() override {
      _queue->trackQueueItemSize(-static_cast<int64_t>(sizeof(*this)));
    }
    void invoke() override {
      LogContext::ScopedContext ctxGuard(
          _logContext, LogContext::ScopedContext::DontRestoreOldContext{});
      this->operator()();
    }

   private:
    AcceptanceQueue* _queue;
    LogContext _logContext;
  };

  virtual ~AcceptanceQueue() = default;

  template<typename F,
           std::enable_if_t<std::is_class_v<std::decay_t<F>>, int> = 0>
  [[nodiscard]] bool tryBoundedQueue(RequestLane const lane, F&& fn) noexcept {
    auto item = std::make_unique<AcceptanceQueueWorkItem<std::decay_t<F>>>(
        std::forward<F>(fn), this, lane);
    auto result = queueItem(std::move(item));
    return result;
  }

  /// @brief set the time it took for the last low prio item to be dequeued
  /// (time between queuing and dequeing) [ms]
  virtual void setLastLowPriorityDequeueTime(uint64_t time) = 0;

  /// @brief returns the last stored dequeue time [ms]
  [[nodiscard]] virtual uint64_t getLastLowPriorityDequeueTime()
      const noexcept = 0;

  [[nodiscard]] virtual Scheduler::QueueStatistics queueStatistics() const = 0;

  virtual bool start() = 0;
  virtual void shutdown() = 0;

  virtual void trackBeginOngoingLowPriorityTask() noexcept = 0;
  virtual void trackEndOngoingLowPriorityTask() noexcept = 0;

  virtual void trackCreateHandlerTask() noexcept = 0;
  virtual bool trackQueueTimeViolation(double requestedQueueTime) noexcept = 0;

  virtual void trackQueueItemSize(std::int64_t x) noexcept = 0;

  virtual bool trackWorkItemRemovedFromQueue(RequestLane lane) = 0;

  /// @brief get information about low prio queue:
  virtual std::pair<uint64_t, uint64_t> getNumberLowPrioOngoingAndQueued()
      const = 0;

  /// @brief approximate fill grade of the scheduler's queue (in %)
  virtual double approximateQueueFillGrade() const = 0;

  /// @brief fill grade of the scheduler's queue (in %) from which onwards
  /// the server is considered unavailable (because of overload)
  virtual double unavailabilityQueueFillGrade() const = 0;

 protected:
  [[nodiscard]] virtual bool queueItem(
      std::unique_ptr<AcceptanceQueueWorkItemBase> item) = 0;
};

}  // namespace arangodb
