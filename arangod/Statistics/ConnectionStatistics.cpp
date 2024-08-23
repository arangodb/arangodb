////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ConnectionStatistics.h"

#include "Rest/CommonDefines.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/lockfree/queue.hpp>

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

namespace {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
// this variable is only used in maintainer mode, to check that we are
// only acquiring memory for statistics in case they are enabled.
bool statisticsEnabled = false;
#endif

std::atomic<uint64_t> memoryUsage = 0;

// initial amount of empty statistics items to be created in statisticsItems
constexpr size_t kInitialQueueSize = 32;

// protects statisticsItems
std::mutex statisticsMutex;

// a container of ConnectionStatistics objects. the vector is populated
// initially with kInitialQueueSize items. It can grow at runtime. The addresses
// of objects in the vector can be stored in freeList, so the objects must not
// be destroyed if they are still in the free list. access to statisticsItems
// must be protected by statisticsMutex
std::vector<std::unique_ptr<ConnectionStatistics>> statisticsItems;

// a free list of ConnectionStatistics objects, not owning them. the free list
// is initially populated with kInitialQueueSize objects.
static boost::lockfree::queue<ConnectionStatistics*> freeList;

bool enqueueItem(ConnectionStatistics* item) noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(statisticsEnabled);
#endif

  int tries = 0;

  try {
    do {
      bool ok = freeList.push(item);
      if (ok) {
        return true;
      }
      std::this_thread::yield();
    } while (++tries < 1000);
  } catch (...) {
  }

  TRI_ASSERT(false);
  // if for whatever reason the push operation fails, we will
  // have a ConnectionStatistics object in statisticsItems
  // that is not in the queue anymore. this item will be
  // allocated at program end normally, but it becomes useless
  // until then. this should be a very rare case though.
  // each ConnectionStatistics object 24 bytes big.

  return false;
}

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

void ConnectionStatistics::Item::SET_HTTP() {
  if (_stat != nullptr) {
    _stat->_http = true;

    statistics::HttpConnections.incCounter();
  }
}

uint64_t ConnectionStatistics::memoryUsage() noexcept {
  return ::memoryUsage.load(std::memory_order_relaxed);
}

void ConnectionStatistics::initialize() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(!statisticsEnabled);
  statisticsEnabled = true;
#endif

  std::lock_guard guard{::statisticsMutex};

  ::freeList.reserve(kInitialQueueSize * 2);

  ::statisticsItems.reserve(kInitialQueueSize);
  for (size_t i = 0; i < kInitialQueueSize; ++i) {
    // create a new ConnectionStatistics object on the heap
    ::statisticsItems.emplace_back(std::make_unique<ConnectionStatistics>());
    // add its address to the freelist
    bool ok = ::enqueueItem(::statisticsItems.back().get());
    if (!ok) {
      // for some reason we couldn't push the item to the queue. so there
      // is no further use for it.
      ::statisticsItems.pop_back();
    }
  }

  ::memoryUsage.fetch_add(::statisticsItems.size() *
                              (sizeof(decltype(::statisticsItems)::value_type) +
                               sizeof(ConnectionStatistics)),
                          std::memory_order_relaxed);
}

ConnectionStatistics::Item ConnectionStatistics::acquire() noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(statisticsEnabled);
#endif

  ConnectionStatistics* statistics = nullptr;

  // try the happy path first
  if (!::freeList.pop(statistics)) {
    // didn't have any items on the free list. now try the
    // expensive path
    try {
      auto cs = std::make_unique<ConnectionStatistics>();
      // store pointer for just-created item
      statistics = cs.get();

      {
        std::lock_guard guard{::statisticsMutex};
        ::statisticsItems.emplace_back(std::move(cs));
      }

      ::memoryUsage.fetch_add(sizeof(decltype(::statisticsItems)::value_type) +
                                  sizeof(ConnectionStatistics),
                              std::memory_order_relaxed);
    } catch (...) {
      statistics = nullptr;
    }
  }

  return Item{statistics};
}

void ConnectionStatistics::release() noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(statisticsEnabled);
#endif

  if (_http) {
    statistics::HttpConnections.decCounter();
  }

  if (_connStart != 0.0 && _connEnd != 0.0) {
    double totalTime = _connEnd - _connStart;
    statistics::ConnectionTimeDistribution.addFigure(totalTime);
  }

  // clear statistics
  reset();

  // put statistics item back onto the freelist
  enqueueItem(this);
}

void ConnectionStatistics::getSnapshot(Snapshot& snapshot) {
  snapshot.httpConnections = statistics::HttpConnections;
  snapshot.totalRequests = statistics::TotalRequests;
  snapshot.totalRequestsSuperuser = statistics::TotalRequestsSuperuser;
  snapshot.totalRequestsUser = statistics::TotalRequestsUser;
  snapshot.methodRequests = statistics::MethodRequests;
  snapshot.asyncRequests = statistics::AsyncRequests;
  snapshot.connectionTime = statistics::ConnectionTimeDistribution;
}
