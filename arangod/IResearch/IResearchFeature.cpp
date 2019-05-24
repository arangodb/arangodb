//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

// otherwise define conflict between 3rdParty\date\include\date\date.h and 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
  #include "date/date.h"
  #undef NOEXCEPT
#endif

#include "search/scorers.hpp"
#include "utils/log.hpp"

#include "ApplicationServerHelper.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlValue.h"
#include "Aql/Function.h"
#include "Basics/ConditionLocker.h"
#include "Basics/SmallVector.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterFeature.h"
#include "Containers.h"
#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchLinkCoordinator.h"
#include "IResearchLinkHelper.h"
#include "IResearchMMFilesLink.h"
#include "IResearchRocksDBLink.h"
#include "IResearchRocksDBRecoveryHelper.h"
#include "IResearchView.h"
#include "IResearchViewCoordinator.h"
#include "Logger/LogMacros.h"
#include "MMFiles/MMFilesEngine.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

namespace arangodb {

namespace basics {

class VPackStringBufferAdapter;
}  // namespace basics

namespace aql {
class Query;
}  // namespace aql

}  // namespace arangodb

namespace {

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

static const std::string FLUSH_COLLECTION_FIELD("cid");
static const std::string FLUSH_INDEX_FIELD("iid");
static const std::string FLUSH_VALUE_FIELD("value");

class IResearchLogTopic final : public arangodb::LogTopic {
 public:
  IResearchLogTopic(std::string const& name)
      : arangodb::LogTopic(name, DEFAULT_LEVEL) {
    setIResearchLogLevel(DEFAULT_LEVEL);
  }

  virtual void setLogLevel(arangodb::LogLevel level) override {
    arangodb::LogTopic::setLogLevel(level);
    setIResearchLogLevel(level);
  }

 private:
  static arangodb::LogLevel const DEFAULT_LEVEL = arangodb::LogLevel::INFO;

  typedef std::underlying_type<irs::logger::level_t>::type irsLogLevelType;
  typedef std::underlying_type<arangodb::LogLevel>::type arangoLogLevelType;

  static_assert(static_cast<irsLogLevelType>(irs::logger::IRL_FATAL) ==
                        static_cast<arangoLogLevelType>(arangodb::LogLevel::FATAL) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_ERROR) ==
                        static_cast<arangoLogLevelType>(arangodb::LogLevel::ERR) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_WARN) ==
                        static_cast<arangoLogLevelType>(arangodb::LogLevel::WARN) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_INFO) ==
                        static_cast<arangoLogLevelType>(arangodb::LogLevel::INFO) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_DEBUG) ==
                        static_cast<arangoLogLevelType>(arangodb::LogLevel::DEBUG) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_TRACE) ==
                        static_cast<arangoLogLevelType>(arangodb::LogLevel::TRACE) - 1,
                "inconsistent log level mapping");

  static void setIResearchLogLevel(arangodb::LogLevel level) {
    if (level == arangodb::LogLevel::DEFAULT) {
      level = DEFAULT_LEVEL;
    }

    auto irsLevel = static_cast<irs::logger::level_t>(
        static_cast<arangoLogLevelType>(level) - 1);  // -1 for DEFAULT

    irsLevel = std::max(irsLevel, irs::logger::IRL_FATAL);
    irsLevel = std::min(irsLevel, irs::logger::IRL_TRACE);

    irs::logger::output_le(irsLevel, stderr);
  }
};  // IResearchLogTopic

// template <char const *name>
arangodb::aql::AqlValue dummyFilterFunc(arangodb::aql::ExpressionContext*,
                                        arangodb::transaction::Methods*,
                                        arangodb::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "ArangoSearch filter functions EXISTS, STARTS_WITH, IN_RANGE, PHRASE, MIN_MATCH, "
      "BOOST and ANALYZER "
      " are designed to be used only within a corresponding SEARCH statement "
      "of ArangoSearch view."
      " Please ensure function signature is correct.");
}

arangodb::aql::AqlValue dummyScorerFunc(arangodb::aql::ExpressionContext*,
                                        arangodb::transaction::Methods*,
                                        arangodb::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "ArangoSearch scorer functions BM25() and TFIDF() are designed to "
      "be used only outside SEARCH statement within a context of ArangoSearch "
      "view."
      " Please ensure function signature is correct.");
}

