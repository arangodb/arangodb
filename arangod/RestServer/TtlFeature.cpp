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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "TtlFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/system-functions.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::options;
        
namespace {
// the AQL query to remove documents
std::string const removeQuery("/*ttl cleanup*/ FOR doc IN @@collection OPTIONS { forceIndexHint: true, indexHint: @indexHint } FILTER doc.@indexAttribute >= 0 && doc.@indexAttribute <= @stamp SORT doc.@indexAttribute LIMIT @limit REMOVE doc IN @@collection OPTIONS { ignoreErrors: true }");
}

namespace arangodb {

TtlStatistics& TtlStatistics::operator+=(VPackSlice const& other) {
  if (!other.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "expecting object for statistics");
  }

  if (other.hasKey("runs")) {
    runs += other.get("runs").getNumericValue<uint64_t>();
  }
  if (other.hasKey("documentsRemoved")) {
    documentsRemoved += other.get("documentsRemoved").getNumericValue<uint64_t>();
  }
  if (other.hasKey("limitReached")) {
    limitReached += other.get("limitReached").getNumericValue<uint64_t>();
  }
  return *this;
}
  
void TtlStatistics::toVelocyPack(VPackBuilder& builder) const {
  builder.openObject();
  builder.add("runs", VPackValue(runs));
  builder.add("documentsRemoved", VPackValue(documentsRemoved));
  builder.add("limitReached", VPackValue(limitReached));
  builder.close();
}

void TtlProperties::toVelocyPack(VPackBuilder& builder, bool isActive) const {
  builder.openObject();
  builder.add("frequency", VPackValue(frequency)); 
  builder.add("maxTotalRemoves", VPackValue(maxTotalRemoves));
  builder.add("maxCollectionRemoves", VPackValue(maxCollectionRemoves));
  // this attribute is hard-coded to false since v3.8, and will be removed later
  builder.add("onlyLoadedCollections", VPackValue(false));
  builder.add("active", VPackValue(isActive));
  builder.close();
}

Result TtlProperties::fromVelocyPack(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "expecting object for properties");
  }

  try {
    uint64_t frequency = this->frequency;
    uint64_t maxTotalRemoves = this->maxTotalRemoves;
    uint64_t maxCollectionRemoves = this->maxCollectionRemoves;

    if (slice.hasKey("frequency")) {
      if (!slice.get("frequency").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER, "expecting numeric value for frequency");
      }
      frequency = slice.get("frequency").getNumericValue<uint64_t>();
      if (frequency < TtlProperties::minFrequency) {
        return Result(TRI_ERROR_BAD_PARAMETER, "too low value for frequency");
      }
    }
    if (slice.hasKey("maxTotalRemoves")) {
      if (!slice.get("maxTotalRemoves").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER, "expecting numeric value for maxTotalRemoves");
      }
      maxTotalRemoves = slice.get("maxTotalRemoves").getNumericValue<uint64_t>();
    }
    if (slice.hasKey("maxCollectionRemoves")) {
      if (!slice.get("maxCollectionRemoves").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER, "expecting numeric value for maxCollectionRemoves");
      }
      maxCollectionRemoves = slice.get("maxCollectionRemoves").getNumericValue<uint64_t>();
    }

    this->frequency = frequency;
    this->maxTotalRemoves = maxTotalRemoves;
    this->maxCollectionRemoves = maxCollectionRemoves;

    return Result();
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  }
}

class TtlThread final : public Thread {
 public:
  explicit TtlThread(application_features::ApplicationServer& server, TtlFeature& ttlFeature)
      : Thread(server, "TTL"), _ttlFeature(ttlFeature), _working(false) {}

  ~TtlThread() { shutdown(); }

  void beginShutdown() override {
    Thread::beginShutdown();

    // wake up the thread that may be waiting in run()
    wakeup();
  }
  
  void wakeup() {
    // wake up the thread that may be waiting in run()
    CONDITION_LOCKER(guard, _condition);
    guard.signal();
  }

  bool isCurrentlyWorking() const {
    return _working.load();
  }

  /// @brief frequency is specified in milliseconds
  void setNextStart(uint64_t frequency) {
    CONDITION_LOCKER(guard, _condition);
    _nextStart = std::chrono::steady_clock::now() + std::chrono::milliseconds(frequency);
  }

