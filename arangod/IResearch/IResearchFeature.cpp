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
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

// otherwise define conflict between 3rdParty\date\include\date\date.h and 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
  #include "date/date.h"
#endif

#include "search/scorers.hpp"
#include "utils/async_utils.hpp"
#include "utils/log.hpp"

#include "ApplicationServerHelper.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlValue.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/application-exit.h"
#include "Basics/ConditionLocker.h"
#include "Basics/NumberOfCores.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Containers/SmallVector.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "IResearch/Containers.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchRocksDBLink.h"
#include "IResearch/IResearchRocksDBRecoveryHelper.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
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
#include "VocBase/LogicalDataSource.h"
#include "VocBase/LogicalView.h"

using namespace std::chrono_literals;

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

// -----------------------------------------------------------------------------
// --SECTION--                                         ArangoSearc AQL functions
// -----------------------------------------------------------------------------

arangodb::aql::AqlValue dummyFilterFunc(arangodb::aql::ExpressionContext*,
                                        arangodb::transaction::Methods*,
                                        arangodb::containers::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "ArangoSearch filter functions EXISTS, PHRASE "
      " are designed to be used only within a corresponding SEARCH statement "
      "of ArangoSearch view."
      " Please ensure function signature is correct.");
}

/// function body for ArangoSearch context functions ANALYZER/BOOST.
/// Just returns its first argument as outside ArangoSearch context
/// there is nothing to do with search stuff, but optimization could roll.
arangodb::aql::AqlValue contextFunc(arangodb::aql::ExpressionContext*,
                                    arangodb::transaction::Methods*,
                                    arangodb::containers::SmallVector<arangodb::aql::AqlValue> const& args) {
  TRI_ASSERT(!args.empty()); //ensured by function signature
  return args[0];
}

/// Check whether prefix is a value prefix
inline bool isPrefix(arangodb::velocypack::StringRef const& prefix, arangodb::velocypack::StringRef const& value) {
  return prefix.size() <= value.size() && value.substr(0, prefix.size()) == prefix;
}

/// Register invalid argument warning
inline arangodb::aql::AqlValue errorAqlValue(arangodb::aql::ExpressionContext* ctx, char const* afn) {
  arangodb::aql::registerInvalidArgumentWarning(ctx, afn);
  return arangodb::aql::AqlValue{arangodb::aql::AqlValueHintNull{}};
}

/// Executes STARTS_WITH function with const parameters locally the same way
/// it will be done in ArangoSearch at runtime
/// This will allow optimize out STARTS_WITH call if all arguments are const
arangodb::aql::AqlValue startsWithFunc(arangodb::aql::ExpressionContext* ctx,
                                       arangodb::transaction::Methods*,
                                       arangodb::containers::SmallVector<arangodb::aql::AqlValue> const& args) {
  static char const* AFN = "STARTS_WITH";

  auto const argc = args.size();
  TRI_ASSERT(argc >= 2 && argc <= 4); // ensured by function signature
  auto& value = args[0];

  if (!value.isString()) {
    return errorAqlValue(ctx, AFN);
  }
  auto const valueRef = value.slice().stringRef();

  auto result = false;

  auto& prefixes = args[1];
  if (prefixes.isArray()) {
    auto const size = static_cast<int64_t>(prefixes.length());
    int64_t minMatchCount = arangodb::iresearch::FilterConstants::DefaultStartsWithMinMatchCount;
    if (argc > 2) {
      auto& minMatchCountValue = args[2];
      if (!minMatchCountValue.isNumber()) {
        return errorAqlValue(ctx, AFN);
      }
      minMatchCount = minMatchCountValue.toInt64();
      if (minMatchCount < 0) {
        return errorAqlValue(ctx, AFN);
      }
    }
    if (0 == minMatchCount) {
      result = true;
    } else if (minMatchCount <= size) {
      int64_t matchedCount = 0;
      for (int64_t i = 0; i < size; ++i) {
        auto mustDestroy = false;
        auto prefix = prefixes.at(i, mustDestroy, false);
        arangodb::aql::AqlValueGuard guard{prefix, mustDestroy};
        if (!prefix.isString()) {
          return errorAqlValue(ctx, AFN);
        }
        if (isPrefix(prefix.slice().stringRef(), valueRef) && ++matchedCount == minMatchCount) {
          result = true;
          break;
        }
      }
    }
  } else {
    if (!prefixes.isString()) {
      return errorAqlValue(ctx, AFN);
    }
    result = isPrefix(prefixes.slice().stringRef(), valueRef);
  }
  return arangodb::aql::AqlValue{arangodb::aql::AqlValueHintBool{result}};
}