size_t computeThreadPoolSize(size_t threads, size_t threadsLimit) {
  static const size_t MAX_THREADS = 8;  // arbitrary limit on the upper bound of threads in pool
  static const size_t MIN_THREADS = 1;  // at least one thread is required
  auto maxThreads = threadsLimit ? threadsLimit : MAX_THREADS;

  return threads ? threads
                 : std::max(MIN_THREADS,
                            std::min(maxThreads,
                                     size_t(std::thread::hardware_concurrency()) / 4));
}

bool upgradeSignleServerArangoSearchView0_1(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& /*upgradeParams*/) {
  using arangodb::application_features::ApplicationServer;

  // NOTE: during the upgrade 'ClusterFeature' is disabled which means 'ClusterFeature::validateOptions(...)'
  // hasn't been called and server role in 'ServerState' is not set properly.
  // In order to upgrade ArangoSearch views from version 0 to version 1 we need to
  // differentiate between signle server and cluster, therefore we temporary set role in 'ServerState',
  // actually supplied by a user, only for the duration of task to avoid other upgrade tasks, that
  // potentially rely on the original behaviour, to be affected.
  struct ServerRoleGuard {
    ServerRoleGuard() {
      auto const* clusterFeature = ApplicationServer::lookupFeature<arangodb::ClusterFeature>("Cluster");
      auto* state = arangodb::ServerState::instance();

      if (state && clusterFeature && !clusterFeature->isEnabled()) {
        auto const role = arangodb::ServerState::stringToRole(clusterFeature->myRole());

        // only for cluster
        if (role > arangodb::ServerState::ROLE_SINGLE) {
          _originalRole = state->getRole();
          state->setRole(role);
          _state = state;
        }
      }
    }

    ~ServerRoleGuard() {
      if (_state) {
        // restore the original server role
        _state->setRole(_originalRole);
      }
    }

    arangodb::ServerState* _state{};
    arangodb::ServerState::RoleEnum _originalRole{arangodb::ServerState::ROLE_UNDEFINED};
  } guard;

  if (!arangodb::ServerState::instance()->isSingleServer() &&
      !arangodb::ServerState::instance()->isDBServer()) {
    return true;  // not applicable for other ServerState roles
  }

  for (auto& view : vocbase.views()) {
    if (!arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(view.get())) {
      continue;  // not an IResearchView
    }

    arangodb::velocypack::Builder builder;
    arangodb::Result res;

    builder.openObject();
    res = view->properties(builder, true, true);  // get JSON with meta + 'version'
    builder.close();

    if (!res.ok()) {
      LOG_TOPIC("c5dc4", WARN, arangodb::iresearch::TOPIC)
          << "failure to generate persisted definition while upgrading "
             "IResearchView from version 0 to version 1";

      return false;  // definition generation failure
    }

    auto versionSlice =
        builder.slice().get(arangodb::iresearch::StaticStrings::VersionField);

    if (!versionSlice.isNumber<uint32_t>()) {
      LOG_TOPIC("eae1c", WARN, arangodb::iresearch::TOPIC)
          << "failure to find 'version' field while upgrading IResearchView "
             "from version 0 to version 1";

      return false;  // required field is missing
    }

    auto version = versionSlice.getNumber<uint32_t>();

    if (0 != version) {
      continue;  // no upgrade required
    }

    builder.clear();
    builder.openObject();
    res = view->properties(builder, true, false);  // get JSON with end-user definition
    builder.close();

    if (!res.ok()) {
      LOG_TOPIC("d6e30", WARN, arangodb::iresearch::TOPIC)
          << "failure to generate persisted definition while upgrading "
             "IResearchView from version 0 to version 1";

      return false;  // definition generation failure
    }

    irs::utf8_path dataPath;

    auto* dbPathFeature = ApplicationServer::lookupFeature<arangodb::DatabasePathFeature>("DatabasePath");

    if (!dbPathFeature) {
      LOG_TOPIC("67c7e", WARN, arangodb::iresearch::TOPIC)
          << "failure to find feature 'DatabasePath' while upgrading "
             "IResearchView from version 0 to version 1";

      return false;  // required feature is missing
    }

    // original algorithm for computing data-store path
    static const std::string subPath("databases");
    static const std::string dbPath("database-");

    dataPath = irs::utf8_path(dbPathFeature->directory());
    dataPath /= subPath;
    dataPath /= dbPath;
    dataPath += std::to_string(vocbase.id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(view->id());

    res = view->drop();  // drop view (including all links)

    if (!res.ok()) {
      LOG_TOPIC("cb9d1", WARN, arangodb::iresearch::TOPIC)
          << "failure to drop view while upgrading IResearchView from version "
             "0 to version 1";

      return false;  // view drom failure
    }

    // .........................................................................
    // non-recoverable state below here
    // .........................................................................

    // non-version 0 IResearchView implementations no longer drop from vocbase
    // on db-server, do it explicitly
    if (arangodb::ServerState::instance()->isDBServer()) {
      res = arangodb::LogicalViewHelperStorageEngine::drop(*view);

      if (!res.ok()) {
        LOG_TOPIC("bfb3d", WARN, arangodb::iresearch::TOPIC)
            << "failure to drop view from vocbase while upgrading "
               "IResearchView from version 0 to version 1";

        return false;  // view drom failure
      }
    }

    if (arangodb::ServerState::instance()->isSingleServer() ||
        arangodb::ServerState::instance()->isDBServer()) {
      bool exists;

      // remove any stale data-store
      if (!dataPath.exists(exists) || (exists && !dataPath.remove())) {
        LOG_TOPIC("9ab42", WARN, arangodb::iresearch::TOPIC)
            << "failure to remove old data-store path while upgrading "
               "IResearchView from version 0 to version 1, view definition: "
            << builder.slice().toString();

        return false;  // data-store removal failure
      }
    }

    if (arangodb::ServerState::instance()->isDBServer()) {
      continue;  // no need to recreate per-cid view
    }

    // recreate view
    res = arangodb::iresearch::IResearchView::factory().create(view, vocbase,
                                                               builder.slice());

    if (!res.ok()) {
      LOG_TOPIC("f8d19", WARN, arangodb::iresearch::TOPIC)
          << "failure to recreate view while upgrading IResearchView from "
             "version 0 to version 1, error: "
          << res.errorNumber() << " " << res.errorMessage()
          << ", view definition: " << builder.slice().toString();

      return false;  // data-store removal failure
    }
  }

  return true;
}

void registerFilters(arangodb::aql::AqlFunctionFeature& functions) {
  using arangodb::iresearch::addFunction;
  auto flags =
      arangodb::aql::Function::makeFlags(arangodb::aql::Function::Flags::Deterministic,
                                         arangodb::aql::Function::Flags::Cacheable,
                                         arangodb::aql::Function::Flags::CanRunOnDBServer);
  addFunction(functions, {"EXISTS", ".|.,.", flags, &dummyFilterFunc});  // (attribute, [ // "analyzer"|"type"|"string"|"numeric"|"bool"|"null" // ])
  addFunction(functions, {"STARTS_WITH", ".,.|.", flags, &dummyFilterFunc});  // (attribute, prefix, scoring-limit)
  addFunction(functions, {"PHRASE", ".,.|.+", flags, &dummyFilterFunc});  // (attribute, input [, offset, input... ] [, analyzer])
  addFunction(functions, {"IN_RANGE", ".,.,.,.,.", flags, &dummyFilterFunc});  // (attribute, lower, upper, include lower, include upper)
  addFunction(functions, {"MIN_MATCH", ".,.|.+", flags, &dummyFilterFunc});  // (filter expression [, filter expression, ... ], min match count)
  addFunction(functions, {"BOOST", ".,.", flags, &dummyFilterFunc});  // (filter expression, boost)
  addFunction(functions, {"ANALYZER", ".,.", flags, &dummyFilterFunc});  // (filter expression, analyzer)
}

void registerIndexFactory() {
  static const std::map<std::string, arangodb::IndexTypeFactory const*> factories = {
      {"ClusterEngine", &arangodb::iresearch::IResearchLinkCoordinator::factory()},
      {"MMFilesEngine", &arangodb::iresearch::IResearchMMFilesLink::factory()},
      {"RocksDBEngine", &arangodb::iresearch::IResearchRocksDBLink::factory()}};
  auto const& indexType = arangodb::iresearch::DATA_SOURCE_TYPE.name();

  // register 'arangosearch' link
  for (auto& entry : factories) {
    auto* engine =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::StorageEngine>(
            entry.first);

    // valid situation if not running with the specified storage engine
    if (!engine) {
      LOG_TOPIC("a562d", WARN, arangodb::iresearch::TOPIC)
          << "failed to find feature '" << entry.first
          << "' while registering index type '" << indexType << "', skipping";
      continue;
    }

    // ok to const-cast since this should only be called on startup
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(engine->indexFactory());
    auto res = indexFactory.emplace(indexType, *(entry.second));

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res.errorNumber(),
          std::string("failure registering IResearch link factory with index "
                      "factory from feature '") +
              entry.first + "': " + res.errorMessage());
    }
  }
}