 protected:
  void run() override {
    TtlProperties properties = _ttlFeature.properties();
    setNextStart(properties.frequency); 

    LOG_TOPIC("c2be7", TRACE, Logger::TTL) << "starting TTL background thread with interval " << properties.frequency << " milliseconds, max removals per run: " << properties.maxTotalRemoves << ", max removals per collection per run " << properties.maxCollectionRemoves;
    
    while (true) {
      auto now = std::chrono::steady_clock::now();

      while (now < _nextStart) {
        if (isStopping()) {
          // server shutdown
          return;
        }

        // wait for our start...
        CONDITION_LOCKER(guard, _condition);
        
        guard.wait(std::chrono::microseconds(std::chrono::duration_cast<std::chrono::microseconds>(_nextStart - now)));
        now = std::chrono::steady_clock::now();
      }
    
      if (isStopping()) {
        // server shutdown
        return;
      }
      
      // properties may have changed... update them
      properties = _ttlFeature.properties();
      setNextStart(properties.frequency); 

      try {
        TtlStatistics stats;
        // note: work() will do nothing if isActive() is false
        work(stats, properties);

        // merge stats
        _ttlFeature.updateStats(stats);
      } catch (std::exception const& ex) {
        LOG_TOPIC("6d28a", WARN, Logger::TTL) << "caught exception in TTL background thread: " << ex.what();
      } catch (...) {
        LOG_TOPIC("44aa8", WARN, Logger::TTL) << "caught unknown exception in TTL background thread";
      }
    }
  }

 private:
  /// @brief whether or not the background thread shall continue working
  bool isActive() const {
    return _ttlFeature.isActive() && !isStopping() && !ServerState::readOnly();
  }

  void work(TtlStatistics& stats, TtlProperties const& properties) {
    if (!isActive()) {
      return;
    }
    
    // mark ourselves as busy
    _working = true;
    auto guard = scopeGuard([this]() { _working = false; });
  
    LOG_TOPIC("139af", TRACE, Logger::TTL) << "ttl thread work()";

    stats.runs++;

    double const stamp = TRI_microtime();
    uint64_t limitLeft = properties.maxTotalRemoves; 
    
    // iterate over all databases
    auto& db = _server.getFeature<DatabaseFeature>();
    for (auto const& name : db.getDatabaseNames()) {
      if (!isActive()) {
        // feature deactivated (for example, due to running on current follower in
        // active failover setup)
        return;
      }

      TRI_vocbase_t* vocbase = db.useDatabase(name);

      if (vocbase == nullptr) {
        continue;
      }
          
      // make sure we decrease the reference counter later
      TRI_DEFER(vocbase->release());
      
      LOG_TOPIC("ec905", TRACE, Logger::TTL) << "TTL thread going to process database '" << vocbase->name() << "'";

      std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections = vocbase->collections(false);

      for (auto const& collection : collections) {
        if (!isActive()) {
          // feature deactivated (for example, due to running on current follower in
          // active failover setup)
          return;
        }

        if (ServerState::instance()->isDBServer() &&
            !collection->followers()->getLeader().empty()) {
          // we are a follower for this shard. do not remove any data here, but
          // let the leader carry out the removal and replicate it
          continue;
        }

        std::vector<std::shared_ptr<Index>> indexes = collection->getIndexes();

        for (auto const& index : indexes) {
          // we are only interested in collections with TTL indexes
          if (index->type() != Index::TRI_IDX_TYPE_TTL_INDEX) {
            continue;
          }
      
          // serialize the index description so we can read the "expireAfter" attribute
          _builder.clear(); 
          index->toVelocyPack(_builder, Index::makeFlags());
          VPackSlice ea = _builder.slice().get(StaticStrings::IndexExpireAfter);
          if (!ea.isNumber()) {
            // index description somehow invalid
            continue;
          }

          double expireAfter = ea.getNumericValue<double>();
          LOG_TOPIC("5cca5", DEBUG, Logger::TTL) << "TTL thread going to work for collection '" << collection->name() << "', expireAfter: " << Logger::FIXED(expireAfter, 0) << ", stamp: " << (stamp - expireAfter) << ", limit: " << std::min(properties.maxCollectionRemoves, limitLeft);

          auto bindVars = std::make_shared<VPackBuilder>();
          bindVars->openObject();
          bindVars->add("indexHint", VPackValue(index->name()));
          bindVars->add("@collection", VPackValue(collection->name()));
          bindVars->add(VPackValue("indexAttribute"));
          bindVars->openArray();
          for (auto const& it : index->fields()[0]) {
            bindVars->add(VPackValue(it.name));
          }
          bindVars->close();
          bindVars->add("stamp", VPackValue(stamp - expireAfter));
          bindVars->add("limit", VPackValue(std::min(properties.maxCollectionRemoves, limitLeft)));
          bindVars->close();

          aql::Query query(transaction::StandaloneContext::Create(*vocbase),
                           aql::QueryString(::removeQuery), bindVars, nullptr);
          query.collections().add(collection->name(), AccessMode::Type::WRITE, aql::Collection::Hint::Shard);
          aql::QueryResult queryResult = query.executeSync();

          if (queryResult.result.fail()) {
            // we can probably live with an error here...
            // the thread will try to remove the documents again on next iteration
            if (!queryResult.result.is(TRI_ERROR_ARANGO_READ_ONLY) &&
                !queryResult.result.is(TRI_ERROR_ARANGO_CONFLICT) &&
                !queryResult.result.is(TRI_ERROR_LOCKED) &&
                !queryResult.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
              LOG_TOPIC("08300", WARN, Logger::TTL) << "error during TTL document removal for collection '" << collection->name() << "': " << queryResult.result.errorMessage();
            }
          } else {
            auto extra = queryResult.extra;
            if (extra != nullptr) {
              VPackSlice v = extra->slice().get("stats");
              if (v.isObject()) {
                v = v.get("writesExecuted");
                if (v.isNumber()) {
                  uint64_t removed = v.getNumericValue<uint64_t>();
                  stats.documentsRemoved += removed;
                  if (removed > 0) {
                    LOG_TOPIC("2455e", DEBUG, Logger::TTL) << "TTL thread removed " << removed << " documents for collection '" << collection->name() << "'";
                    if (limitLeft >= removed) {
                      limitLeft -= removed;
                    } else { 
                      limitLeft = 0;
                    }
                  }
                }
              }
            }
          }

          // there can only be one TTL index per collection, so we can abort the loop here
          break; 
        }

        if (limitLeft == 0) {
          // removed as much as we are allowed to. now stop and remove more in next iteration
          ++stats.limitReached;
          return;
        }
    
        if (isStopping()) {
          // server has been stopped, so abort our loop(s)
          return;
        }
      }
    }
  }