/// Executes MIN_MATCH function with const parameters locally the same way
/// it will be done in ArangoSearch at runtime
/// This will allow optimize out MIN_MATCH call if all arguments are const
arangodb::aql::AqlValue minMatchFunc(arangodb::aql::ExpressionContext* ctx,
                                     arangodb::transaction::Methods*,
                                     arangodb::containers::SmallVector<arangodb::aql::AqlValue> const& args) {
  static char const* AFN = "MIN_MATCH";

  TRI_ASSERT(args.size() > 1); // ensured by function signature
  auto& minMatchValue = args.back();
  if (ADB_UNLIKELY(!minMatchValue.isNumber())) {
    return errorAqlValue(ctx, AFN);
  }

  auto matchesLeft = minMatchValue.toInt64();
  const auto argsCount = args.size() - 1;
  for (size_t i = 0; i < argsCount && matchesLeft > 0; ++i) {
    auto& currValue = args[i];
    if (currValue.toBoolean()) {
      matchesLeft--;
    }
  }

  return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(matchesLeft == 0));
}

arangodb::aql::AqlValue dummyScorerFunc(arangodb::aql::ExpressionContext*,
                                        arangodb::transaction::Methods*,
                                        arangodb::containers::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "ArangoSearch scorer functions BM25() and TFIDF() are designed to "
      "be used only outside SEARCH statement within a context of ArangoSearch "
      "view."
      " Please ensure function signature is correct.");
}

////////////////////////////////////////////////////////////////////////////////
/// @class IResearchLogTopic
/// @brief Log topic implementation for IResearch
////////////////////////////////////////////////////////////////////////////////
class IResearchLogTopic final : public arangodb::LogTopic {
 public:
  explicit IResearchLogTopic(std::string const& name)
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

  static void log_appender(void* context, const char* function, const char* file, int line,
                           irs::logger::level_t level, const char* message,
                           size_t message_len);
  static void setIResearchLogLevel(arangodb::LogLevel level) {
    if (level == arangodb::LogLevel::DEFAULT) {
      level = DEFAULT_LEVEL;
    }

    auto irsLevel = static_cast<irs::logger::level_t>(
        static_cast<arangoLogLevelType>(level) - 1);  // -1 for DEFAULT

    irsLevel = std::max(irsLevel, irs::logger::IRL_FATAL);
    irsLevel = std::min(irsLevel, irs::logger::IRL_TRACE);
    irs::logger::output_le(irsLevel, log_appender, nullptr);
  }
};  // IResearchLogTopic

uint32_t computeIdleThreadsCount(uint32_t idleThreads, uint32_t threads) noexcept {
  if (0 == idleThreads) {
    return std::max(threads/2, 1U);
  } else {
    return std::min(idleThreads, threads);
  }
}

uint32_t computeThreadsCount(uint32_t threads, uint32_t threadsLimit, uint32_t div) noexcept {
  TRI_ASSERT(div);
  constexpr uint32_t MAX_THREADS = 8;  // arbitrary limit on the upper bound of threads in pool
  constexpr uint32_t MIN_THREADS = 1;  // at least one thread is required

  return std::max(MIN_THREADS,
                  std::min(threadsLimit ? threadsLimit : MAX_THREADS,
                           threads ? threads : uint32_t(arangodb::NumberOfCores::getValue()) / div));
}