void registerRecoveryMarkerHandler() {
  static const arangodb::FlushFeature::FlushRecoveryCallback callback = []( // callback
    TRI_vocbase_t const& vocbase, // marker vocbase
    arangodb::velocypack::Slice const& slice // marker data
  )->arangodb::Result {
    if (!slice.isObject()) {
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        "non-object recovery marker body recieved by the arangosearch handler" // message
      );
    }

    if (!slice.hasKey(FLUSH_COLLECTION_FIELD) // missing field
        || !slice.get(FLUSH_COLLECTION_FIELD).isNumber<TRI_voc_cid_t>()) {
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        "arangosearch handler failed to get collection indentifier from the recovery marker" // message
      );
    }

    if (!slice.hasKey(FLUSH_INDEX_FIELD) // missing field
        || !slice.get(FLUSH_INDEX_FIELD).isNumber<TRI_idx_iid_t>()) {
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        "arangosearch handler failed to get link indentifier from the recovery marker" // message
      );
    }

    auto collection = vocbase.lookupCollection( // collection of the recovery marker
      slice.get(FLUSH_COLLECTION_FIELD).getNumber<TRI_voc_cid_t>() // args
    );

    // arangosearch handler failed to find collection from the recovery marker, possibly already removed
    if (!collection) {
      return arangodb::Result();
    }

    auto link = arangodb::iresearch::IResearchLinkHelper::find( // link of the recovery marker
      *collection, slice.get(FLUSH_INDEX_FIELD).getNumber<TRI_idx_iid_t>() // args
    );

    // arangosearch handler failed to find link from the recovery marker, possibly already removed
    if (!link) {
      return arangodb::Result();
    }

    return link->walFlushMarker(slice.get(FLUSH_VALUE_FIELD));
  };
  auto& type = arangodb::iresearch::DATA_SOURCE_TYPE.name();

  arangodb::FlushFeature::registerFlushRecoveryCallback(type, callback);
}

