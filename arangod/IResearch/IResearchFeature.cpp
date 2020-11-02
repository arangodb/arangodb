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
#include "utils/log.hpp"

#include "ApplicationServerHelper.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlValue.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
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
                                        arangodb::aql::AstNode const&,
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
                                    arangodb::aql::AstNode const&,
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
                                       arangodb::aql::AstNode const&,
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
                                     arangodb::aql::AstNode const&,
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
                                        arangodb::aql::AstNode const&,
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

size_t computeThreadPoolSize(size_t threads, size_t threadsLimit) {
  static const size_t MAX_THREADS = 8;  // arbitrary limit on the upper bound of threads in pool
  static const size_t MIN_THREADS = 1;  // at least one thread is required
  auto maxThreads = threadsLimit ? threadsLimit : MAX_THREADS;

  return threads ? threads
                 : std::max(MIN_THREADS,
                            std::min(maxThreads,
                                     arangodb::NumberOfCores::getValue() / 4));
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
    dataPath += std::to_string(view->id().id());

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
                                         arangodb::aql::Function::Flags::CanRunOnDBServer);
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
                                               arangodb::aql::Function::Flags::CanRunOnDBServer),
            &dummyScorerFunc  // function implementation
        });

    LOG_TOPIC("f42f9", TRACE, arangodb::iresearch::TOPIC)
        << "registered ArangoSearch scorer '" << upperName << "'";

    return true;
  });
}

void registerRecoveryHelper(arangodb::application_features::ApplicationServer& server) {
  auto helper =
      std::make_shared<arangodb::iresearch::IResearchRocksDBRecoveryHelper>(server);
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

void IResearchLogTopic::log_appender(void* context, const char* function, const char* file, int line,
                                     irs::logger::level_t level, const char* message,
                                     size_t message_len) {
  auto const arangoLevel = static_cast<arangodb::LogLevel>(level + 1);
  std::string msg(message, message_len); 
  arangodb::Logger::log("9afd3", function, file, line, arangoLevel, LIBIRESEARCH.id(), msg);
}

}  // namespace

namespace arangodb {
namespace iresearch {

bool isFilter(arangodb::aql::Function const& func) noexcept {
  return func.implementation == &dummyFilterFunc ||
         func.implementation == &contextFunc ||
         func.implementation == &minMatchFunc ||
         func.implementation == &startsWithFunc ||
         func.implementation == &aql::Functions::GeoContains ||
         func.implementation == &aql::Functions::GeoInRange ||
         func.implementation == &aql::Functions::GeoIntersects ||
         func.implementation == &aql::Functions::LevenshteinMatch ||
         func.implementation == &aql::Functions::Like ||
         func.implementation == &aql::Functions::NgramMatch ||
         func.implementation == &aql::Functions::InRange;
}

bool isScorer(arangodb::aql::Function const& func) noexcept {
  return func.implementation == &dummyScorerFunc;
}

class IResearchFeature::Async {
 public:
  typedef std::function<bool(size_t& timeoutMsec, bool timeout)> Fn;

  explicit Async(IResearchFeature& feature, size_t poolSize = 0);
  Async(IResearchFeature& feature, size_t poolSize, Async&& other);
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

    explicit Task(Pending&& pending) : Pending(std::move(pending)) {}
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

    explicit Thread(arangodb::application_features::ApplicationServer& server,
                    std::string const& name)
        : arangodb::Thread(server, name),
          _next(nullptr),
          _terminate(nullptr),
          _wasNotified(false) {}
    Thread(Thread&& other)  // used in constructor before tasks are started
        : arangodb::Thread(other._server, other.name()),
          _next(nullptr),
          _terminate(nullptr),
          _wasNotified(false) {}
    ~Thread() { shutdown(); }
    virtual bool isSystem() const override {
      return true;
    }  // or start(...) will fail
    virtual void run() override;
  };

  arangodb::basics::ConditionVariable _join;  // mutex to join on
  std::vector<Thread> _pool;  // thread pool (size fixed for the entire life of object)
  std::atomic<bool> _terminate;  // unconditionally terminate async tasks

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
      SCOPED_LOCK_NAMED(_mutex, lock);  // acquire before '_terminate' check so
                                        // that don't miss notify()

      if (_terminate->load()) {
        break;  // termination requested
      }

