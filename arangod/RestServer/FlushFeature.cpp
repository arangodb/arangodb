////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include <rocksdb/utilities/transaction_db.h>

#include "FlushFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/encoding.h"
#include "Basics/ReadLocker.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/MMFilesWalMarker.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/FlushThread.h"
#include "Utils/FlushTransaction.h"

namespace arangodb {

  /// @brief base class for FlushSubscription implementations
  class FlushFeature::FlushSubscriptionBase
      : public FlushFeature::FlushSubscription {
   public:
    TRI_voc_tick_t currentTick() const noexcept { return _currentTick; }

   protected:
    TRI_voc_tick_t _currentTick;
    TRI_voc_tick_t const _databaseId;
    arangodb::StorageEngine const& _engine;
    std::string const _type;

    FlushSubscriptionBase(
        std::string const& type,
        TRI_voc_tick_t databaseId,
        arangodb::StorageEngine const& engine
    ): _currentTick(engine.currentTick()),
       _databaseId(databaseId),
       _engine(engine) {
    }
  };

} // arangodb

namespace {

static const std::string DATA_ATTRIBUTE("data"); // attribute inside the flush marker storing custom data body
static const std::string TYPE_ATTRIBUTE("type"); // attribute inside the flush marker storing custom data type

// wrap vector inside a static function to ensure proper initialization order
std::unordered_map<std::string, arangodb::FlushFeature::FlushRecoveryCallback>& getFlushRecoveryCallbacks() {
  static std::unordered_map<std::string, arangodb::FlushFeature::FlushRecoveryCallback> callbacks;

  return callbacks;
}

arangodb::Result applyRecoveryMarker(
    TRI_voc_tick_t dbId,
    arangodb::velocypack::Slice const& slice
) {
  if (!slice.isObject()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      "failed to find a JSON object inside the supplied marker body while applying 'Flush' recovery marker"
    );
  }

  if (!slice.hasKey(TYPE_ATTRIBUTE)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      "failed to find a 'type' attribute inside the supplied marker body while applying 'Flush' recovery marker"
    );
  }

  auto typeSlice = slice.get(TYPE_ATTRIBUTE);

  if (!typeSlice.isString()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      "failed to find a string 'type' attribute inside the supplied marker body while applying 'Flush' recovery marker"
    );
  }

  auto type = typeSlice.copyString();
  auto& callbacks = getFlushRecoveryCallbacks();
  auto itr = callbacks.find(type);

  if (itr == callbacks.end()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find handler while applying 'Flush' recovery marker of type '") + type + "'"
    );
  }

  TRI_ASSERT(itr->second); // non-nullptr ensured by registerFlushRecoveryCallback(...)

  auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabaseFeature
  >("Database");

  if (!dbFeature) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to find feature 'Database' while applying 'Flush' recovery marker of type '") + type + "'"
    );
  }

  auto* vocbase = dbFeature->useDatabase(dbId);

  if (!vocbase) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find database '") + std::to_string(dbId) + "' while applying 'Flush' recovery marker of type '" + type + "'"
    );
  }

  TRI_DEFER(vocbase->release());

  if (!slice.hasKey(TYPE_ATTRIBUTE)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find a 'data' attribute inside the supplied marker body while applying 'Flush' recovery marker of type '") + type + "'"
    );
  }

  auto data = slice.get(DATA_ATTRIBUTE);

  try {
    return itr->second(*vocbase, data);
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(
      e.code(),
      std::string("caught exception while applying 'Flush' recovery marker of type '") + type + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while applying 'Flush' recovery marker of type '") + type + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while applying 'Flush' recovery marker of type '") + type + "'"
    );
  }
}

