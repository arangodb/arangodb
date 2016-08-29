////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "collection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/PageSizeFeature.h"
#include "Aql/QueryCache.h"
#include "Basics/Barrier.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/ThreadPool.h"
#include "Basics/Timers.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterMethods.h"
#include "FulltextIndex/fulltext-index.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/CollectionWriteLocker.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/IndexPoolFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "Wal/DocumentOperation.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"
#include "Wal/Slots.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBIndex.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>
#endif

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

int TRI_AddOperationTransaction(TRI_transaction_t*,
                                arangodb::wal::DocumentOperation&, bool&);

TRI_collection_t::TRI_collection_t(TRI_vocbase_t* vocbase, 
                                   arangodb::VocbaseCollectionInfo const& parameters)
      : _vocbase(vocbase), 
        _tickMax(0),
        _uncollectedLogfileEntries(0),
        _ditches(this),
        _nextCompactionStartIndex(0),
        _lastCompactionStatus(nullptr),
        _lastCompaction(0.0) {

  // check if we can generate the key generator
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> buffer =
      parameters.keyOptions();

  VPackSlice slice;
  if (buffer != nullptr) {
    slice = VPackSlice(buffer->data());
  }
  
  std::unique_ptr<KeyGenerator> keyGenerator(KeyGenerator::factory(slice));

  if (keyGenerator == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR);
  }

  _keyGenerator.reset(keyGenerator.release());

  setCompactionStatus("compaction not yet started");
}
  
TRI_collection_t::~TRI_collection_t() {
  _ditches.destroy();
}

/// @brief whether or not a collection is fully collected
bool TRI_collection_t::isFullyCollected() {
  READ_LOCKER(readLocker, _lock);

  int64_t uncollected = _uncollectedLogfileEntries.load();

  return (uncollected == 0);
}

void TRI_collection_t::setNextCompactionStartIndex(size_t index) {
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  _nextCompactionStartIndex = index;
}

size_t TRI_collection_t::getNextCompactionStartIndex() {
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  return _nextCompactionStartIndex;
}

void TRI_collection_t::setCompactionStatus(char const* reason) {
  TRI_ASSERT(reason != nullptr);
  struct tm tb;
  time_t tt = time(nullptr);
  TRI_gmtime(tt, &tb);

  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  _lastCompactionStatus = reason;

  strftime(&_lastCompactionStamp[0], sizeof(_lastCompactionStamp),
           "%Y-%m-%dT%H:%M:%SZ", &tb);
}

void TRI_collection_t::getCompactionStatus(char const*& reason,
                                           char* dst, size_t maxSize) {
  memset(dst, 0, maxSize);
  if (maxSize > sizeof(_lastCompactionStamp)) {
    maxSize = sizeof(_lastCompactionStamp);
  }
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  reason = _lastCompactionStatus;
  memcpy(dst, &_lastCompactionStamp[0], maxSize);
}

void TRI_collection_t::figures(std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  DatafileStatisticsContainer dfi = _datafileStatistics.all();

  builder->add("alive", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(dfi.numberAlive));
  builder->add("size", VPackValue(dfi.sizeAlive));
  builder->close(); // alive
  
  builder->add("dead", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(dfi.numberDead));
  builder->add("size", VPackValue(dfi.sizeDead));
  builder->add("deletion", VPackValue(dfi.numberDeletions));
  builder->close(); // dead

  builder->add("uncollectedLogfileEntries", VPackValue(_uncollectedLogfileEntries));
  builder->add("lastTick", VPackValue(_tickMax));

  builder->add("documentReferences", VPackValue(_ditches.numDocumentDitches()));
  
  char const* waitingForDitch = _ditches.head();
  builder->add("waitingFor", VPackValue(waitingForDitch == nullptr ? "-" : waitingForDitch));
  
  // fills in compaction status
  char const* lastCompactionStatus = nullptr;
  char lastCompactionStamp[21];
  getCompactionStatus(lastCompactionStatus,
                      &lastCompactionStamp[0],
                      sizeof(lastCompactionStamp));

  if (lastCompactionStatus == nullptr) {
    lastCompactionStatus = "-";
    lastCompactionStamp[0] = '-';
    lastCompactionStamp[1] = '\0';
  }
  
  builder->add("compactionStatus", VPackValue(VPackValueType::Object));
  builder->add("message", VPackValue(lastCompactionStatus));
  builder->add("time", VPackValue(&lastCompactionStamp[0]));
  builder->close(); // compactionStatus
}