      // transfer any new pending tasks into active tasks
      for (auto& pending : _pending) {
        _tasks.emplace_back(std::move(pending));  // will acquire resource lock

        auto& task = _tasks.back();

        if (task._mutex) {
          task._lock = std::unique_lock<ReadMutex>(task._mutex->mutex(), std::try_to_lock);

          if (!task._lock.owns_lock()) {
            // if can't lock 'task._mutex' then reassign the task to the next
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

      onlyPending = !_wasNotified; // process all tasks if a notification was raised
      _wasNotified = false;  // ignore notification since woke up

      if (_terminate->load()) {  // check again after sleep
        break;                   // termination requested
      }
    }

    timeoutSet = false;

    // transfer some tasks to '_next' if have too many
    if (!pendingRedelegate.empty() ||
        (_size.load() > _next->_size.load() * 2 && _tasks.size() > 1)) {
      {
        SCOPED_LOCK(_next->_mutex);

        // reassign to '_next' tasks that failed resourceMutex aquisition
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
      }
      _next->_cond.notify_all();  // notify thread about a new task (thread may
                                  // be sleeping indefinitely)
    }

    onlyPending &= (pendingStart < _tasks.size());

    for (size_t i = onlyPending ? pendingStart : 0,
                count = _tasks.size();  // optimization to skip previously run
                                        // tasks if a notification was not raised
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
  // move all tasks back into _pending in case the may need to be reassigned
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

IResearchFeature::Async::Async(IResearchFeature& feature, size_t poolSize)
    : _terminate(false) {
  poolSize = std::max(size_t(1), poolSize);  // need at least one thread

  for (size_t i = 0; i < poolSize; ++i) {
    _pool.emplace_back(feature.server(),
                       std::string("ArangoSearch #") + std::to_string(i));
  }

  auto* last = &(_pool.back());

  // build circular list
  for (auto& thread : _pool) {
    last->_next = &thread;
    last = &thread;
    thread._terminate = &_terminate;
  }
}

IResearchFeature::Async::Async(IResearchFeature& feature, size_t poolSize, Async&& other)
    : Async(feature, poolSize) {
  other.stop(&_pool[0]);
}

IResearchFeature::Async::~Async() { stop(); }

void IResearchFeature::Async::emplace(std::shared_ptr<ResourceMutex> const& mutex, Fn&& fn) {
  if (!fn) {
    return;  // skip empty functers
  }

  auto& thread = _pool[0];
  {
    SCOPED_LOCK(thread._mutex);
    thread._pending.emplace_back(mutex, std::move(fn));
    ++thread._size;
  }
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

  LOG_TOPIC("c1b64", INFO, arangodb::iresearch::TOPIC)
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
      {
        SCOPED_LOCK(redelegate->_mutex);

        for (auto& task : thread._pending) {
          redelegate->_pending.emplace_back(std::move(task));
          ++redelegate->_size;
        }

        thread._pending.clear();
      }
      redelegate->_cond.notify_all();  // notify thread about a new task (thread
                                       // may be sleeping indefinitely)
    }
  }
}

IResearchFeature::IResearchFeature(arangodb::application_features::ApplicationServer& server)
    : ApplicationFeature(server, IResearchFeature::name()),
      _async(std::make_unique<Async>(*this)),
      _running(false),
      _threads(0),
      _threadsLimit(0) {
  setOptional(true);
  startsAfter<arangodb::application_features::V8FeaturePhase>();

  startsAfter<IResearchAnalyzerFeature>();  // used for retrieving IResearch
                                            // analyzers for functions
  startsAfter<arangodb::aql::AqlFunctionFeature>();
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
  registerIndexFactory(_factories, server());

  // register 'arangosearch' view
  registerViewFactory(server());

  // register 'arangosearch' Transaction DataSource registration callback
  registerTransactionDataSourceRegistrationCallback();

  registerRecoveryHelper(server());

  // start the async task thread pool
  if (!ServerState::instance()->isCoordinator() // not a coordinator
      && !ServerState::instance()->isAgent()) {
    auto poolSize = computeThreadPoolSize(_threads, _threadsLimit);

    if (_async->poolSize() != poolSize) {
      _async = std::make_unique<Async>(*this, poolSize, std::move(*_async));
    }

    _async->start();
  }
}

void IResearchFeature::start() {
  TRI_ASSERT(isEnabled());

  ApplicationFeature::start();

  // register IResearchView filters
  {
    if (server().hasFeature<arangodb::aql::AqlFunctionFeature>()) {
      auto& functions = server().getFeature<arangodb::aql::AqlFunctionFeature>();
      registerFilters(functions);
      registerScorers(functions);
    } else {
      LOG_TOPIC("462d7", WARN, arangodb::iresearch::TOPIC)
          << "failure to find feature 'AQLFunctions' while registering "
             "arangosearch filters";
    }
  }

  registerUpgradeTasks(server());  // register tasks after UpgradeFeature::prepare() has finished

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

template <typename Engine, typename std::enable_if<std::is_base_of<StorageEngine, Engine>::value, int>::type>
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