class MMFilesFlushMarker final: public arangodb::MMFilesWalMarker {
 public:
  /// @brief read constructor
  MMFilesFlushMarker(MMFilesMarker const& marker) {
    TRI_ASSERT(type() == marker.getType());
    auto* data = reinterpret_cast<uint8_t const*>(&marker);
    auto* ptr = data + sizeof(MMFilesMarker);
    auto* end = data + marker.getSize();

    TRI_ASSERT(sizeof(TRI_voc_tick_t) <= size_t(end - ptr));
    _databaseId = arangodb::encoding::readNumber<TRI_voc_tick_t>(
      ptr, sizeof(TRI_voc_tick_t)
    );
    ptr += sizeof(TRI_voc_tick_t);
    _slice = arangodb::velocypack::Slice(ptr);
    TRI_ASSERT(_slice.byteSize() == size_t(end - ptr));
  }

  /// @brief write constructor
  MMFilesFlushMarker(
      TRI_voc_tick_t databaseId,
      arangodb::velocypack::Slice const& slice
  ): _databaseId(databaseId), _slice(slice) {
  }

  TRI_voc_tick_t databaseId() const noexcept { return _databaseId; }

  // always 0 since not yet in any datafile
  TRI_voc_fid_t fid() const override final { return 0; }

  uint32_t size() const override final {
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tick_t) + _slice.byteSize();
  }

  arangodb::velocypack::Slice const& slice() const noexcept { return _slice; }

  void store(char* mem) const override final {
    auto* ptr = reinterpret_cast<uint8_t*>(mem) + sizeof(MMFilesMarker);

    arangodb::encoding::storeNumber<TRI_voc_tick_t>(
      ptr, _databaseId, sizeof(TRI_voc_tick_t)
    );
    ptr += sizeof(TRI_voc_tick_t);
    std::memcpy(ptr, _slice.begin(), size_t(_slice.byteSize()));
  }

  MMFilesMarkerType type() const override final {
    return MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC;
  }

 private:
  TRI_voc_tick_t _databaseId;
  arangodb::velocypack::Slice _slice;
};

class MMFilesFlushSubscription final
    : public arangodb::FlushFeature::FlushSubscriptionBase {
 public:
  MMFilesFlushSubscription(
    std::string const& type,
    TRI_voc_tick_t databaseId,
    arangodb::StorageEngine const& engine,
    arangodb::MMFilesLogfileManager& wal
  ): arangodb::FlushFeature::FlushSubscriptionBase(type, databaseId, engine),
     _wal(wal) {
  }

  arangodb::Result commit(arangodb::velocypack::Slice const& data) override {
    arangodb::velocypack::Builder builder;

    builder.openObject();
    builder.add(TYPE_ATTRIBUTE, arangodb::velocypack::Value(_type));
    builder.add(DATA_ATTRIBUTE, data);
    builder.close();

    MMFilesFlushMarker marker(_databaseId, builder.slice());
    auto res = arangodb::Result(_wal.allocateAndWrite(marker, true).errorCode);

    if (res.ok()) {
      _currentTick = _engine.currentTick();
    }

    return res;
  }

 private:
  arangodb::MMFilesLogfileManager& _wal;
};

class MMFilesRecoveryHelper final: public arangodb::MMFilesRecoveryHelper {
  virtual arangodb::Result replayMarker(
      MMFilesMarker const& marker
  ) const override {
    switch (marker.getType()) {
     case MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC: {
      auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::DatabaseFeature
      >("Database");

      if (!dbFeature) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          "failure to find feature 'Database' while applying 'Flush' recovery marker"
        );
      }

      MMFilesFlushMarker flushMarker(marker);

      return applyRecoveryMarker(flushMarker.databaseId(), flushMarker.slice());
     }
     default: {
      // NOOP
     }
    };

    return arangodb::Result();
  }
};

class RocksDBFlushMarker {
 public:
  /// @brief read constructor
  RocksDBFlushMarker(rocksdb::Slice const& marker) {
    TRI_ASSERT(arangodb::RocksDBLogType::FlushSync == arangodb::RocksDBLogValue::type(marker));
    auto* data = marker.data();
    auto* ptr = data + sizeof(arangodb::RocksDBLogType);
    auto* end = data + marker.size();

    TRI_ASSERT(sizeof(uint64_t) <= size_t(end - ptr));
    _databaseId = arangodb::rocksutils::uint64FromPersistent(ptr);
    ptr += sizeof(uint64_t);
    _slice = arangodb::velocypack::Slice(ptr);
    TRI_ASSERT(_slice.byteSize() == size_t(end - ptr));
  }