/// @note must match registerRecoveryMarkerHandler() above
/// @note implemented separately to be closer to registerRecoveryMarkerHandler()
arangodb::iresearch::IResearchFeature::WalFlushCallback registerRecoveryMarkerSubscription(
    arangodb::iresearch::IResearchLink const& link // wal source
) {
  auto* feature = arangodb::application_features::ApplicationServer::lookupFeature< // lookup
    arangodb::FlushFeature // type
  >("Flush"); // name

  if (!feature) {
    LOG_TOPIC("7007e", WARN, arangodb::iresearch::TOPIC)
      << "failed to find feature 'Flush' while registering recovery subscription";

    return {}; // it's an std::function so don't use a constructor or ASAN complains
  }

  auto& type = arangodb::iresearch::DATA_SOURCE_TYPE.name();
  auto& vocbase = link.collection().vocbase();
  auto subscription = feature->registerFlushSubscription(type, vocbase);

  if (!subscription) {
    LOG_TOPIC("df64a", WARN, arangodb::iresearch::TOPIC)
      << "failed to find register subscription with  feature 'Flush' while  registering recovery subscription";

    return {}; // it's an std::function so don't use a constructor or ASAN complains
  }

  auto cid = link.collection().id();
  auto iid = link.id();

  return [cid, iid, subscription]( // callback
    arangodb::velocypack::Slice const& value // args
  )->arangodb::Result {
    arangodb::velocypack::Builder builder;

    builder.openObject();
    builder.add(FLUSH_COLLECTION_FIELD, arangodb::velocypack::Value(cid));
    builder.add(FLUSH_INDEX_FIELD, arangodb::velocypack::Value(iid));
    builder.add(FLUSH_VALUE_FIELD, value);
    builder.close();

    return subscription->commit(builder.slice());
  };
}

void registerScorers(arangodb::aql::AqlFunctionFeature& functions) {
  irs::string_ref const args(".|+");  // positional arguments (attribute [,
                                      // <scorer-specific properties>...]);

  irs::scorers::visit([&functions, &args](irs::string_ref const& name,
                                          irs::text_format::type_id const& args_format) -> bool {
    // ArangoDB, for API consistency, only supports scorers configurable via
    // jSON
    if (irs::text_format::json != args_format) {
      return true;
    }

    std::string upperName = name;

    // AQL function external names are always in upper case
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

    arangodb::iresearch::addFunction(
        functions,
        {
            std::move(upperName), args.c_str(),
            arangodb::aql::Function::makeFlags(arangodb::aql::Function::Flags::Deterministic,
                                               arangodb::aql::Function::Flags::Cacheable,
                                               arangodb::aql::Function::Flags::CanRunOnDBServer),
            &dummyScorerFunc  // function implementation
        });

    return true;
  });
}