bool upgradeSingleServerArangoSearchView0_1(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& /*upgradeParams*/) {
  using arangodb::application_features::ApplicationServer;

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
    res = view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence); // get JSON with meta + 'version'
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

    auto const version = versionSlice.getNumber<uint32_t>();

    if (0 != version) {
      continue;  // no upgrade required
    }

    builder.clear();
    builder.openObject();
    res = view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties); // get JSON with end-user definition
    builder.close();

    if (!res.ok()) {
      LOG_TOPIC("d6e30", WARN, arangodb::iresearch::TOPIC)
          << "failure to generate persisted definition while upgrading "
             "IResearchView from version 0 to version 1";

      return false;  // definition generation failure
    }

    irs::utf8_path dataPath;

    auto& server = vocbase.server();
    if (!server.hasFeature<arangodb::DatabasePathFeature>()) {
      LOG_TOPIC("67c7e", WARN, arangodb::iresearch::TOPIC)
          << "failure to find feature 'DatabasePath' while upgrading "
             "IResearchView from version 0 to version 1";

      return false;  // required feature is missing
    }
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();

    // original algorithm for computing data-store path
    static const std::string subPath("databases");
    static const std::string dbPath("database-");

    dataPath = irs::utf8_path(dbPathFeature.directory());
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
      LOG_TOPIC("f8d20", WARN, arangodb::iresearch::TOPIC)
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
                                         arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
                                         arangodb::aql::Function::Flags::CanRunOnDBServerOneShard);
  addFunction(functions, { "EXISTS", ".|.,.", flags, &dummyFilterFunc });  // (attribute, [ // "analyzer"|"type"|"string"|"numeric"|"bool"|"null" // ])
  addFunction(functions, { "STARTS_WITH", ".,.|.,.", flags, &startsWithFunc });  // (attribute, [ '[' ] prefix [, prefix, ... ']' ] [, scoring-limit|min-match-count ] [, scoring-limit ])
  addFunction(functions, { "PHRASE", ".,.|.+", flags, &dummyFilterFunc });  // (attribute, input [, offset, input... ] [, analyzer])
  addFunction(functions, { "MIN_MATCH", ".,.|.+", flags, &minMatchFunc });  // (filter expression [, filter expression, ... ], min match count)
  addFunction(functions, { "BOOST", ".,.", flags, &contextFunc });  // (filter expression, boost)
  addFunction(functions, { "ANALYZER", ".,.", flags, &contextFunc });  // (filter expression, analyzer)
}

namespace {
template <typename T>
void registerSingleFactory(
    std::map<std::type_index, std::shared_ptr<arangodb::IndexTypeFactory>> const& m,
    arangodb::application_features::ApplicationServer& server) {
  TRI_ASSERT(m.find(std::type_index(typeid(T))) != m.end());
  arangodb::IndexTypeFactory& factory = *m.find(std::type_index(typeid(T)))->second;
  auto const& indexType = arangodb::iresearch::DATA_SOURCE_TYPE.name();
  if (server.hasFeature<T>()) {
    auto& engine = server.getFeature<T>();
    auto& engineFactory = const_cast<arangodb::IndexFactory&>(engine.indexFactory());
    arangodb::Result res = engineFactory.emplace(indexType, factory);
    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res.errorNumber(),
          std::string("failure registering IResearch link factory with index "
                      "factory from feature '") +
              engine.name() + "': " + res.errorMessage());
    }
  }
}
}  // namespace

void registerIndexFactory(std::map<std::type_index, std::shared_ptr<arangodb::IndexTypeFactory>>& m,
                          arangodb::application_features::ApplicationServer& server) {
  m.emplace(std::type_index(typeid(arangodb::ClusterEngine)),
            arangodb::iresearch::IResearchLinkCoordinator::createFactory(server));
  registerSingleFactory<arangodb::ClusterEngine>(m, server);
  m.emplace(std::type_index(typeid(arangodb::RocksDBEngine)),
            arangodb::iresearch::IResearchRocksDBLink::createFactory(server));
  registerSingleFactory<arangodb::RocksDBEngine>(m, server);
}