  /// @brief write constructor
  RocksDBFlushMarker(
      TRI_voc_tick_t databaseId,
      arangodb::velocypack::Slice const& slice
  ): _databaseId(databaseId), _slice(slice) {
  }

  TRI_voc_tick_t databaseId() const noexcept { return _databaseId; }
  arangodb::velocypack::Slice const& slice() const noexcept { return _slice; }

  void store(std::string& buf) const {
    buf.reserve(
      buf.size()
      + sizeof(arangodb::RocksDBLogType)
      + sizeof(uint64_t)
      + _slice.byteSize()
    );
    buf.push_back(static_cast<char>(arangodb::RocksDBLogType::FlushSync));
    arangodb::rocksutils::uint64ToPersistent(buf, _databaseId);
    buf.append(_slice.startAs<char>(), _slice.byteSize());
  }

 private:
  TRI_voc_tick_t _databaseId;
  arangodb::velocypack::Slice _slice;
};

class RocksDBFlushSubscription final
    : public arangodb::FlushFeature::FlushSubscriptionBase {
 public:
  RocksDBFlushSubscription(
    std::string const& type,
    TRI_voc_tick_t databaseId,
    arangodb::StorageEngine const& engine,
    rocksdb::DB& wal
  ): arangodb::FlushFeature::FlushSubscriptionBase(type, databaseId, engine),
     _wal(wal) {
  }

  arangodb::Result commit(arangodb::velocypack::Slice const& data) override {
    arangodb::velocypack::Builder builder;

    builder.openObject();
    builder.add(TYPE_ATTRIBUTE, arangodb::velocypack::Value(_type));
    builder.add(DATA_ATTRIBUTE, data);
    builder.close();

    rocksdb::WriteBatch batch;
    std::string buf;
    RocksDBFlushMarker marker(_databaseId, builder.slice());

    marker.store(buf);
    batch.PutLogData(rocksdb::Slice(buf));

    static const rocksdb::WriteOptions op;
    auto res = arangodb::rocksutils::convertStatus(_wal.Write(op, &batch));

    if (res.ok()) {
      _currentTick = _engine.currentTick();
    }

    return res;
  }

 private:
  rocksdb::DB& _wal;
};

class RocksDBRecoveryHelper final: public arangodb::RocksDBRecoveryHelper {
  virtual void LogData(rocksdb::Slice const& marker) override {
    switch (arangodb::RocksDBLogValue::type(marker)) {
     case arangodb::RocksDBLogType::FlushSync: {
      auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::DatabaseFeature
      >("Database");

      if (!dbFeature) {
        THROW_ARANGO_EXCEPTION(arangodb::Result(
          TRI_ERROR_INTERNAL,
          "failure to find feature 'Database' while applying 'Flush' recovery marker"
        ));
      }

      RocksDBFlushMarker flushMarker(marker);
      auto res =
        applyRecoveryMarker(flushMarker.databaseId(), flushMarker.slice());

      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }

      return;
     }
     default: {
      // NOOP
     }
    };
  }
};

void registerRecoveryHelper() {
  static const MMFilesRecoveryHelper mmfilesHelper;
  static const RocksDBRecoveryHelper rocksDBHelper;

  auto res = arangodb::MMFilesEngine::registerRecoveryHelper(mmfilesHelper);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  res = arangodb::RocksDBEngine::registerRecoveryHelper(
    std::shared_ptr<RocksDBRecoveryHelper>(
      const_cast<RocksDBRecoveryHelper*>(&rocksDBHelper),
      [](RocksDBRecoveryHelper*)->void {}
    )
  );

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

}

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

std::atomic<bool> FlushFeature::_isRunning(false);