/// @brief checks if a collection name is allowed
/// Returns true if the name is allowed and false otherwise
bool TRI_collection_t::IsAllowedName(bool allowSystem, std::string const& name) {
  bool ok;
  char const* ptr;
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is
  // allowed
  for (ptr = name.c_str(); *ptr; ++ptr) {
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') ||
             ('A' <= *ptr && *ptr <= 'Z');
      } else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    } else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') ||
           ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (!ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             std::string const& name,
                                             TRI_col_type_e type,
                                             TRI_voc_size_t maximalSize,
                                             VPackSlice const& keyOptions)
    : _type(type),
      _revision(0),
      _cid(0),
      _planId(0),
      _maximalSize(32 * 1024 * 1024), // just to have a default
      _initialCount(-1),
      _indexBuckets(DatabaseFeature::DefaultIndexBuckets),
      _keyOptions(nullptr),
      _isSystem(false),
      _deleted(false),
      _doCompact(true),
      _isVolatile(false),
      _waitForSync(false) {

  auto database = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
  _maximalSize = static_cast<TRI_voc_size_t>(database->maximalJournalSize());
  _waitForSync = database->waitForSync();

  size_t pageSize = PageSizeFeature::getPageSize();
  _maximalSize =
      static_cast<TRI_voc_size_t>((maximalSize / pageSize) * pageSize);
  if (_maximalSize == 0 && maximalSize != 0) {
    _maximalSize = static_cast<TRI_voc_size_t>(pageSize);
  }
  memset(_name, 0, sizeof(_name));
  TRI_CopyString(_name, name.c_str(), sizeof(_name) - 1);

  if (!keyOptions.isNone()) {
    VPackBuilder builder;
    builder.add(keyOptions);
    _keyOptions = builder.steal();
  }
}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             std::string const& name,
                                             VPackSlice const& options, 
                                             bool forceIsSystem)
    : VocbaseCollectionInfo(vocbase, name, TRI_COL_TYPE_DOCUMENT, options,
                            forceIsSystem) {}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             std::string const& name,
                                             TRI_col_type_e type,
                                             VPackSlice const& options,
                                             bool forceIsSystem)
    : _type(type),
      _revision(0),
      _cid(0),
      _planId(0),
      _maximalSize(32 * 1024 * 1024), // just to have a default
      _initialCount(-1),
      _indexBuckets(DatabaseFeature::DefaultIndexBuckets),
      _keyOptions(nullptr),
      _isSystem(false),
      _deleted(false),
      _doCompact(true),
      _isVolatile(false),
      _waitForSync(false) {
  
  auto database = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
  _maximalSize = static_cast<TRI_voc_size_t>(database->maximalJournalSize());
  _waitForSync = database->waitForSync();

  memset(_name, 0, sizeof(_name));

  TRI_CopyString(_name, name.c_str(), sizeof(_name) - 1);

  if (options.isObject()) {
    TRI_voc_size_t maximalSize;
    if (options.hasKey("journalSize")) {
      maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              options, "journalSize", _maximalSize);
    } else {
      maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              options, "maximalSize", _maximalSize);
    }

    size_t pageSize = PageSizeFeature::getPageSize();
    _maximalSize =
        static_cast<TRI_voc_size_t>((maximalSize / pageSize) * pageSize);
    if (_maximalSize == 0 && maximalSize != 0) {
      _maximalSize = static_cast<TRI_voc_size_t>(pageSize);
    }

    if (options.hasKey("count")) {
      _initialCount =
          arangodb::basics::VelocyPackHelper::getNumericValue<int64_t>(
              options, "count", -1);
    }

    _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "doCompact", true);
    _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "waitForSync", _waitForSync);
    _isVolatile = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "isVolatile", false);
    _indexBuckets =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
            options, "indexBuckets", DatabaseFeature::DefaultIndexBuckets);
    _type = static_cast<TRI_col_type_e>(
        arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
            options, "type", _type));

    std::string cname =
        arangodb::basics::VelocyPackHelper::getStringValue(options, "name", "");
    if (!cname.empty()) {
      TRI_CopyString(_name, cname.c_str(), sizeof(_name) - 1);
    }

    std::string cidString =
        arangodb::basics::VelocyPackHelper::getStringValue(options, "cid", "");
    if (!cidString.empty()) {
      // note: this may throw
      _cid = std::stoull(cidString);
    }

    if (options.hasKey("isSystem")) {
      VPackSlice isSystemSlice = options.get("isSystem");
      if (isSystemSlice.isBoolean()) {
        _isSystem = isSystemSlice.getBoolean();
      }
    } else {
      _isSystem = false;
    }

    if (options.hasKey("journalSize")) {
      VPackSlice maxSizeSlice = options.get("journalSize");
      TRI_voc_size_t maximalSize =
          maxSizeSlice.getNumericValue<TRI_voc_size_t>();
      if (maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "journalSize is too small");
      }
    }

    VPackSlice const planIdSlice = options.get("planId");
    TRI_voc_cid_t planId = 0;
    if (planIdSlice.isNumber()) {
      planId = planIdSlice.getNumericValue<TRI_voc_cid_t>();
    } else if (planIdSlice.isString()) {
      std::string tmp = planIdSlice.copyString();
      planId = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
    }

    if (planId > 0) {
      _planId = planId;
    }

    VPackSlice const cidSlice = options.get("id");
    if (cidSlice.isNumber()) {
      _cid = cidSlice.getNumericValue<TRI_voc_cid_t>();
    } else if (cidSlice.isString()) {
      std::string tmp = cidSlice.copyString();
      _cid = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
    }

    if (options.hasKey("keyOptions")) {
      VPackSlice const slice = options.get("keyOptions");
      VPackBuilder builder;
      builder.add(slice);
      // Copy the ownership of the options over
      _keyOptions = builder.steal();
    }

    if (options.hasKey("deleted")) {
      VPackSlice const slice = options.get("deleted");
      if (slice.isBoolean()) {
        _deleted = slice.getBoolean();
      }
    }
  }

