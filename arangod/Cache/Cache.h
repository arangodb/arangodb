////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_CACHE_H
#define ARANGODB_CACHE_CACHE_H

#include "Basics/Common.h"
#include "Cache/CachedValue.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Manager.h"
#include "Cache/ManagerTasks.h"
#include "Cache/Metadata.h"
#include "Cache/State.h"

#include <stdint.h>
#include <list>
#include <memory>

namespace arangodb {
namespace cache {

class PlainCache;          // forward declaration
class TransactionalCache;  // forward declaration

////////////////////////////////////////////////////////////////////////////////
/// @brief The common structure of all caches managed by Manager.
///
/// Any pure virtual functions are documented in derived classes implementing
/// them.
////////////////////////////////////////////////////////////////////////////////
class Cache : public std::enable_shared_from_this<Cache> {
 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief A dummy class to restrict constructor access.
  //////////////////////////////////////////////////////////////////////////////
  class ConstructionGuard {
   private:
    ConstructionGuard();
    friend class PlainCache;
    friend class TransactionalCache;
  };

 public:
  typedef FrequencyBuffer<uint8_t> StatBuffer;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief A helper class for managing CachedValue lifecycles.
  ///
  /// Returned to clients by Cache::find. Clients must destroy the Finding
  /// object within a short period of time to allow proper memory management
  /// within the cache system. If the underlying value needs to be retained for
  /// any significant period of time, it must be copied so that the finding
  /// object may be destroyed.
  //////////////////////////////////////////////////////////////////////////////
  class Finding {
   public:
    Finding(CachedValue* v);
    Finding(Finding const& other);
    Finding(Finding&& other);
    Finding& operator=(Finding const& other);
    Finding& operator=(Finding&& other);
    ~Finding();

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Changes the underlying CachedValue pointer.
    ////////////////////////////////////////////////////////////////////////////
    void reset(CachedValue* v);

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Specifies whether the value was found. If not, value is nullptr.
    ////////////////////////////////////////////////////////////////////////////
    bool found() const;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Returns the underlying value pointer.
    ////////////////////////////////////////////////////////////////////////////
    CachedValue const* value() const;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a copy of the underlying value and returns a pointer.
    ////////////////////////////////////////////////////////////////////////////
    CachedValue* copy() const;

   private:
    CachedValue* _value;
  };

 public:
  Cache(ConstructionGuard guard, Manager* manager,
        Manager::MetadataItr metadata, bool allowGrowth,
        bool enableWindowedStats);
  virtual ~Cache() = default;

  // primary functionality; documented in derived classes
  virtual Finding find(void const* key, uint32_t keySize) = 0;
  virtual bool insert(CachedValue* value) = 0;
  virtual bool remove(void const* key, uint32_t keySize) = 0;
  virtual bool blacklist(void const* key, uint32_t keySize) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the limit on memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t limit();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the current memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t usage();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the cache hit-rates.
  ///
  /// The first return value is the lifetime hit-rate for this cache. The second
  /// is the "windowed" hit-rate, that is the hit-rate when only considering the
  /// past several thousand find operations. If windowed stats are not enabled,
  /// this will be NaN.
  //////////////////////////////////////////////////////////////////////////////
  std::pair<double, double> hitRates();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Disallows the cache from requesting to be resized when it runs out
  /// of space.
  //////////////////////////////////////////////////////////////////////////////
  void disableGrowth();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Allows the cache from requesting to be resized when it runs out of
  /// space.
  //////////////////////////////////////////////////////////////////////////////
  void enableGrowth();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Request that this cache be given a new limit as specified.
  ///
  /// If there is enough free memory globally and the cache is not currently
  /// resizing, the request should be granted. If downsizing the cache, it may
  /// need to free some memory, which will be done in an asynchronous task.
  //////////////////////////////////////////////////////////////////////////////
  bool resize(uint64_t requestedLimit = 0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache is currently in the process of resizing.
  //////////////////////////////////////////////////////////////////////////////
  bool isResizing();

 protected:
  State _state;

  // whether to allow the cache to resize larger when it fills
  bool _allowGrowth;

  // structures to handle statistics
  enum class Stat : uint8_t {
    findHit = 1,
    findMiss = 2,
    insertEviction = 3,
    insertNoEviction = 4
  };
  static uint64_t _evictionStatsCapacity;
  StatBuffer _evictionStats;
  std::atomic<uint64_t> _insertionCount;
  static uint64_t _findStatsCapacity;
  bool _enableWindowedStats;
  std::unique_ptr<StatBuffer> _findStats;
  std::atomic<uint64_t> _findHits;
  std::atomic<uint64_t> _findMisses;

  // allow communication with manager
  Manager* _manager;
  Manager::MetadataItr _metadata;

  // keep track of number of open operations to allow clean shutdown
  std::atomic<uint32_t> _openOperations;

  // times to wait until requesting is allowed again
  Manager::time_point _migrateRequestTime;
  Manager::time_point _resizeRequestTime;
  bool _lastResizeRequestStatus;

  // friend class manager and tasks
  friend class FreeMemoryTask;
  friend class Manager;
  friend class MigrateTask;

 protected:
  // shutdown cache and let its memory be reclaimed
  static void destroy(std::shared_ptr<Cache> cache);

  bool isOperational() const;
  void startOperation();
  void endOperation();

  bool isMigrating() const;
  bool requestResize(uint64_t requestedLimit = 0, bool internal = true);
  void requestMigrate(uint32_t requestedLogSize = 0);

  void freeValue(CachedValue* value);
  bool reclaimMemory(uint64_t size);
  virtual void clearTables() = 0;

  uint32_t hashKey(void const* key, uint32_t keySize) const;
  void recordStat(Cache::Stat stat);

  // management
  Manager::MetadataItr& metadata();
  void beginShutdown();
  void shutdown();
  bool canResize();
  bool canMigrate();
  virtual bool freeMemory() = 0;
  virtual bool migrate() = 0;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