void registerRecoveryHelper() {
  auto helper = std::make_shared<arangodb::iresearch::IResearchRocksDBRecoveryHelper>();
  auto res = arangodb::RocksDBEngine::registerRecoveryHelper(helper);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(), "failed to register RocksDB recovery helper");
  }
}

void registerUpgradeTasks() {
  auto* upgrade =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::UpgradeFeature>(
          "Upgrade");

  if (!upgrade) {
    return;  // nothing to register with (OK if no tasks actually need to be applied)
  }

  // move IResearch data-store from IResearchView to IResearchLink
  {
    arangodb::methods::Upgrade::Task task;

    task.name = "upgradeArangoSearch0_1";
    task.description = "store ArangoSearch index on per linked collection basis";
    task.systemFlag = arangodb::methods::Upgrade::Flags::DATABASE_ALL;
    task.clusterFlags = arangodb::methods::Upgrade::Flags::CLUSTER_DB_SERVER_LOCAL  // db-server
                        | arangodb::methods::Upgrade::Flags::CLUSTER_NONE;          // local server
    task.databaseFlags = arangodb::methods::Upgrade::Flags::DATABASE_UPGRADE;
    task.action = &upgradeSignleServerArangoSearchView0_1;
    upgrade->addTask(std::move(task));
  }
}

void registerViewFactory() {
  auto& viewType = arangodb::iresearch::DATA_SOURCE_TYPE;
  auto* viewTypes =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::ViewTypesFeature>();

  if (!viewTypes) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   std::string("failed to find feature '") +
                                       arangodb::ViewTypesFeature::name() +
                                       "' while registering view type '" +
                                       viewType.name() + "'");
  }

  arangodb::Result res;

  // DB server in custer or single-server
  if (arangodb::ServerState::instance()->isCoordinator()) {
    res = viewTypes->emplace(viewType,
                             arangodb::iresearch::IResearchViewCoordinator::factory());
  } else if (arangodb::ServerState::instance()->isDBServer()) {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchView::factory());
  } else if (arangodb::ServerState::instance()->isSingleServer()) {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchView::factory());
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_FAILED,
        std::string("Invalid role for arangosearch view creation."));
  }

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        std::string("failure registering arangosearch view factory: ") + res.errorMessage());
  }
}

arangodb::Result transactionDataSourceRegistrationCallback(
    arangodb::LogicalDataSource& dataSource, arangodb::transaction::Methods& trx) {
  if (arangodb::iresearch::DATA_SOURCE_TYPE != dataSource.type()) {
    return {};  // not an IResearchView (noop)
  }

// TODO FIXME find a better way to look up a LogicalView
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* view = dynamic_cast<arangodb::LogicalView*>(&dataSource);
#else
  auto* view = static_cast<arangodb::LogicalView*>(&dataSource);
#endif

  if (!view) {
    LOG_TOPIC("f42f8", WARN, arangodb::iresearch::TOPIC)
        << "failure to get LogicalView while processing a TransactionState by "
           "IResearchFeature for name '"
        << dataSource.name() << "'";

    return {TRI_ERROR_INTERNAL};
  }

  // TODO FIXME find a better way to look up an IResearch View
  auto& impl = arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*view);

  return arangodb::Result(impl.apply(trx) ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL);
}

void registerTransactionDataSourceRegistrationCallback() {
  if (arangodb::ServerState::instance()->isSingleServer()) {
    arangodb::transaction::Methods::addDataSourceRegistrationCallback(
        &transactionDataSourceRegistrationCallback);
  }
}

std::string const FEATURE_NAME("ArangoSearch");
IResearchLogTopic LIBIRESEARCH("libiresearch");

}  // namespace

namespace arangodb {
namespace iresearch {

bool isFilter(arangodb::aql::Function const& func) noexcept {
  return func.implementation == &dummyFilterFunc;
}

bool isScorer(arangodb::aql::Function const& func) noexcept {
  return func.implementation == &dummyScorerFunc;
}

class IResearchFeature::Async {
 public:
  typedef std::function<bool(size_t& timeoutMsec, bool timeout)> Fn;

  explicit Async(size_t poolSize = 0);
  Async(size_t poolSize, Async&& other);
  ~Async();