#ifndef TRI_HAVE_ANONYMOUS_MMAP
  if (_isVolatile) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections are not supported on this platform");
  }
#endif

  if (_isVolatile && _waitForSync) {
    // the combination of waitForSync and isVolatile makes no sense
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections do not support the waitForSync option");
  }

  if (_indexBuckets < 1 || _indexBuckets > 1024) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "indexBuckets must be a two-power between 1 and 1024");
  }

  if (!TRI_collection_t::IsAllowedName(_isSystem || forceIsSystem, _name)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // fix _isSystem value if mis-specified by user
  _isSystem = (*_name == '_');
}

// collection type
TRI_col_type_e VocbaseCollectionInfo::type() const { return _type; }

// local collection identifier
TRI_voc_cid_t VocbaseCollectionInfo::id() const { return _cid; }

// cluster-wide collection identifier
TRI_voc_cid_t VocbaseCollectionInfo::planId() const { return _planId; }

// last revision id written
TRI_voc_rid_t VocbaseCollectionInfo::revision() const { return _revision; }

// maximal size of memory mapped file
TRI_voc_size_t VocbaseCollectionInfo::maximalSize() const {
  return _maximalSize;
}

// initial count, used when loading a collection
int64_t VocbaseCollectionInfo::initialCount() const { return _initialCount; }

// number of buckets used in hash tables for indexes
uint32_t VocbaseCollectionInfo::indexBuckets() const { return _indexBuckets; }

// name of the collection
std::string VocbaseCollectionInfo::name() const { return std::string(_name); }

// options for key creation
std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
VocbaseCollectionInfo::keyOptions() const {
  return _keyOptions;
}

// If true, collection has been deleted
bool VocbaseCollectionInfo::deleted() const { return _deleted; }

// If true, collection will be compacted
bool VocbaseCollectionInfo::doCompact() const { return _doCompact; }

// If true, collection is a system collection
bool VocbaseCollectionInfo::isSystem() const { return _isSystem; }

// If true, collection is memory-only
bool VocbaseCollectionInfo::isVolatile() const { return _isVolatile; }