 private:
  TtlFeature& _ttlFeature;

  arangodb::basics::ConditionVariable _condition;
  
  /// @brief next time the thread should run
  std::chrono::time_point<std::chrono::steady_clock> _nextStart;

  /// @brief a builder object we reuse to save a few memory allocations
  VPackBuilder _builder;

  /// @brief set to true while the TTL thread is actually performing deletions,
  /// false otherwise
  std::atomic<bool> _working;
};

}

TtlFeature::TtlFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "Ttl"), 
      _allowRunning(true),
      _active(true) {
  startsAfter<application_features::DatabaseFeaturePhase>();
  startsAfter<application_features::ServerFeaturePhase>();
}

TtlFeature::~TtlFeature() {
  shutdownThread();
}

void TtlFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("ttl", "TTL options");

  options->addOption("ttl.frequency", "frequency (in milliseconds) for the TTL background thread invocation. "
                     "a value of 0 turns the TTL background thread off entirely",
                     new UInt64Parameter(&_properties.frequency));
  
  options->addOption("ttl.max-total-removes", "maximum number of documents to remove per invocation of the TTL thread",
                     new UInt64Parameter(&_properties.maxTotalRemoves));
  
  options->addOption("ttl.max-collection-removes", "maximum number of documents to remove per collection",
                     new UInt64Parameter(&_properties.maxCollectionRemoves));

  // the following option was obsoleted in 3.8
  options->addObsoleteOption("ttl.only-loaded-collection", "only consider already loaded collections for removal", false);
}

void TtlFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_properties.maxTotalRemoves == 0) {
    LOG_TOPIC("1e299", FATAL, arangodb::Logger::STARTUP)
        << "invalid value for '--ttl.max-total-removes'.";
    FATAL_ERROR_EXIT();
  }

  if (_properties.maxCollectionRemoves == 0) {
    LOG_TOPIC("2ab82", FATAL, arangodb::Logger::STARTUP)
        << "invalid value for '--ttl.max-collection-removes'.";
    FATAL_ERROR_EXIT();
  }

  if (_properties.frequency < TtlProperties::minFrequency) {
    LOG_TOPIC("ea696", FATAL, arangodb::Logger::STARTUP)
        << "too low value for '--ttl.frequency'.";
    FATAL_ERROR_EXIT();
  }
}

