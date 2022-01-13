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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/debugging.h"
#include "Scheduler/Scheduler.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <atomic>

namespace arangodb {

class SoftShutdownTracker;

class SoftShutdownFeature final
    : public application_features::ApplicationFeature {
 public:
  SoftShutdownFeature(application_features::ApplicationServer& server);
  SoftShutdownTracker& softShutdownTracker() const {
    TRI_ASSERT(_softShutdownTracker != nullptr);
    return *_softShutdownTracker;
  }

  void beginShutdown() override final;

 private:
  std::shared_ptr<SoftShutdownTracker> _softShutdownTracker;
};

class SoftShutdownTracker
    : public std::enable_shared_from_this<SoftShutdownTracker> {
  // This is a class which tracks the proceedings in case of a soft shutdown.
  // Soft shutdown is a means to shut down a coordinator gracefully. It
  // means that certain things are allowed to run to completion but
  // new instances are no longer allowed to start. This class tracks
  // the number of these things in flight, so that the real shut down
  // can be triggered, once all tracked activity has ceased.
  // This class has customers, like the CursorRepositories of each vocbase,
  // these customers can on creation get a reference to an atomic counter,
  // which they can increase and decrease, the highest bit in the counter
  // is initially set, soft shutdown state is indicated when the highest
  // bit in each counter is reset. Then no new activity should be begun.

 private:
  application_features::ApplicationServer& _server;
  std::atomic<bool> _softShutdownOngoing;  // flag, if soft shutdown is ongoing
  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;  // used for soft shutdown checker
  std::function<void(bool)> _checkFunc;

 public:
  struct Status {
    uint64_t AQLcursors{0};
    uint64_t transactions{0};
    uint64_t pendingJobs{0};
    uint64_t doneJobs{0};
    uint64_t pregelConductors{0};
    uint64_t lowPrioOngoingRequests{0};
    uint64_t lowPrioQueuedRequests{0};

    bool const softShutdownOngoing;

    explicit Status(bool softShutdownOngoing)
        : softShutdownOngoing(softShutdownOngoing) {}

    bool allClear() const noexcept {
      return AQLcursors == 0 && transactions == 0 && pendingJobs == 0 &&
             doneJobs == 0 && lowPrioOngoingRequests == 0 &&
             lowPrioQueuedRequests == 0 && pregelConductors == 0;
    }
  };

  SoftShutdownTracker(application_features::ApplicationServer& server);
  ~SoftShutdownTracker(){};

  void initiateSoftShutdown();

  void cancelChecker();

  bool softShutdownOngoing() const {
    return _softShutdownOngoing.load(std::memory_order_relaxed);
  }

  std::atomic<bool> const* getSoftShutdownFlag() const {
    return &_softShutdownOngoing;
  }

  Status getStatus() const;

  void toVelocyPack(VPackBuilder& builder) {
    Status status = getStatus();
    toVelocyPack(builder, status);
  }

  static void toVelocyPack(VPackBuilder& builder, Status const& status);

 private:
  bool checkAndShutdownIfAllClear() const;
  // returns true if actual shutdown triggered
  void initiateActualShutdown() const;
};

}  // namespace arangodb