// If true waits for mysnc
bool VocbaseCollectionInfo::waitForSync() const { return _waitForSync; }

void VocbaseCollectionInfo::rename(std::string const& name) {
  TRI_CopyString(_name, name.c_str(), sizeof(_name) - 1);
}

void VocbaseCollectionInfo::setRevision(TRI_voc_rid_t rid, bool force) {
  if (force || rid > _revision) {
    _revision = rid;
  }
}

void VocbaseCollectionInfo::setCollectionId(TRI_voc_cid_t cid) { _cid = cid; }

void VocbaseCollectionInfo::updateCount(size_t size) { _initialCount = size; }

void VocbaseCollectionInfo::setPlanId(TRI_voc_cid_t planId) {
  _planId = planId;
}

void VocbaseCollectionInfo::setDeleted(bool deleted) { _deleted = deleted; }

void VocbaseCollectionInfo::clearKeyOptions() { _keyOptions.reset(); }

void VocbaseCollectionInfo::update(VPackSlice const& slice, bool preferDefaults,
                                   TRI_vocbase_t const* vocbase) {
  // the following collection properties are intentionally not updated as
  // updating
  // them would be very complicated:
  // - _cid
  // - _name
  // - _type
  // - _isSystem
  // - _isVolatile
  // ... probably a few others missing here ...

  if (preferDefaults) {
    if (vocbase != nullptr) {
      auto database = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");

      _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "doCompact", true);
      _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "waitForSync", database->waitForSync());
      if (slice.hasKey("journalSize")) {
        _maximalSize = arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
            slice, "journalSize", static_cast<TRI_voc_size_t>(database->maximalJournalSize()));
      } else {
        _maximalSize = arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
            slice, "maximalSize", static_cast<TRI_voc_size_t>(database->maximalJournalSize()));
      }
    } else {
      _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "doCompact", true);
      _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "waitForSync", false);
      if (slice.hasKey("journalSize")) {
        _maximalSize =
            arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
                slice, "journalSize", TRI_JOURNAL_DEFAULT_SIZE);
      } else {
        _maximalSize =
            arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
                slice, "maximalSize", TRI_JOURNAL_DEFAULT_SIZE);
      }
    }
    _indexBuckets =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
            slice, "indexBuckets", DatabaseFeature::DefaultIndexBuckets);
  } else {
    _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
        slice, "doCompact", _doCompact);
    _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
        slice, "waitForSync", _waitForSync);
    if (slice.hasKey("journalSize")) {
      _maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              slice, "journalSize", _maximalSize);
    } else {
      _maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              slice, "maximalSize", _maximalSize);
    }
    _indexBuckets =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
            slice, "indexBuckets", _indexBuckets);

    _initialCount =
        arangodb::basics::VelocyPackHelper::getNumericValue<int64_t>(
            slice, "count", _initialCount);
  }
}

void VocbaseCollectionInfo::update(VocbaseCollectionInfo const& other) {
  _type = other.type();
  _cid = other.id();
  _planId = other.planId();
  _revision = other.revision();
  _maximalSize = other.maximalSize();
  _initialCount = other.initialCount();
  _indexBuckets = other.indexBuckets();

  TRI_CopyString(_name, other.name().c_str(), sizeof(_name) - 1);

  _keyOptions = other.keyOptions();

  _deleted = other.deleted();
  _doCompact = other.doCompact();
  _isSystem = other.isSystem();
  _isVolatile = other.isVolatile();
  _waitForSync = other.waitForSync();
}

std::shared_ptr<VPackBuilder> VocbaseCollectionInfo::toVelocyPack() const {
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  toVelocyPack(*builder);
  builder->close();
  return builder;
}