  void emplace(std::shared_ptr<ResourceMutex> const& mutex,
               Fn&& fn);  // add an asynchronous task
  void notify() const;    // notify all tasks
  size_t poolSize() { return _pool.size(); }
  void start();

 private:
  struct Pending {
    Fn _fn;                                 // the function to execute
    std::shared_ptr<ResourceMutex> _mutex;  // mutex for the task resources
    std::chrono::system_clock::time_point _timeout;  // when the task should be notified
                                                     // (std::chrono::milliseconds::max() == disabled)

    Pending(std::shared_ptr<ResourceMutex> const& mutex, Fn&& fn)
        : _fn(std::move(fn)),
          _mutex(mutex),
          _timeout(std::chrono::system_clock::time_point::max()) {}
  };

  struct Task : public Pending {
    std::unique_lock<ReadMutex> _lock;  // prevent resource deallocation

    Task(Pending&& pending) : Pending(std::move(pending)) {}
  };

  struct Thread : public arangodb::Thread {
    mutable std::condition_variable _cond;  // trigger task run
    mutable std::mutex _mutex;  // mutex used with '_cond' and '_pending'
    Thread* _next;  // next thread in circular-list (never null!!!) (need to
                    // store pointer for move-assignment)
    std::vector<Pending> _pending;  // pending tasks
    std::atomic<size_t> _size;  // approximate size of the active+pending task list
    std::vector<Task> _tasks;       // the tasks to perform
    std::atomic<bool>* _terminate;  // trigger termination of this thread (need
                                    // to store pointer for move-assignment)
    mutable bool _wasNotified;  // a notification was raised from another thread

    Thread(std::string const& name)
        : arangodb::Thread(name), _next(nullptr), _terminate(nullptr), _wasNotified(false) {}
    Thread(Thread&& other)  // used in constructor before tasks are started
        : arangodb::Thread(other.name()),
          _next(nullptr),
          _terminate(nullptr),
          _wasNotified(false) {}
    ~Thread() { shutdown(); }
    virtual bool isSystem() override {
      return true;
    }  // or start(...) will fail
    virtual void run() override;
  };

  arangodb::basics::ConditionVariable _join;  // mutex to join on
  std::vector<Thread> _pool;  // thread pool (size fixed for the entire life of object)
  std::atomic<bool> _terminate;  // unconditionaly terminate async tasks