FlushFeature::FlushFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Flush"), _flushInterval(1000000) {
  setOptional(true);
  startsAfter("BasicsPhase");

  startsAfter("StorageEngine");
  startsAfter("MMFilesLogfileManager");
}

void FlushFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");
  options->addOption("--server.flush-interval",
                     "interval (in microseconds) for flushing data",
                     new UInt64Parameter(&_flushInterval),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
}

/*static*/ bool FlushFeature::registerFlushRecoveryCallback(
    std::string const& type,
    FlushRecoveryCallback const& callback
) {
  return !callback // skip empty callbacks
         || getFlushRecoveryCallbacks().emplace(type, callback).second;
}

std::shared_ptr<FlushFeature::FlushSubscription> FlushFeature::registerFlushSubscription(
    std::string const& type,
    TRI_vocbase_t const& vocbase
) {
  auto* engine = EngineSelectorFeature::ENGINE;

  if (!engine) {
    LOG_TOPIC(ERR, Logger::FLUSH)
      << "failed to find a storage engine while registering 'Flush' marker subscription for type '" << type << "'";

    return nullptr;
  }

  auto* mmfilesEngine = dynamic_cast<MMFilesEngine*>(engine);

  if (mmfilesEngine) {
    auto* logFileManager = MMFilesLogfileManager::instance();

    if (!logFileManager) {
      LOG_TOPIC(ERR, Logger::FLUSH)
        << "failed to find an MMFiles log file manager instance while registering 'Flush' marker subscription for type '" << type << "'";

      return nullptr;
    }

    auto subscription = std::make_shared<MMFilesFlushSubscription>(
      type, vocbase.id(), *mmfilesEngine, *logFileManager
    );
    std::lock_guard<std::mutex> lock(_flushSubscriptionsMutex);

    _flushSubscriptions.emplace(subscription);

    return subscription;
  }

  auto* rocksdbEngine = dynamic_cast<RocksDBEngine*>(engine);

  if (rocksdbEngine) {
    auto* db = rocksdbEngine->db();

    if (!db) {
      LOG_TOPIC(ERR, Logger::FLUSH)
       << "failed to find a RocksDB engine db while registering 'Flush' marker subscription for type '" << type << "'";

      return nullptr;
    }

    auto* rootDb = db->GetRootDB();

    if (!rootDb) {
      LOG_TOPIC(ERR, Logger::FLUSH)
        << "failed to find a RocksDB engine root db while registering 'Flush' marker subscription for type '" << type << "'";

      return nullptr;
    }

    auto subscription = std::make_shared<RocksDBFlushSubscription>(
      type, vocbase.id(), *rocksdbEngine, *rootDb
    );
    std::lock_guard<std::mutex> lock(_flushSubscriptionsMutex);

    _flushSubscriptions.emplace(subscription);

    return subscription;
  }

  LOG_TOPIC(ERR, Logger::FLUSH)
    << "failed to identify storage engine while registering 'Flush' marker subscription for type '" << type << "'";

  return nullptr;
}

arangodb::Result FlushFeature::releaseUnusedTicks() {
  auto* engine = EngineSelectorFeature::ENGINE;

  if (!engine) {
    return Result(
      TRI_ERROR_INTERNAL,
      "failed to find a storage engine while releasing unused ticks"
    );
  }

  auto minTick = engine->currentTick();

  {
    std::lock_guard<std::mutex> lock(_flushSubscriptionsMutex);

    // find min tick and remove stale subscriptions
    for (auto itr = _flushSubscriptions.begin(),
         end = _flushSubscriptions.end();
         itr != end;
         ) {
      auto entry = *itr;

      if (!entry || entry.use_count() == 1) {
        itr = _flushSubscriptions.erase(itr); // remove stale
      } else {
        minTick = std::min(minTick, entry->currentTick());
        ++itr;
      }
    }
  }

  engine->waitForSyncTick(minTick);
  engine->releaseTick(minTick);

  return Result();
}

void FlushFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  if (_flushInterval < 1000) {
    // do not go below 1000 microseconds
    _flushInterval = 1000;
  }
}

void FlushFeature::prepare() {
  // At least for now we need FlushThread for ArangoSearch views
  // on a DB/Single server only, so we avoid starting FlushThread on
  // a coordinator and on agency nodes.
  setEnabled(!arangodb::ServerState::instance()->isCoordinator() &&
             !arangodb::ServerState::instance()->isAgent());

  if (!isEnabled()) {
    return;
  }

  registerRecoveryHelper();
}

void FlushFeature::start() {
  {
    WRITE_LOCKER(lock, _threadLock);
    _flushThread.reset(new FlushThread(_flushInterval));
  }
  DatabaseFeature* dbFeature = DatabaseFeature::DATABASE;
  dbFeature->registerPostRecoveryCallback([this]() -> Result {
    READ_LOCKER(lock, _threadLock);
    if (!this->_flushThread->start()) {
      LOG_TOPIC(FATAL, Logger::FLUSH) << "unable to start FlushThread";
      FATAL_ERROR_ABORT();
    } else {
      LOG_TOPIC(DEBUG, Logger::FLUSH) << "started FlushThread";
    }

    this->_isRunning.store(true);

    return {TRI_ERROR_NO_ERROR};
  });
}

void FlushFeature::beginShutdown() {
  // pass on the shutdown signal
  READ_LOCKER(lock, _threadLock);
  if (_flushThread != nullptr) {
    _flushThread->beginShutdown();
  }
}

void FlushFeature::stop() {
  LOG_TOPIC(TRACE, arangodb::Logger::FLUSH) << "stopping FlushThread";
  // wait until thread is fully finished

  FlushThread* thread = nullptr;
  {
    READ_LOCKER(lock, _threadLock);
    thread = _flushThread.get();
  }
  if (thread != nullptr) {
    while (thread->isRunning()) {
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }

    {
      WRITE_LOCKER(wlock, _threadLock);
      _isRunning.store(false);
      _flushThread.reset();
    }
  }
}

void FlushFeature::unprepare() {
  WRITE_LOCKER(locker, _callbacksLock);
  _callbacks.clear();
}

void FlushFeature::registerCallback(void* ptr, FlushFeature::FlushCallback const& cb) {
  WRITE_LOCKER(locker, _callbacksLock);
  _callbacks.emplace(ptr, std::move(cb));
  LOG_TOPIC(TRACE, arangodb::Logger::FLUSH) << "registered new flush callback";
}

bool FlushFeature::unregisterCallback(void* ptr) {
  WRITE_LOCKER(locker, _callbacksLock);

  auto it = _callbacks.find(ptr);
  if (it == _callbacks.end()) {
    return false;
  }

  _callbacks.erase(it);
  LOG_TOPIC(TRACE, arangodb::Logger::FLUSH) << "unregistered flush callback";
  return true;
}

void FlushFeature::executeCallbacks() {
  std::vector<FlushTransactionPtr> transactions;

  {
    READ_LOCKER(locker, _callbacksLock);
    transactions.reserve(_callbacks.size());

    // execute all callbacks. this will create as many transactions as
    // there are callbacks
    for (auto const& cb : _callbacks) {
      // copy elision, std::move(..) not required
      LOG_TOPIC(TRACE, arangodb::Logger::FLUSH) << "executing flush callback";
      transactions.emplace_back(cb.second());
    }
  }

  // TODO: make sure all data is synced

  // commit all transactions
  for (auto const& trx : transactions) {
    LOG_TOPIC(DEBUG, Logger::FLUSH)
        << "commiting flush transaction '" << trx->name() << "'";

    Result res = trx->commit();

    LOG_TOPIC_IF(ERR, Logger::FLUSH, res.fail())
        << "could not commit flush transaction '" << trx->name()
        << "': " << res.errorMessage();
    // TODO: honor the commit results here
  }
}

}  // namespace arangodb