void VocbaseCollectionInfo::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(!builder.isClosed());

  std::string cidString = std::to_string(id());
  std::string planIdString = std::to_string(planId());

  builder.add("version", VPackValue(version()));
  builder.add("type", VPackValue(type()));
  builder.add("cid", VPackValue(cidString));

  if (planId() > 0) {
    builder.add("planId", VPackValue(planIdString));
  }

  if (initialCount() >= 0) {
    builder.add("count", VPackValue(initialCount()));
  }
  builder.add("indexBuckets", VPackValue(indexBuckets()));
  builder.add("deleted", VPackValue(deleted()));
  builder.add("doCompact", VPackValue(doCompact()));
  builder.add("maximalSize", VPackValue(maximalSize()));
  builder.add("name", VPackValue(name()));
  builder.add("isVolatile", VPackValue(isVolatile()));
  builder.add("waitForSync", VPackValue(waitForSync()));
  builder.add("isSystem", VPackValue(isSystem()));

  auto opts = keyOptions();
  if (opts.get() != nullptr) {
    VPackSlice const slice(opts->data());
    builder.add("keyOptions", slice);
  }
}

/// @brief state during opening of a collection
struct OpenIteratorState {
  LogicalCollection* _collection;
  TRI_collection_t* _document;
  TRI_voc_tid_t _tid;
  TRI_voc_fid_t _fid;
  std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer*> _stats;
  DatafileStatisticsContainer* _dfi;
  TRI_vocbase_t* _vocbase;
  arangodb::Transaction* _trx;
  uint64_t _deletions;
  uint64_t _documents;
  int64_t _initialCount;

  // NOTE We need to hand in both because in some situation the collection_t  is not
  // yet attached to the LogicalCollection.
  OpenIteratorState(LogicalCollection* collection, TRI_collection_t* document,
                    TRI_vocbase_t* vocbase)
      : _collection(collection),
        _document(document),
        _tid(0),
        _fid(0),
        _stats(),
        _dfi(nullptr),
        _vocbase(vocbase),
        _trx(nullptr),
        _deletions(0),
        _documents(0),
        _initialCount(-1) {
    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(document != nullptr);
  }

  ~OpenIteratorState() {
    for (auto& it : _stats) {
      delete it.second;
    }
  }
};

/// @brief find a statistics container for a given file id
static DatafileStatisticsContainer* FindDatafileStats(
    OpenIteratorState* state, TRI_voc_fid_t fid) {
  auto it = state->_stats.find(fid);

  if (it != state->_stats.end()) {
    return (*it).second;
  }

  auto stats = std::make_unique<DatafileStatisticsContainer>();

  state->_stats.emplace(fid, stats.get());
  auto p = stats.release();

  return p;
}