void registerScorers(arangodb::aql::AqlFunctionFeature& functions) {
  irs::string_ref const args(".|+");  // positional arguments (attribute [,
                                      // <scorer-specific properties>...]);

  irs::scorers::visit([&functions, &args](irs::string_ref const& name,
                                          irs::type_info const& args_format) -> bool {
    // ArangoDB, for API consistency, only supports scorers configurable via
    // jSON
    if (irs::type<irs::text_format::json>::id() != args_format.id()) {
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
                                               arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
                                               arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
            &dummyScorerFunc  // function implementation
        });

    LOG_TOPIC("f42f9", TRACE, arangodb::iresearch::TOPIC)
        << "registered ArangoSearch scorer '" << upperName << "'";

    return true;
  });
}

void registerRecoveryHelper() {
  auto helper =
      std::make_shared<arangodb::iresearch::IResearchRocksDBRecoveryHelper>();
  auto res = arangodb::RocksDBEngine::registerRecoveryHelper(helper);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(), "failed to register RocksDB recovery helper");
  }
}

void registerUpgradeTasks(arangodb::application_features::ApplicationServer& server) {
  if (!server.hasFeature<arangodb::UpgradeFeature>()) {
    return;  // nothing to register with (OK if no tasks actually need to be applied)
  }
  auto& upgrade = server.getFeature<arangodb::UpgradeFeature>();

  // move IResearch data-store from IResearchView to IResearchLink
  {
    arangodb::methods::Upgrade::Task task;

    task.name = "upgradeArangoSearch0_1";
    task.description = "store ArangoSearch index on per linked collection basis";
    task.systemFlag = arangodb::methods::Upgrade::Flags::DATABASE_ALL;
    task.clusterFlags = arangodb::methods::Upgrade::Flags::CLUSTER_DB_SERVER_LOCAL  // db-server
                        | arangodb::methods::Upgrade::Flags::CLUSTER_NONE           // local server
                        | arangodb::methods::Upgrade::Flags::CLUSTER_LOCAL;
    task.databaseFlags = arangodb::methods::Upgrade::Flags::DATABASE_UPGRADE;
    task.action = &upgradeSingleServerArangoSearchView0_1;
    upgrade.addTask(std::move(task));
  }
}