  void stop(Thread* redelegate = nullptr);
};

void IResearchFeature::Async::Thread::run() {
  std::vector<Pending> pendingRedelegate;
  std::chrono::system_clock::time_point timeout;
  bool timeoutSet = false;

  for (;;) {
    bool onlyPending;
    auto pendingStart = _tasks.size();

    {
      SCOPED_LOCK_NAMED(_mutex, lock);  // aquire before '_terminate' check so
                                        // that don't miss notify()

      if (_terminate->load()) {
        break;  // termination requested
      }

      // transfer any new pending tasks into active tasks
      for (auto& pending : _pending) {
        _tasks.emplace_back(std::move(pending));  // will aquire resource lock

        auto& task = _tasks.back();

        if (task._mutex) {
          task._lock = std::unique_lock<ReadMutex>(task._mutex->mutex(), std::try_to_lock);

          if (!task._lock.owns_lock()) {
            // if can't lock 'task._mutex' then reasign the task to the next
            // worker
            pendingRedelegate.emplace_back(std::move(task));
          } else if (*(task._mutex)) {
            continue;  // resourceMutex acquisition successful
          }

          _tasks.pop_back();  // resource no longer valid
        }
      }

      _pending.clear();
      _size.store(_tasks.size());

      // do not sleep if a notification was raised or pending tasks were added
      if (_wasNotified || pendingStart < _tasks.size() || !pendingRedelegate.empty()) {
        timeout = std::chrono::system_clock::now();
        timeoutSet = true;
      }

      // sleep until timeout
      if (!timeoutSet) {
        _cond.wait(lock);  // wait forever
      } else {
        _cond.wait_until(lock, timeout);  // wait for timeout or notify
      }

      onlyPending = !_wasNotified && pendingStart < _tasks.size();  // process all tasks if a notification was raised
      _wasNotified = false;  // ignore notification since woke up

      if (_terminate->load()) {  // check again after sleep
        break;                   // termination requested
      }
    }

    timeoutSet = false;

    // transfer some tasks to '_next' if have too many
    if (!pendingRedelegate.empty() ||
        (_size.load() > _next->_size.load() * 2 && _tasks.size() > 1)) {
      SCOPED_LOCK(_next->_mutex);

      // reasign to '_next' tasks that failed resourceMutex aquisition
      while (!pendingRedelegate.empty()) {
        _next->_pending.emplace_back(std::move(pendingRedelegate.back()));
        pendingRedelegate.pop_back();
        ++_next->_size;
      }

      // transfer some tasks to '_next' if have too many
      while (_size.load() > _next->_size.load() * 2 && _tasks.size() > 1) {
        _next->_pending.emplace_back(std::move(_tasks.back()));
        _tasks.pop_back();
        ++_next->_size;
        --_size;
      }

      _next->_cond.notify_all();  // notify thread about a new task (thread may
                                  // be sleeping indefinitely)
    }

    for (size_t i = onlyPending ? pendingStart : 0,
                count = _tasks.size();  // optimization to skip previously run
                                        // tasks if a notificationw as not raised
         i < count;) {
      auto& task = _tasks[i];
      auto exec = std::chrono::system_clock::now() >= task._timeout;
      size_t timeoutMsec = 0;  // by default reschedule for the same time span

      try {
        if (!task._fn(timeoutMsec, exec)) {
          if (i + 1 < count) {
            std::swap(task, _tasks[count - 1]);  // swap 'i' with tail
          }

          _tasks.pop_back();  // remove stale tail
          --count;

          continue;
        }
      } catch (...) {
        LOG_TOPIC("d43ee", WARN, arangodb::iresearch::TOPIC)
            << "caught error while executing asynchronous task";
        IR_LOG_EXCEPTION();
        timeoutMsec = 0;  // sleep until previously set timeout
      }

      // task reschedule time modification requested
      if (timeoutMsec) {
        task._timeout = std::chrono::system_clock::now() +
                        std::chrono::milliseconds(timeoutMsec);
      }

      timeout = timeoutSet ? std::min(timeout, task._timeout) : task._timeout;
      timeoutSet = true;
      ++i;
    }
  }

  // ...........................................................................
  // move all tasks back into _pending in case the may neeed to be reasigned
  // ...........................................................................
  SCOPED_LOCK_NAMED(_mutex, lock);  // '_pending' may be modified asynchronously

  for (auto& task : pendingRedelegate) {
    _pending.emplace_back(std::move(task));
  }

  for (auto& task : _tasks) {
    _pending.emplace_back(std::move(task));
  }

  _tasks.clear();
}

IResearchFeature::Async::Async(size_t poolSize) : _terminate(false) {
  poolSize = std::max(size_t(1), poolSize);  // need at least one thread

  for (size_t i = 0; i < poolSize; ++i) {
    _pool.emplace_back(std::string("ArangoSearch #") + std::to_string(i));
  }

  auto* last = &(_pool.back());

  // build circular list
  for (auto& thread : _pool) {
    last->_next = &thread;
    last = &thread;
    thread._terminate = &_terminate;
  }
}

IResearchFeature::Async::Async(size_t poolSize, Async&& other)
    : Async(poolSize) {
  other.stop(&_pool[0]);
}

IResearchFeature::Async::~Async() { stop(); }

void IResearchFeature::Async::emplace(std::shared_ptr<ResourceMutex> const& mutex, Fn&& fn) {
  if (!fn) {
    return;  // skip empty functers
  }

  auto& thread = _pool[0];
  SCOPED_LOCK(thread._mutex);
  thread._pending.emplace_back(mutex, std::move(fn));
  ++thread._size;
  thread._cond.notify_all();  // notify thread about a new task (thread may be
                              // sleeping indefinitely)
}

void IResearchFeature::Async::notify() const {
  // notify all threads
  for (auto& thread : _pool) {
    SCOPED_LOCK(thread._mutex);
    thread._cond.notify_all();
    thread._wasNotified = true;
  }
}

void IResearchFeature::Async::start() {
  // start threads
  for (auto& thread : _pool) {
    thread.start(&_join);
  }

  LOG_TOPIC("c1b64", DEBUG, arangodb::iresearch::TOPIC)
      << "started " << _pool.size() << " ArangoSearch maintenance thread(s)";
}

void IResearchFeature::Async::stop(Thread* redelegate /*= nullptr*/) {
  _terminate.store(true);  // request stop asynchronous tasks
  notify();                // notify all threads

  CONDITION_LOCKER(lock, _join);

  // join with all threads in pool
  for (auto& thread : _pool) {
    if (thread.hasStarted()) {
      while (thread.isRunning()) {
        _join.wait();
      }
    }

    // redelegate all thread tasks if requested
    if (redelegate) {
      SCOPED_LOCK(redelegate->_mutex);

      for (auto& task : thread._pending) {
        redelegate->_pending.emplace_back(std::move(task));
        ++redelegate->_size;
      }

      thread._pending.clear();
      redelegate->_cond.notify_all();  // notify thread about a new task (thread
                                       // may be sleeping indefinitely)
    }
  }
}

IResearchFeature::IResearchFeature(arangodb::application_features::ApplicationServer& server)
    : ApplicationFeature(server, IResearchFeature::name()),
      _async(std::make_unique<Async>()),
      _running(false),
      _threads(0),
      _threadsLimit(0) {
  setOptional(true);
  startsAfter("V8Phase");
  startsAfter("IResearchAnalyzer");  // used for retrieving IResearch analyzers
                                     // for functions
  startsAfter("AQLFunctions");
}

void IResearchFeature::async(std::shared_ptr<ResourceMutex> const& mutex, Async::Fn&& fn) {
  _async->emplace(mutex, std::move(fn));
}

void IResearchFeature::asyncNotify() const { _async->notify(); }

void IResearchFeature::beginShutdown() {
  _running.store(false);
  ApplicationFeature::beginShutdown();
}

void IResearchFeature::collectOptions(std::shared_ptr<arangodb::options::ProgramOptions> options) {
  auto section = FEATURE_NAME;

  _running.store(false);
  std::transform(section.begin(), section.end(), section.begin(), ::tolower);
  ApplicationFeature::collectOptions(options);
  options->addSection(section,
                      std::string("Configure the ") + FEATURE_NAME + " feature");
  options->addOption(std::string("--") + section + ".threads",
                     "the exact number of threads to use for asynchronous "
                     "tasks (0 == autodetect)",
                     new arangodb::options::UInt64Parameter(&_threads));
  options->addOption(std::string("--") + section + ".threads-limit",
                     "upper limit to the autodetected number of threads to use "
                     "for asynchronous tasks (0 == use default)",
                     new arangodb::options::UInt64Parameter(&_threadsLimit));
}

/*static*/ std::string const& IResearchFeature::name() { return FEATURE_NAME; }

void IResearchFeature::prepare() {
  TRI_ASSERT(isEnabled());

  _running.store(false);
  ApplicationFeature::prepare();

  // load all known codecs
  ::iresearch::formats::init();

  // load all known scorers
  ::iresearch::scorers::init();

  // register 'arangosearch' index
  registerIndexFactory();

  // register 'arangosearch' view
  registerViewFactory();

  // register 'arangosearch' Transaction DataSource registration callback
  registerTransactionDataSourceRegistrationCallback();

  registerRecoveryHelper();

  // register 'arangosearch' flush marker recovery handler
  registerRecoveryMarkerHandler();

  // start the async task thread pool
  if (!ServerState::instance()->isCoordinator() // not a coordinator
      && !ServerState::instance()->isAgent()) {
    auto poolSize = computeThreadPoolSize(_threads, _threadsLimit);

    if (_async->poolSize() != poolSize) {
      _async = std::make_unique<Async>(poolSize, std::move(*_async));
    }

    _async->start();
  }
}

void IResearchFeature::start() {
  TRI_ASSERT(isEnabled());

  ApplicationFeature::start();

  // register IResearchView filters
  {
    auto* functions =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::aql::AqlFunctionFeature>(
            "AQLFunctions");

    if (functions) {
      registerFilters(*functions);
      registerScorers(*functions);
    } else {
      LOG_TOPIC("462d7", WARN, arangodb::iresearch::TOPIC)
          << "failure to find feature 'AQLFunctions' while registering "
             "arangosearch filters";
    }
  }

  registerUpgradeTasks();  // register tasks after UpgradeFeature::prepare() has finished

  _running.store(true);
}

void IResearchFeature::stop() {
  TRI_ASSERT(isEnabled());
  _running.store(false);
  ApplicationFeature::stop();
}

void IResearchFeature::unprepare() {
  TRI_ASSERT(isEnabled());
  _running.store(false);
  ApplicationFeature::unprepare();
}

void IResearchFeature::validateOptions(std::shared_ptr<arangodb::options::ProgramOptions> options) {
  _running.store(false);
  ApplicationFeature::validateOptions(options);
}

/*static*/ IResearchFeature::WalFlushCallback IResearchFeature::walFlushCallback( // callback
    IResearchLink const& link // subscription target
) {
  return registerRecoveryMarkerSubscription(link);
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