/// @brief process a document (or edge) marker when opening a collection
static int OpenIteratorHandleDocumentMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            OpenIteratorState* state) {
  auto const fid = datafile->_fid;
  LogicalCollection* collection = state->_collection;
  TRI_collection_t* document = state->_document;
  arangodb::Transaction* trx = state->_trx;

  VPackSlice const slice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  Transaction::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
 
  collection->setRevision(revisionId, false);
  VPackValueLength length;
  char const* p = keySlice.getString(length);
  document->_keyGenerator->track(p, length);

  ++state->_documents;
 
  if (state->_fid != fid) {
    // update the state
    state->_fid = fid; // when we're here, we're looking at a datafile
    state->_dfi = FindDatafileStats(state, fid);
  }

  auto primaryIndex = collection->primaryIndex();

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto found = primaryIndex->lookupKey(trx, keySlice);

  // it is a new entry
  if (found == nullptr) {
    TRI_doc_mptr_t* header = collection->_masterPointers.request();

    if (header == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    header->setFid(fid, false);
    header->setHash(primaryIndex->calculateHash(trx, keySlice));
    header->setVPackFromMarker(marker);  

    // insert into primary index
    void const* result = nullptr;
    int res = primaryIndex->insertKey(trx, header, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      collection->_masterPointers.release(header);
      LOG(ERR) << "inserting document into primary index failed with error: " << TRI_errno_string(res);

      return res;
    }
    ++collection->_numberDocuments;

    // update the datafile info
    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive += DatafileHelper::AlignedMarkerSize<int64_t>(marker);
  }

  // it is an update, but only if found has a smaller revision identifier
  else {
    // save the old data
    TRI_doc_mptr_t oldData = *found;

    // update the header info
    found->setFid(fid, false); // when we're here, we're looking at a datafile
    found->setVPackFromMarker(marker);

    // update the datafile info
    DatafileStatisticsContainer* dfi;
    if (oldData.getFid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, oldData.getFid());
    }

    if (oldData.vpack() != nullptr) { 
      int64_t size = static_cast<int64_t>(oldData.markerSize());

      dfi->numberAlive--;
      dfi->sizeAlive -= DatafileHelper::AlignedSize<int64_t>(size);
      dfi->numberDead++;
      dfi->sizeDead += DatafileHelper::AlignedSize<int64_t>(size);
    }

    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive += DatafileHelper::AlignedMarkerSize<int64_t>(marker);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief process a deletion marker when opening a collection
static int OpenIteratorHandleDeletionMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            OpenIteratorState* state) {
  LogicalCollection* collection = state->_collection;
  arangodb::Transaction* trx = state->_trx;

  VPackSlice const slice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_REMOVE));
  
  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  Transaction::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
 
  collection->setRevision(revisionId, false);
  VPackValueLength length;
  char const* p = keySlice.getString(length);
  collection->keyGenerator()->track(p, length);

  ++state->_deletions;

  if (state->_fid != datafile->_fid) {
    // update the state
    state->_fid = datafile->_fid;
    state->_dfi = FindDatafileStats(state, datafile->_fid);
  }

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto primaryIndex = collection->primaryIndex();
  TRI_doc_mptr_t* found = primaryIndex->lookupKey(trx, keySlice);

  // it is a new entry, so we missed the create
  if (found == nullptr) {
    // update the datafile info
    state->_dfi->numberDeletions++;
  }

  // it is a real delete
  else {
    // update the datafile info
    DatafileStatisticsContainer* dfi;

    if (found->getFid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, found->getFid());
    }

    TRI_ASSERT(found->vpack() != nullptr);

    int64_t size = DatafileHelper::AlignedSize<int64_t>(found->markerSize());

    dfi->numberAlive--;
    dfi->sizeAlive -= DatafileHelper::AlignedSize<int64_t>(size);
    dfi->numberDead++;
    dfi->sizeDead += DatafileHelper::AlignedSize<int64_t>(size);
    state->_dfi->numberDeletions++;

    collection->deletePrimaryIndex(trx, found);

    --collection->_numberDocuments;

    // free the header
    collection->_masterPointers.release(found);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief iterator for open
static bool OpenIterator(TRI_df_marker_t const* marker, OpenIteratorState* data,
                         TRI_datafile_t* datafile) {
  TRI_collection_t* document = data->_document;
  TRI_voc_tick_t const tick = marker->getTick();
  TRI_df_marker_type_t const type = marker->getType();

  int res;

  if (type == TRI_DF_MARKER_VPACK_DOCUMENT) {
    res = OpenIteratorHandleDocumentMarker(marker, datafile, data);

    if (datafile->_dataMin == 0) {
      datafile->_dataMin = tick;
    }

    if (tick > datafile->_dataMax) {
      datafile->_dataMax = tick;
    }
  } else if (type == TRI_DF_MARKER_VPACK_REMOVE) {
    res = OpenIteratorHandleDeletionMarker(marker, datafile, data);
  } else {
    if (type == TRI_DF_MARKER_HEADER) {
      // ensure there is a datafile info entry for each datafile of the
      // collection
      FindDatafileStats(data, datafile->_fid);
    }

    LOG(TRACE) << "skipping marker type " << TRI_NameMarkerDatafile(marker);
    res = TRI_ERROR_NO_ERROR;
  }

  if (datafile->_tickMin == 0) {
    datafile->_tickMin = tick;
  }

  if (tick > datafile->_tickMax) {
    datafile->_tickMax = tick;
  }

  if (tick > document->_tickMax) {
    if (type != TRI_DF_MARKER_HEADER &&
        type != TRI_DF_MARKER_FOOTER &&
        type != TRI_DF_MARKER_COL_HEADER &&
        type != TRI_DF_MARKER_PROLOGUE) {
      document->_tickMax = tick;
    }
  }

  return (res == TRI_ERROR_NO_ERROR);
}

