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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RequestStatistics.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <iomanip>
#include <thread>
#include <vector>

#include <boost/lockfree/queue.hpp>

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

namespace {
// initial amount of empty statistics items to be created in statisticsItems
constexpr size_t kInitialQueueSize = 64;

// protects statisticsItems
Mutex statisticsMutex;

// a container of RequestStatistics objects. the vector is populated initially
// with kInitialQueueSize items. It can grow at runtime. The addresses of
// objects in the vector can be stored in freeList, so the objects must not be
// destroyed if they are still in the free list. access to statisticsItems must
// be protected by statisticsMutex
std::vector<std::unique_ptr<RequestStatistics>> statisticsItems;

// a free list of RequestStatistics objects, not owning them. the free list
// is initially populated with kInitialQueueSize objects.
boost::lockfree::queue<RequestStatistics*> freeList;

// a list of finished (to-be-process) RequestStatistics objects, not owning
// them. this list will be initially empty
boost::lockfree::queue<RequestStatistics*> finishedList;

bool enqueueItem(boost::lockfree::queue<RequestStatistics*>& queue,
                 RequestStatistics* item) noexcept {
  int tries = 0;

  try {
    do {
      bool ok = queue.push(item);
      if (ok) {
        return true;
      }
      std::this_thread::yield();
    } while (++tries < 1000);
  } catch (...) {
  }

  // if for whatever reason the push operation fails, we will
  // have a RequestStatistics object in statisticsItems
  // that is not in the queue anymore. this item will be
  // allocated at program end normally, but it becomes useless
  // until then. this should be a very rare case though.
  // a RequestStatisticsItem is around 100 bytes big.

  return false;
}

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

void RequestStatistics::initialize() {
  MUTEX_LOCKER(guard, ::statisticsMutex);

  ::freeList.reserve(kInitialQueueSize * 2);
  ::finishedList.reserve(kInitialQueueSize * 2);

  ::statisticsItems.reserve(kInitialQueueSize);
  for (size_t i = 0; i < kInitialQueueSize; ++i) {
    // create a new RequestStatistics object on the heap
    ::statisticsItems.emplace_back(std::make_unique<RequestStatistics>());
    RequestStatistics* item = ::statisticsItems.back().get();
    // add its address to the freelist
    bool ok = ::enqueueItem(::freeList, item);
    if (!ok) {
      // for some reason we couldn't push the item to the queue. so there
      // is no further use for it.
      ::statisticsItems.pop_back();
    }
  }
}

size_t RequestStatistics::processAll() {
  RequestStatistics* statistics = nullptr;
  size_t count = 0;

  while (::finishedList.pop(statistics)) {
    if (statistics != nullptr) {
      process(statistics);
      ++count;
    }
  }

  return count;
}

RequestStatistics::Item RequestStatistics::acquire() noexcept {
  RequestStatistics* statistics = nullptr;

  // try the happy path first
  if (::freeList.pop(statistics)) {
    return Item{statistics};
  }

  // didn't have any items on the free list. now try the
  // expensive path
  try {
    auto cs = std::make_unique<RequestStatistics>();
    // store pointer for just-created item
    statistics = cs.get();

    MUTEX_LOCKER(guard, ::statisticsMutex);
    ::statisticsItems.emplace_back(std::move(cs));
  } catch (...) {
    statistics = nullptr;
  }

  return Item{statistics};
}

void RequestStatistics::release() noexcept {
  ::enqueueItem(::finishedList, this);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            static private methods
// -----------------------------------------------------------------------------

void RequestStatistics::process(RequestStatistics* statistics) {
  TRI_ASSERT(statistics != nullptr);

  {
    statistics::TotalRequests.incCounter();

    if (statistics->_async) {
      statistics::AsyncRequests.incCounter();
    }

    statistics::MethodRequests[(size_t)statistics->_requestType].incCounter();

    // check that the request was completely received and transmitted
    if (statistics->_readStart != 0.0 &&
        (statistics->_async || statistics->_writeEnd != 0.0)) {
      double totalTime;

      if (statistics->_async) {
        totalTime = statistics->_requestEnd - statistics->_readStart;
      } else {
        totalTime = statistics->_writeEnd - statistics->_readStart;
      }

      bool const isSuperuser = statistics->_superuser;
      if (isSuperuser) {
        statistics::TotalRequestsSuperuser.incCounter();
      } else {
        statistics::TotalRequestsUser.incCounter();
      }

      statistics::RequestFigures& figures =
          isSuperuser ? statistics::SuperuserRequestFigures
                      : statistics::UserRequestFigures;

      figures.totalTimeDistribution.addFigure(totalTime);

      double requestTime = statistics->_requestEnd - statistics->_requestStart;
      figures.requestTimeDistribution.addFigure(requestTime);

      double queueTime = 0.0;
      if (statistics->_queueStart != 0.0 && statistics->_queueEnd != 0.0) {
        queueTime = statistics->_queueEnd - statistics->_queueStart;
        figures.queueTimeDistribution.addFigure(queueTime);
      }

      double ioTime = totalTime - requestTime - queueTime;
      if (ioTime >= 0.0) {
        figures.ioTimeDistribution.addFigure(ioTime);
      }

      figures.bytesSentDistribution.addFigure(statistics->_sentBytes);
      figures.bytesReceivedDistribution.addFigure(statistics->_receivedBytes);
    }
  }

  // clear statistics
  statistics->reset();

  // put statistics item back onto the freelist
  ::enqueueItem(::freeList, statistics);
}

void RequestStatistics::getSnapshot(Snapshot& snapshot,
                                    stats::RequestStatisticsSource source) {
  statistics::RequestFigures& figures =
      source == stats::RequestStatisticsSource::USER
          ? statistics::UserRequestFigures
          : statistics::SuperuserRequestFigures;

  snapshot.totalTime = figures.totalTimeDistribution;
  snapshot.requestTime = figures.requestTimeDistribution;
  snapshot.queueTime = figures.queueTimeDistribution;
  snapshot.ioTime = figures.ioTimeDistribution;
  snapshot.bytesSent = figures.bytesSentDistribution;
  snapshot.bytesReceived = figures.bytesReceivedDistribution;

  if (source == stats::RequestStatisticsSource::ALL) {
    TRI_ASSERT(&figures == &statistics::SuperuserRequestFigures);
    snapshot.totalTime.add(
        statistics::UserRequestFigures.totalTimeDistribution);
    snapshot.requestTime.add(
        statistics::UserRequestFigures.requestTimeDistribution);
    snapshot.queueTime.add(
        statistics::UserRequestFigures.queueTimeDistribution);
    snapshot.ioTime.add(statistics::UserRequestFigures.ioTimeDistribution);
    snapshot.bytesSent.add(
        statistics::UserRequestFigures.bytesSentDistribution);
    snapshot.bytesReceived.add(
        statistics::UserRequestFigures.bytesReceivedDistribution);
  }
}

std::string RequestStatistics::Item::timingsCsv() const {
  TRI_ASSERT(_stat != nullptr);
  std::stringstream ss;

  ss << std::setprecision(9) << std::fixed << "read,"
     << (_stat->_readEnd - _stat->_readStart) << ",queue,"
     << (_stat->_queueEnd - _stat->_queueStart) << ",queue-size,"
     << _stat->_queueSize << ",request,"
     << (_stat->_requestEnd - _stat->_requestStart) << ",total,"
     << (StatisticsFeature::time() - _stat->_readStart);

  return ss.str();
}