void registerViewFactory(arangodb::application_features::ApplicationServer& server) {
  auto& viewType = arangodb::iresearch::DATA_SOURCE_TYPE;
  auto& viewTypes = server.getFeature<arangodb::ViewTypesFeature>();

  arangodb::Result res;

  // DB server in custer or single-server
  if (arangodb::ServerState::instance()->isCoordinator()) {
    res = viewTypes.emplace(viewType,
                            arangodb::iresearch::IResearchViewCoordinator::factory());
  } else if (arangodb::ServerState::instance()->isDBServer()) {
    res = viewTypes.emplace(viewType, arangodb::iresearch::IResearchView::factory());
  } else if (arangodb::ServerState::instance()->isSingleServer()) {
    res = viewTypes.emplace(viewType, arangodb::iresearch::IResearchView::factory());
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

std::string const THREADS_PARAM("--arangosearch.threads");
std::string const THREADS_LIMIT_PARAM("--arangosearch.threads-limit");
std::string const COMMIT_THREADS_PARAM("--arangosearch.commit-threads");
std::string const COMMIT_THREADS_IDLE_PARAM("--arangosearch.commit-threads-idle");
std::string const CONSOLIDATION_THREADS_PARAM("--arangosearch.consolidation-threads");
std::string const CONSOLIDATION_THREADS_IDLE_PARAM("--arangosearch.consolidation-threads-idle");

void IResearchLogTopic::log_appender(void* /*context*/, const char* function, const char* file, int line,
                                     irs::logger::level_t level, const char* message,
                                     size_t message_len) {
  auto const arangoLevel = static_cast<arangodb::LogLevel>(level + 1);
  std::string msg(message, message_len); 
  arangodb::Logger::log(function, file, line, arangoLevel, LIBIRESEARCH.id(), msg);
}

}  // namespace

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class IResearchAsync
/// @brief helper class for holding thread groups
////////////////////////////////////////////////////////////////////////////////
class IResearchAsync{
 public:
  using ThreadPool = irs::async_utils::thread_pool;

  ~IResearchAsync() {
    stop();
  }

  ThreadPool& get(ThreadGroup id)
#ifndef ARANGODB_ENABLE_FAILURE_TESTS
  noexcept
#endif
  {
    TRI_IF_FAILURE("IResearchFeature::testGroupAccess") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(static_cast<size_t>(id) < 2);
    return (ThreadGroup::_0 == id) ? _0 : _1;
  }

  void stop() noexcept {
    try { _0.stop(true); } catch (...) { }
    try { _1.stop(true); } catch (...) { }
  }

 private:
  ThreadPool _0{0, 0, IR_NATIVE_STRING("ARS-0")};
  ThreadPool _1{0, 0, IR_NATIVE_STRING("ARS-1")};
}; // IResearchAsync

bool isFilter(arangodb::aql::Function const& func) noexcept {
  return func.implementation == &dummyFilterFunc ||
         func.implementation == &contextFunc ||
         func.implementation == &minMatchFunc ||
         func.implementation == &startsWithFunc ||
         func.implementation == &aql::Functions::LevenshteinMatch ||
         func.implementation == &aql::Functions::Like ||
         func.implementation == &aql::Functions::NgramMatch ||
         func.implementation == &aql::Functions::InRange;
}

bool isScorer(arangodb::aql::Function const& func) noexcept {
  return func.implementation == &dummyScorerFunc;
}

IResearchFeature::IResearchFeature(arangodb::application_features::ApplicationServer& server)
    : ApplicationFeature(server, IResearchFeature::name()),
      _async(std::make_unique<IResearchAsync>()),
      _running(false),
      _consolidationThreads(0),
      _consolidationThreadsIdle(0),
      _commitThreads(0),
      _commitThreadsIdle(0),
      _threads(0),
      _threadsLimit(0) {
  setOptional(true);
  startsAfter<application_features::V8FeaturePhase>();
  startsAfter<IResearchAnalyzerFeature>();
  startsAfter<aql::AqlFunctionFeature>();
}

void IResearchFeature::beginShutdown() {
  _running.store(false);
}

void IResearchFeature::collectOptions(std::shared_ptr<arangodb::options::ProgramOptions> options) {
  _running.store(false);
  options->addSection("arangosearch",
                      std::string("Configure the ") + FEATURE_NAME + " feature");
  options->addOption(THREADS_PARAM,
                     "the exact number of threads to use for asynchronous "
                     "tasks (0 == autodetect)",
                     new options::UInt32Parameter(&_threads))
                     .setDeprecatedIn(30705);
  options->addOption(THREADS_LIMIT_PARAM,
                     "upper limit to the autodetected number of threads to use "
                     "for asynchronous tasks (0 == use default)",
                     new options::UInt32Parameter(&_threadsLimit))
                     .setDeprecatedIn(30705);
  options->addOption(CONSOLIDATION_THREADS_PARAM,
                     "upper limit to the allowed number of consolidation threads "
                     "(0 == autodetect)",
                     new options::UInt32Parameter(&_consolidationThreads))
                     .setIntroducedIn(30705);
  options->addOption(CONSOLIDATION_THREADS_IDLE_PARAM,
                     "upper limit to the allowed number of idle threads to use "
                     "for consolidation tasks (0 == autodetect)",
                     new options::UInt32Parameter(&_consolidationThreadsIdle))
                     .setIntroducedIn(30705);
  options->addOption(COMMIT_THREADS_PARAM,
                     "upper limit to the allowed number of commit threads "
                     "(0 == autodetect)",
                     new options::UInt32Parameter(&_commitThreads))
                     .setIntroducedIn(30705);
  options->addOption(COMMIT_THREADS_IDLE_PARAM,
                     "upper limit to the allowed number of idle threads to use "
                     "for commit tasks (0 == autodetect)",
                     new options::UInt32Parameter(&_commitThreadsIdle))
                     .setIntroducedIn(30705);
}

void IResearchFeature::validateOptions(std::shared_ptr<arangodb::options::ProgramOptions> options) {
  auto const& args = options->processingResult();
  bool const threadsSet = args.touched(THREADS_PARAM);
  bool const threadsLimitSet = args.touched(THREADS_LIMIT_PARAM);
  bool const commitThreadsSet = args.touched(COMMIT_THREADS_PARAM);
  bool const commitThreadsIdleSet = args.touched(COMMIT_THREADS_IDLE_PARAM);
  bool const consolidationThreadsSet = args.touched(CONSOLIDATION_THREADS_PARAM);
  bool const consolidationThreadsIdleSet = args.touched(CONSOLIDATION_THREADS_IDLE_PARAM);

  uint32_t threadsLimit = static_cast<uint32_t>(4*arangodb::NumberOfCores::getValue());

  if ((threadsLimitSet || threadsSet) &&
      !commitThreadsSet && !consolidationThreadsSet) {
    // backwards compatibility
    threadsLimit              = std::min(threadsLimit, _threadsLimit);
    uint32_t const threads    = computeThreadsCount(_threads, threadsLimit, 4);
    _commitThreads            = std::max(threads/2, 1U);
    _consolidationThreads     = _commitThreads;
  } else {
    _commitThreads            = computeThreadsCount(_commitThreads, threadsLimit, 6);
    _consolidationThreads     = computeThreadsCount(_consolidationThreads, threadsLimit, 6);
  }

  _commitThreadsIdle          = commitThreadsIdleSet
    ? computeIdleThreadsCount(_commitThreadsIdle, _commitThreads)
    : _commitThreads;

  _consolidationThreadsIdle   = consolidationThreadsIdleSet
    ? computeIdleThreadsCount(_consolidationThreadsIdle, _consolidationThreads)
    : _consolidationThreads;

  _running.store(false);
}

/*static*/ std::string const& IResearchFeature::name() { return FEATURE_NAME; }

void IResearchFeature::prepare() {
  TRI_ASSERT(isEnabled());

  _running.store(false);

  // load all known codecs
  ::irs::formats::init();

  // load all known scorers
  ::irs::scorers::init();

  // register 'arangosearch' index
  registerIndexFactory(_factories, server());

  // register 'arangosearch' view
  registerViewFactory(server());

  // register 'arangosearch' Transaction DataSource registration callback
  registerTransactionDataSourceRegistrationCallback();

  registerRecoveryHelper();

  // register filters
  if (server().hasFeature<arangodb::aql::AqlFunctionFeature>()) {
    auto& functions = server().getFeature<arangodb::aql::AqlFunctionFeature>();
    registerFilters(functions);
    registerScorers(functions);
  } else {
    LOG_TOPIC("462d7", WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'AQLFunctions' while registering "
           "arangosearch filters";
  }

  // ensure no tasks are scheduled and no threads are started
  TRI_ASSERT(std::make_tuple(size_t(0), size_t(0), size_t(0)) == stats(ThreadGroup::_0));
  TRI_ASSERT(std::make_tuple(size_t(0), size_t(0), size_t(0)) == stats(ThreadGroup::_1));

  // submit tasks to ensure that at least 1 worker for each group is started
  if (ServerState::instance()->isDBServer() ||
      ServerState::instance()->isSingleServer()) {
    _startState = std::make_shared<State>();

    auto submitTask = [this](ThreadGroup group) {
      return queue(group, 0ms, [state = _startState]() noexcept {
        {
          auto lock = irs::make_lock_guard(state->mtx);
          ++state->counter;
        }
        state->cv.notify_one();
      });
    };

    if (!submitTask(ThreadGroup::_0) ||
        !submitTask(ThreadGroup::_1)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_SYS_ERROR,
        "failed to initialize ArangoSearch maintenance threads");
    }

    TRI_ASSERT(std::make_tuple(size_t(0), size_t(1), size_t(0)) == stats(ThreadGroup::_0));
    TRI_ASSERT(std::make_tuple(size_t(0), size_t(1), size_t(0)) == stats(ThreadGroup::_1));
  }
}

void IResearchFeature::start() {
  TRI_ASSERT(isEnabled());

  // register tasks after UpgradeFeature::prepare() has finished
  registerUpgradeTasks(server());

  // ensure that at least 1 worker for each group is started
  if (ServerState::instance()->isDBServer() ||
      ServerState::instance()->isSingleServer()) {
    TRI_ASSERT(_startState);
    TRI_ASSERT(_commitThreads && _commitThreadsIdle);
    TRI_ASSERT(_consolidationThreads && _consolidationThreadsIdle);

    _async->get(ThreadGroup::_0).limits(_commitThreads, _commitThreadsIdle);
    _async->get(ThreadGroup::_1).limits(_consolidationThreads, _consolidationThreadsIdle);

    LOG_TOPIC("c1b63", INFO, arangodb::iresearch::TOPIC)
        << "ArangoSearch maintenance: "
        << "[" << _commitThreadsIdle << ".." << _commitThreads << "] commit thread(s), "
        << "[" << _consolidationThreadsIdle << ".." << _consolidationThreads << "] consolidation thread(s)";

    auto lock = irs::make_unique_lock(_startState->mtx);
    if (!_startState->cv.wait_for(lock, 60s,
                                  [this](){ return _startState->counter == 2; })) {


      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_SYS_ERROR,
        "failed to start ArangoSearch maintenance threads");
    }

    _startState = nullptr;
  }

  _running.store(true);
}