/// @brief iterate all markers of the collection
static int IterateMarkersCollection(arangodb::Transaction* trx,
                                    LogicalCollection* collection,
                                    TRI_collection_t* document) {

  // FIXME All still reference collection()->_info.
  // initialize state for iteration
  OpenIteratorState openState(collection, document, collection->vocbase());

  if (collection->getPhysical()->initialCount() != -1) {
    auto primaryIndex = collection->primaryIndex();

    int res = primaryIndex->resize(
        trx, static_cast<size_t>(collection->getPhysical()->initialCount() * 1.1));

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    openState._initialCount = collection->getPhysical()->initialCount();
  }

  // read all documents and fill primary index
  auto cb = [&openState](TRI_df_marker_t const* marker, TRI_datafile_t* datafile) -> bool {
    return OpenIterator(marker, &openState, datafile); 
  };

  collection->iterateDatafiles(cb);

  LOG(TRACE) << "found " << openState._documents << " document markers, " 
             << openState._deletions << " deletion markers for collection '" << collection->name() << "'";
  
  // update the real statistics for the collection
  try {
    for (auto& it : openState._stats) {
      document->_datafileStatistics.create(it.first, *(it.second));
    }
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief creates a new collection
TRI_collection_t* TRI_collection_t::create(
    TRI_vocbase_t* vocbase, VocbaseCollectionInfo& parameters,
    TRI_voc_cid_t cid) {
  if (cid > 0) {
    TRI_UpdateTickServer(cid);
  } else {
    cid = TRI_NewTickServer();
  }

  parameters.setCollectionId(cid);
      
  auto collection = std::make_unique<TRI_collection_t>(vocbase, parameters); 

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::string const path = engine->createCollection(vocbase, cid, parameters);
  collection->setPath(path);
  
  return collection.release();
}

/// @brief opens an existing collection
TRI_collection_t* TRI_collection_t::open(TRI_vocbase_t* vocbase,
                                         arangodb::LogicalCollection* col,
                                         bool ignoreErrors) {
  VPackBuilder builder;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->getCollectionInfo(vocbase, col->cid(), builder, false, 0);

  VocbaseCollectionInfo parameters(vocbase, col->name(), builder.slice().get("parameters"), true); 
  TRI_ASSERT(parameters.id() != 0);
  
  // open the collection
  auto collection = std::make_unique<TRI_collection_t>(vocbase, parameters);

  double start = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "open-document-collection { collection: " << vocbase->name() << "/"
      << col->name() << " }";

  collection->_path = col->path();
  int res = col->open(ignoreErrors);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot open document collection from path '" << col->path() << "'";

    return nullptr;
  }

  res = col->createInitialIndexes();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot initialize document collection";
    return nullptr;
  }
  
  arangodb::SingleCollectionTransaction trx(
      arangodb::StandaloneTransactionContext::Create(vocbase),
      col->cid(), TRI_TRANSACTION_WRITE);

  // build the primary index
  res = TRI_ERROR_INTERNAL;

  try {
    double start = TRI_microtime();

    LOG_TOPIC(TRACE, Logger::PERFORMANCE)
        << "iterate-markers { collection: " << vocbase->name() << "/"
        << col->name() << " }";

    // iterate over all markers of the collection
    res = IterateMarkersCollection(&trx, col, collection.get());

    LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::FIXED(TRI_microtime() - start) << " s, iterate-markers { collection: " << vocbase->name() << "/" << col->name() << " }";
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (std::bad_alloc const&) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot iterate data of document collection";
    TRI_set_errno(res);

    return nullptr;
  }

  // build the indexes meta-data, but do not fill the indexes yet
  {
    auto old = col->useSecondaryIndexes();

    // turn filling of secondary indexes off. we're now only interested in getting
    // the indexes' definition. we'll fill them below ourselves.
    col->useSecondaryIndexes(false);

    try {
      col->detectIndexes(&trx);
      col->useSecondaryIndexes(old);
    } catch (...) {
      col->useSecondaryIndexes(old);
      LOG(ERR) << "cannot initialize collection indexes";
      return nullptr;
    }
  }


  if (!arangodb::wal::LogfileManager::instance()->isInRecovery()) {
    // build the index structures, and fill the indexes
    col->fillIndexes(&trx);
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "[timer] " << Logger::FIXED(TRI_microtime() - start)
      << " s, open-document-collection { collection: " << vocbase->name() << "/"
      << col->name() << " }";

  return collection.release();
}
