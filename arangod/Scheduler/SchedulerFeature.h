////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_SCHEDULER_FEATURE_H
#define ARANGOD_SCHEDULER_SCHEDULER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/asio_ns.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SupervisedScheduler.h"

namespace arangodb {

class SchedulerFeature final : public application_features::ApplicationFeature {
 public:
  static SupervisedScheduler* SCHEDULER;

  explicit SchedulerFeature(application_features::ApplicationServer& server);
  ~SchedulerFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void unprepare() override final;

 private:
  uint64_t _nrMinimalThreads = 4;
  uint64_t _nrMaximalThreads = 0;
  uint64_t _queueSize = 4096;
  uint64_t _fifo1Size = 4096;
  uint64_t _fifo2Size = 4096;
  uint64_t _fifo3Size = 4096;
  double _ongoingLowPriorityMultiplier = 4.0;
  double _unavailabilityQueueFillGrade = 0.75;

  std::unique_ptr<Scheduler> _scheduler;

  // -------------------------------------------------------------------------
  // UNRELATED SECTION STARTS HERE: Signals and other things crept into Sched
  // -------------------------------------------------------------------------

 public:
  void buildControlCHandler();
  void buildHangupHandler();

 private:
  void signalStuffInit();
  void signalStuffDeinit();

  std::function<void(const asio_ns::error_code&, int)> _signalHandler;
  std::function<void(const asio_ns::error_code&, int)> _exitHandler;
  std::shared_ptr<asio_ns::signal_set> _exitSignals;

  std::function<void(const asio_ns::error_code&, int)> _hangupHandler;
  std::shared_ptr<asio_ns::signal_set> _hangupSignals;
};

// This can cause ABI problems.
/*template<typename T, template<typename> typename Fut>
struct futures::scheduler_addition<T, Fut, futures::arangodb_tag> {
  auto reschedule(RequestLane lane = RequestLane::CLUSTER_INTERNAL) {
    auto&& [f, p] = futures::makePromise<T>();
    std::move(self()).finally([lane, p = std::move(p)](T&& t) noexcept {
      std::ignore = SchedulerFeature::SCHEDULER->queue(lane, [t = std::move(t),
          p = std::move(p)]() mutable noexcept {
        std::move(p).fulfill(std::move(t));
      });
      // WHAT HAPPENS WHEN queue fails?
      // Then the promise p is abandoned and a ARANGO_EXPCETION with error code
      // TRI_ERROR_PROMISE_ABANDONED is thrown into the promise.
      // However, it is executed in this thread.
    });
  }

 private:
  using future_type = Fut<T>;
  future_type& self() { return static_cast<future_type&>(*this); }
};*/

}  // namespace arangodb

#endif