void IResearchFeature::stop() {
  TRI_ASSERT(isEnabled());
  _async->stop();
  _running.store(false);
}

void IResearchFeature::unprepare() {
  TRI_ASSERT(isEnabled());
  _running.store(false);
}

bool IResearchFeature::queue(
    ThreadGroup id,
    std::chrono::steady_clock::duration delay,
    std::function<void()>&& fn) {
  try {

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("IResearchFeature::queue") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    switch (id) {
      case ThreadGroup::_0:
        TRI_IF_FAILURE("IResearchFeature::queueGroup0") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        break;
      case ThreadGroup::_1:
        TRI_IF_FAILURE("IResearchFeature::queueGroup1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        break;
      default:
        TRI_ASSERT(false);
        break;
    }
#endif

    if (_async->get(id).run(std::move(fn), delay)) {
      return true;
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("c1b64", WARN, arangodb::iresearch::TOPIC)
      << "Caught exception while sumbitting a task to thread group '"
      << std::to_string(std::underlying_type_t<ThreadGroup>(id))
      << "' error '" << e.what() << "'";
  } catch (...) {
    LOG_TOPIC("c1b65", WARN, arangodb::iresearch::TOPIC)
      << "Caught an exception while sumbitting a task to thread group '"
      << std::to_string(std::underlying_type_t<ThreadGroup>(id)) << "'";
  }

  LOG_TOPIC("c1b66", ERR, arangodb::iresearch::TOPIC)
    << "Failed to submit a task to thread group '"
    << std::to_string(std::underlying_type_t<ThreadGroup>(id)) << "'";

  return false;
}

std::tuple<size_t, size_t, size_t> IResearchFeature::stats(ThreadGroup id) const {
  return _async->get(id).stats();
}

std::pair<size_t, size_t> IResearchFeature::limits(ThreadGroup id) const {
  return _async->get(id).limits();
}

template <typename Engine, typename std::enable_if_t<std::is_base_of_v<StorageEngine, Engine>, int>>
IndexTypeFactory& IResearchFeature::factory() {
  TRI_ASSERT(_factories.find(std::type_index(typeid(Engine))) != _factories.end());
  return *_factories.find(std::type_index(typeid(Engine)))->second;
}
template IndexTypeFactory& IResearchFeature::factory<arangodb::ClusterEngine>();
template IndexTypeFactory& IResearchFeature::factory<arangodb::RocksDBEngine>();

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