void TtlFeature::start() {
  // the thread will not run on a coordinator or an agency node,
  // just locally on DB servers or single servers
  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isAgent()) {
    LOG_TOPIC("e94bb", DEBUG, Logger::TTL) << "turning off TTL feature because of coordinator / agency";
    return;
  }

  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();
  if (databaseFeature.checkVersion() || databaseFeature.upgrade()) {
    LOG_TOPIC("5614a", DEBUG, Logger::TTL) << "turning off TTL feature because of version checking or upgrade procedure";
    return;
  }

  // a frequency of 0 means the thread is not started at all
  if (_properties.frequency == 0) {
    return;
  }
    
  MUTEX_LOCKER(locker, _threadMutex);

  if (server().isStopping()) {
    // don't create the thread if we are already shutting down
    return;
  }

  _thread.reset(new TtlThread(server(), *this));

  if (!_thread->start()) {
    LOG_TOPIC("33c33", FATAL, Logger::TTL) << "could not start ttl background thread";
    FATAL_ERROR_EXIT();
  }
}

void TtlFeature::beginShutdown() {
  // this will make the TTL background thread stop as soon as possible
  deactivate();
  
  MUTEX_LOCKER(locker, _threadMutex);

  if (_thread != nullptr) {
    // this will also wake up the thread if it should be sleeping
    _thread->beginShutdown();
  }
}

void TtlFeature::stop() {
  shutdownThread();
}

void TtlFeature::allowRunning(bool value) {
  {
    MUTEX_LOCKER(locker, _propertiesMutex);

    if (value) {
      _allowRunning = true;
    } else {
      _allowRunning = false;
    }
  }

  if (value) {
    return;
  }
     
  waitForThreadWork();
}

void TtlFeature::waitForThreadWork() {
  while (true) {
    {
      MUTEX_LOCKER(locker, _threadMutex);

      if (_thread == nullptr) {
        break;
      }

      _thread->wakeup();

      if (!_thread->isCurrentlyWorking()) {
        break;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}
  
void TtlFeature::activate() { 
  {
    MUTEX_LOCKER(locker, _propertiesMutex);
    if (_active) {
      // already activated
      return;
    }
    _active = true; 
  }

  LOG_TOPIC("79862", DEBUG, Logger::TTL) << "activated TTL background thread";
}

void TtlFeature::deactivate() { 
  {
    MUTEX_LOCKER(locker, _propertiesMutex);
    if (!_active) {
      // already deactivated
      return;
    }
    _active = false; 
  }

  waitForThreadWork();
  
  LOG_TOPIC("898a7", DEBUG, Logger::TTL) << "deactivated TTL background thread";
}

bool TtlFeature::isActive() const { 
  MUTEX_LOCKER(locker, _propertiesMutex);
  return _allowRunning && _active; 
}

void TtlFeature::statsToVelocyPack(VPackBuilder& builder) const {
  MUTEX_LOCKER(locker, _statisticsMutex);
  _statistics.toVelocyPack(builder);
}

void TtlFeature::updateStats(TtlStatistics const& stats) {
  MUTEX_LOCKER(locker, _statisticsMutex);
  _statistics += stats;
}

void TtlFeature::propertiesToVelocyPack(VPackBuilder& builder) const {
  MUTEX_LOCKER(locker, _propertiesMutex);
  _properties.toVelocyPack(builder, _active);
}

TtlProperties TtlFeature::properties() const {
  MUTEX_LOCKER(locker, _propertiesMutex);
  return _properties;
}

Result TtlFeature::propertiesFromVelocyPack(VPackSlice const& slice, VPackBuilder& out) {
  Result res;
  uint64_t frequency;
  bool active;

  {
    MUTEX_LOCKER(locker, _propertiesMutex);
    
    bool const hasActiveFlag = slice.isObject() && slice.hasKey("active");
    if (hasActiveFlag && !slice.get("active").isBool()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "active flag should be a boolean value");
    }

    // store properties
    res = _properties.fromVelocyPack(slice);
    if (!res.fail() && hasActiveFlag) {
      // update active flag
      _active = slice.get("active").getBool();
    }
    _properties.toVelocyPack(out, _active);
    frequency = _properties.frequency;
    active = _active;
  }
 
  { 
    MUTEX_LOCKER(locker, _threadMutex);

    if (_thread != nullptr) {
      _thread->setNextStart(frequency);
      _thread->wakeup();
    
      while (!active && _thread->isCurrentlyWorking()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }

  return res;
}

void TtlFeature::shutdownThread() noexcept {
  MUTEX_LOCKER(locker, _threadMutex);

  if (_thread != nullptr) {
    try {
      _thread->beginShutdown();
      while (_thread->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    } catch (...) {
    }

    _thread.reset();
  }
}
