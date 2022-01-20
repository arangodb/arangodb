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
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

// otherwise define conflict between 3rdParty\date\include\date\date.h and
// 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
#include "date/date.h"
#endif

#include "search/scorers.hpp"
#include "utils/async_utils.hpp"
#include "utils/log.hpp"
#include "utils/file_utils.hpp"

#include "ApplicationServerHelper.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/application-exit.h"
#include "Basics/ConditionLocker.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#ifdef USE_ENTERPRISE
#include "Cluster/ClusterMethods.h"
#endif
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
#include "RocksDBEngine/RocksDBLogValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/LogicalView.h"

using namespace std::chrono_literals;

namespace arangodb {

namespace aql {
class Query;
}  // namespace aql

}  // namespace arangodb

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

aql::AqlValue dummyFilterFunc(aql::ExpressionContext*, aql::AstNode const&,
                              containers::SmallVector<aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "ArangoSearch filter functions EXISTS, PHRASE "
      " are designed to be used only within a corresponding SEARCH statement "
      "of ArangoSearch view."
      " Please ensure function signature is correct.");
}

// Function body for ArangoSearch context functions ANALYZER/BOOST.
// Just returns its first argument as outside ArangoSearch context
// there is nothing to do with search stuff, but optimization could roll.
aql::AqlValue contextFunc(aql::ExpressionContext* ctx, aql::AstNode const&,
                          containers::SmallVector<aql::AqlValue> const& args) {
  TRI_ASSERT(ctx);
  TRI_ASSERT(!args.empty());  // ensured by function signature

  aql::AqlValueMaterializer materializer(&ctx->trx().vpackOptions());
  return aql::AqlValue{materializer.slice(args[0], true)};
}

// Register invalid argument warning
inline aql::AqlValue errorAqlValue(aql::ExpressionContext* ctx,
                                   char const* afn) {
  aql::registerInvalidArgumentWarning(ctx, afn);
  return aql::AqlValue{aql::AqlValueHintNull{}};
}

// Executes STARTS_WITH function with const parameters locally the same way
// it will be done in ArangoSearch at runtime
// This will allow optimize out STARTS_WITH call if all arguments are const
aql::AqlValue startsWithFunc(
    aql::ExpressionContext* ctx, aql::AstNode const&,
    containers::SmallVector<aql::AqlValue> const& args) {
  static char const* AFN = "STARTS_WITH";

  auto const argc = args.size();
  TRI_ASSERT(argc >= 2 && argc <= 4);  // ensured by function signature
  auto& value = args[0];

  if (!value.isString()) {
    return errorAqlValue(ctx, AFN);
  }
  auto const valueRef = value.slice().stringView();

  auto result = false;

  auto& prefixes = args[1];
  if (prefixes.isArray()) {
    auto const size = static_cast<int64_t>(prefixes.length());
    int64_t minMatchCount = FilterConstants::DefaultStartsWithMinMatchCount;
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
        aql::AqlValueGuard guard{prefix, mustDestroy};
        if (!prefix.isString()) {
          return errorAqlValue(ctx, AFN);
        }
        if (valueRef.starts_with(prefix.slice().stringView()) &&
            ++matchedCount == minMatchCount) {
          result = true;
          break;
        }
      }
    }
  } else {
    if (!prefixes.isString()) {
      return errorAqlValue(ctx, AFN);
    }
    result = valueRef.starts_with(prefixes.slice().stringView());
  }
  return aql::AqlValue{aql::AqlValueHintBool{result}};
}

/// Executes MIN_MATCH function with const parameters locally the same way
/// it will be done in ArangoSearch at runtime
/// This will allow optimize out MIN_MATCH call if all arguments are const
aql::AqlValue minMatchFunc(aql::ExpressionContext* ctx, aql::AstNode const&,
                           containers::SmallVector<aql::AqlValue> const& args) {
  static char const* AFN = "MIN_MATCH";

  TRI_ASSERT(args.size() > 1);  // ensured by function signature
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

  return aql::AqlValue(aql::AqlValueHintBool(matchesLeft == 0));
}

aql::AqlValue dummyScorerFunc(aql::ExpressionContext*, aql::AstNode const&,
                              containers::SmallVector<aql::AqlValue> const&) {
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
class IResearchLogTopic final : public LogTopic {
 public:
  explicit IResearchLogTopic(std::string const& name)
      : LogTopic(name, kDefaultLevel) {
    setIResearchLogLevel(kDefaultLevel);
  }

  virtual void setLogLevel(LogLevel level) override {
    LogTopic::setLogLevel(level);
    setIResearchLogLevel(level);
  }

 private:
  static LogLevel const kDefaultLevel = LogLevel::INFO;

  typedef std::underlying_type<irs::logger::level_t>::type irsLogLevelType;
  typedef std::underlying_type<LogLevel>::type arangoLogLevelType;

  static_assert(static_cast<irsLogLevelType>(irs::logger::IRL_FATAL) ==
                        static_cast<arangoLogLevelType>(LogLevel::FATAL) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_ERROR) ==
                        static_cast<arangoLogLevelType>(LogLevel::ERR) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_WARN) ==
                        static_cast<arangoLogLevelType>(LogLevel::WARN) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_INFO) ==
                        static_cast<arangoLogLevelType>(LogLevel::INFO) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_DEBUG) ==
                        static_cast<arangoLogLevelType>(LogLevel::DEBUG) - 1 &&
                    static_cast<irsLogLevelType>(irs::logger::IRL_TRACE) ==
                        static_cast<arangoLogLevelType>(LogLevel::TRACE) - 1,
                "inconsistent log level mapping");

  static void log_appender(void* context, const char* function,
                           const char* file, int line,
                           irs::logger::level_t level, const char* message,
                           size_t message_len);
  static void setIResearchLogLevel(LogLevel level) {
    if (level == LogLevel::DEFAULT) {
      level = kDefaultLevel;
    }

    auto irsLevel = static_cast<irs::logger::level_t>(
        static_cast<arangoLogLevelType>(level) - 1);  // -1 for DEFAULT

    irsLevel = std::max(irsLevel, irs::logger::IRL_FATAL);
    irsLevel = std::min(irsLevel, irs::logger::IRL_TRACE);
    irs::logger::output_le(irsLevel, log_appender, nullptr);
  }
};  // IResearchLogTopic

uint32_t computeIdleThreadsCount(uint32_t idleThreads,
                                 uint32_t threads) noexcept {
  if (0 == idleThreads) {
    return std::max(threads / 2, 1U);
  } else {
    return std::min(idleThreads, threads);
  }
}

uint32_t computeThreadsCount(uint32_t threads, uint32_t threadsLimit,
                             uint32_t div) noexcept {
  TRI_ASSERT(div);
  constexpr uint32_t MAX_THREADS =
      8;  // arbitrary limit on the upper bound of threads in pool
  constexpr uint32_t MIN_THREADS = 1;  // at least one thread is required

  return std::max(
      MIN_THREADS,
      std::min(threadsLimit ? threadsLimit : MAX_THREADS,
               threads ? threads : uint32_t(NumberOfCores::getValue()) / div));
}

bool upgradeArangoSearchLinkCollectionName(
    TRI_vocbase_t& vocbase, velocypack::Slice const& /*upgradeParams*/) {
  using application_features::ApplicationServer;
  if (!ServerState::instance()->isDBServer()) {
    return true;  // not applicable for other ServerState roles
  }
  auto& selector = vocbase.server().getFeature<EngineSelectorFeature>();
  auto& clusterInfo =
      vocbase.server().getFeature<ClusterFeature>().clusterInfo();
  // persist collection names in links
  for (auto& collection : vocbase.collections(false)) {
    auto indexes = collection->getIndexes();
    std::string clusterCollectionName;
    if (!collection->shardIds()->empty()) {
      unsigned tryCount{60};
      do {
        LOG_TOPIC("423b3", TRACE, arangodb::iresearch::TOPIC)
            << " Checking collection '" << collection->name()
            << "' in database '" << vocbase.name() << "'";
        // we use getCollectionNameForShard as getCollectionNT here is still not
        // available but shard-collection mapping is loaded eventually
        clusterCollectionName =
            clusterInfo.getCollectionNameForShard(collection->name());
        if (!clusterCollectionName.empty()) {
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      } while (--tryCount);
    } else {
      clusterCollectionName = collection->name();
    }
    if (!clusterCollectionName.empty()) {
      LOG_TOPIC("773b4", TRACE, arangodb::iresearch::TOPIC)
          << " Processing collection " << clusterCollectionName;
#ifdef USE_ENTERPRISE
      ClusterMethods::realNameFromSmartName(clusterCollectionName);
#endif
      for (auto& index : indexes) {
        if (index->type() == Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK) {
          auto indexPtr = dynamic_cast<IResearchLink*>(index.get());
          if (indexPtr) {
            LOG_TOPIC("d6edb", TRACE, arangodb::iresearch::TOPIC)
                << "Checking collection name '" << clusterCollectionName
                << "' for link " << indexPtr->id().id();
            if (indexPtr->setCollectionName(clusterCollectionName)) {
              LOG_TOPIC("b269d", INFO, arangodb::iresearch::TOPIC)
                  << "Setting collection name '" << clusterCollectionName
                  << "' for link " << indexPtr->id().id();
              if (selector.engineName() == RocksDBEngine::EngineName) {
                auto& engine = selector.engine<RocksDBEngine>();
                auto builder = collection->toVelocyPackIgnore(
                    {"path", "statusString"}, LogicalDataSource::Serialization::
                                                  PersistenceWithInProgress);
                auto res = engine.writeCreateCollectionMarker(
                    vocbase.id(), collection->id(), builder.slice(),
                    RocksDBLogValue::Empty());
                if (res.fail()) {
                  LOG_TOPIC("50ace", WARN, arangodb::iresearch::TOPIC)
                      << "Unable to store updated link information on upgrade "
                         "for collection '"
                      << clusterCollectionName << "' for link "
                      << indexPtr->id().id() << ": " << res.errorMessage();
                }
#ifdef ARANGODB_USE_GOOGLE_TESTS
              } else if (selector.engineName() !=
                         "Mock") {  // for unit tests just ignore write to
                                    // storage
#else
              } else {
#endif
                TRI_ASSERT(false);
                LOG_TOPIC("d6edc", WARN, arangodb::iresearch::TOPIC)
                    << "Unsupported engine '" << selector.engineName()
                    << "' for link upgrade task";
              }
            }
          }
        }
      }
    } else {
      LOG_TOPIC("d61d3", WARN, arangodb::iresearch::TOPIC)
          << "Failed to find collection name for shard '" << collection->name()
          << "'!";
    }
  }
  return true;
}

bool upgradeSingleServerArangoSearchView0_1(
    TRI_vocbase_t& vocbase, velocypack::Slice const& /*upgradeParams*/) {
  using application_features::ApplicationServer;

  if (!ServerState::instance()->isSingleServer() &&
      !ServerState::instance()->isDBServer()) {
    return true;  // not applicable for other ServerState roles
  }

  for (auto& view : vocbase.views()) {
    if (!LogicalView::cast<IResearchView>(view.get())) {
      continue;  // not an IResearchView
    }

    velocypack::Builder builder;

    builder.openObject();
    Result res = view->properties(
        builder,
        LogicalDataSource::Serialization::Persistence);  // get JSON with meta +
                                                         // 'version'
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
    // get JSON with end-user definition
    res =
        view->properties(builder, LogicalDataSource::Serialization::Properties);
    builder.close();

    if (!res.ok()) {
      LOG_TOPIC("d6e30", WARN, arangodb::iresearch::TOPIC)
          << "failure to generate persisted definition while upgrading "
             "IResearchView from version 0 to version 1";

      return false;  // definition generation failure
    }

    irs::utf8_path dataPath;

    auto& server = vocbase.server();
    if (!server.hasFeature<DatabasePathFeature>()) {
      LOG_TOPIC("67c7e", WARN, arangodb::iresearch::TOPIC)
          << "failure to find feature 'DatabasePath' while upgrading "
             "IResearchView from version 0 to version 1";

      return false;  // required feature is missing
    }
    auto& dbPathFeature = server.getFeature<DatabasePathFeature>();

    // original algorithm for computing data-store path
    dataPath = irs::utf8_path(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(vocbase.id());
    dataPath /= arangodb::iresearch::StaticStrings::DataSourceType;
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
    if (ServerState::instance()->isDBServer()) {
      res = LogicalViewHelperStorageEngine::drop(*view);

      if (!res.ok()) {
        LOG_TOPIC("bfb3d", WARN, arangodb::iresearch::TOPIC)
            << "failure to drop view from vocbase while upgrading "
               "IResearchView from version 0 to version 1";

        return false;  // view drom failure
      }
    }

    if (ServerState::instance()->isSingleServer() ||
        ServerState::instance()->isDBServer()) {
      bool exists;

      // remove any stale data-store
      if (!irs::file_utils::exists_directory(exists, dataPath.c_str()) ||
          (exists && !irs::file_utils::remove(dataPath.c_str()))) {
        LOG_TOPIC("9ab42", WARN, arangodb::iresearch::TOPIC)
            << "failure to remove old data-store path while upgrading "
               "IResearchView from version 0 to version 1, view definition: "
            << builder.slice().toString();

        return false;  // data-store removal failure
      }
    }

    if (ServerState::instance()->isDBServer()) {
      continue;  // no need to recreate per-cid view
    }

    // recreate view
    res = arangodb::iresearch::IResearchView::factory().create(
        view, vocbase, builder.slice(), true);

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

void registerFilters(aql::AqlFunctionFeature& functions) {
  using arangodb::iresearch::addFunction;

  auto flags = aql::Function::makeFlags(
      aql::Function::Flags::Deterministic, aql::Function::Flags::Cacheable,
      aql::Function::Flags::CanRunOnDBServerCluster,
      aql::Function::Flags::CanRunOnDBServerOneShard,
      aql::Function::Flags::CanUseInAnalyzer);

  auto flagsNoAnalyzer = aql::Function::makeFlags(
      aql::Function::Flags::Deterministic, aql::Function::Flags::Cacheable,
      aql::Function::Flags::CanRunOnDBServerCluster,
      aql::Function::Flags::CanRunOnDBServerOneShard);

  // (attribute, [ // "analyzer"|"type"|"string"|"numeric"|"bool"|"null" // ]).
  // cannot be used in analyzers!
  addFunction(functions,
              {"EXISTS", ".|.,.", flagsNoAnalyzer, &dummyFilterFunc});

  // (attribute, [ '[' ] prefix [, prefix, ... ']' ] [,
  // scoring-limit|min-match-count ] [, scoring-limit ])
  addFunction(functions, {"STARTS_WITH", ".,.|.,.", flags, &startsWithFunc});

  // (attribute, input [, offset, input... ] [, analyzer])
  // cannot be used in analyzers!
  addFunction(functions,
              {"PHRASE", ".,.|.+", flagsNoAnalyzer, &dummyFilterFunc});

  // (filter expression [, filter expression, ... ], min match count)
  addFunction(functions, {"MIN_MATCH", ".,.|.+", flags, &minMatchFunc});

  // (filter expression, boost)
  addFunction(functions, {"BOOST", ".,.", flags, &contextFunc});

  // (filter expression, analyzer)
  // cannot be used in analyzers!
  addFunction(functions, {"ANALYZER", ".,.", flagsNoAnalyzer, &contextFunc});
}

namespace {
template<typename T>
void registerSingleFactory(
    std::map<std::type_index, std::shared_ptr<IndexTypeFactory>> const& m,
    application_features::ApplicationServer& server) {
  TRI_ASSERT(m.find(std::type_index(typeid(T))) != m.end());
  IndexTypeFactory& factory = *m.find(std::type_index(typeid(T)))->second;
  if (server.hasFeature<T>()) {
    auto& engine = server.getFeature<T>();
    auto& engineFactory = const_cast<IndexFactory&>(engine.indexFactory());
    Result res = engineFactory.emplace(
        std::string{arangodb::iresearch::StaticStrings::DataSourceType},
        factory);
    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res.errorNumber(),
          basics::StringUtils::concatT(
              "failure registering IResearch link factory with index "
              "factory from feature '",
              engine.name(), "': ", res.errorMessage()));
    }
  }
}
}  // namespace

void registerIndexFactory(
    std::map<std::type_index, std::shared_ptr<IndexTypeFactory>>& m,
    application_features::ApplicationServer& server) {
  m.emplace(
      std::type_index(typeid(ClusterEngine)),
      arangodb::iresearch::IResearchLinkCoordinator::createFactory(server));
  registerSingleFactory<ClusterEngine>(m, server);
  m.emplace(std::type_index(typeid(RocksDBEngine)),
            arangodb::iresearch::IResearchRocksDBLink::createFactory(server));
  registerSingleFactory<RocksDBEngine>(m, server);
}

void registerScorers(aql::AqlFunctionFeature& functions) {
  // positional arguments (attribute [<scorer-specific properties>...]);
  irs::string_ref constexpr args(".|+");

  irs::scorers::visit(
      [&functions, &args](irs::string_ref const& name,
                          irs::type_info const& args_format) -> bool {
        // ArangoDB, for API consistency, only supports scorers configurable via
        // jSON
        if (irs::type<irs::text_format::json>::id() != args_format.id()) {
          return true;
        }

        auto upperName = static_cast<std::string>(name);

        // AQL function external names are always in upper case
        std::transform(upperName.begin(), upperName.end(), upperName.begin(),
                       ::toupper);

        // scorers are not usable in analyzers
        arangodb::iresearch::addFunction(
            functions, {std::move(upperName), args.c_str(),
                        aql::Function::makeFlags(
                            aql::Function::Flags::Deterministic,
                            aql::Function::Flags::Cacheable,
                            aql::Function::Flags::CanRunOnDBServerCluster,
                            aql::Function::Flags::CanRunOnDBServerOneShard),
                        &dummyScorerFunc});

        LOG_TOPIC("f42f9", TRACE, arangodb::iresearch::TOPIC)
            << "registered ArangoSearch scorer '" << upperName << "'";

        return true;
      });
}

void registerRecoveryHelper(application_features::ApplicationServer& server) {
  auto helper = std::make_shared<IResearchRocksDBRecoveryHelper>(server);
  auto res = RocksDBEngine::registerRecoveryHelper(helper);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(), "failed to register RocksDB recovery helper");
  }
}

void registerUpgradeTasks(application_features::ApplicationServer& server) {
  if (!server.hasFeature<UpgradeFeature>()) {
    return;  // nothing to register with (OK if no tasks actually need to be
             // applied)
  }
  auto& upgrade = server.getFeature<UpgradeFeature>();

  // move IResearch data-store from IResearchView to IResearchLink
  {
    methods::Upgrade::Task task;

    task.name = "upgradeArangoSearch0_1";
    task.description =
        "store ArangoSearch index on per linked collection basis";
    task.systemFlag = methods::Upgrade::Flags::DATABASE_ALL;
    task.clusterFlags =
        methods::Upgrade::Flags::CLUSTER_DB_SERVER_LOCAL  // db-server
        | methods::Upgrade::Flags::CLUSTER_NONE           // local server
        | methods::Upgrade::Flags::CLUSTER_LOCAL;
    task.databaseFlags = methods::Upgrade::Flags::DATABASE_UPGRADE |
                         // seal the task after execution
                         methods::Upgrade::Flags::DATABASE_ONLY_ONCE;
    task.action = &upgradeSingleServerArangoSearchView0_1;
    upgrade.addTask(std::move(task));
  }

  // store collection name in IResearchLinkMeta for cluster
  {
    methods::Upgrade::Task task;

    task.name = "upgradeArangoSearchLinkCollectionName";
    task.description = "store collection name in ArangoSearch Link`s metadata";
    task.systemFlag = methods::Upgrade::Flags::DATABASE_ALL;
    // will be run only by cluster bootstrap and database init (latter case it
    // will just do nothing but flags don`t allow to distinguih cases)
    task.clusterFlags = methods::Upgrade::Flags::CLUSTER_DB_SERVER_LOCAL |
                        methods::Upgrade::Flags::CLUSTER_LOCAL;  // db-server
    task.databaseFlags = methods::Upgrade::Flags::DATABASE_EXISTING |
                         // seal the task after execution
                         methods::Upgrade::Flags::DATABASE_ONLY_ONCE;
    task.action = &upgradeArangoSearchLinkCollectionName;
    upgrade.addTask(std::move(task));
  }
}

void registerViewFactory(application_features::ApplicationServer& server) {
  static_assert(IResearchView::typeInfo() ==
                IResearchViewCoordinator::typeInfo());
  constexpr std::string_view kViewType{IResearchView::typeInfo().second};

  Result res;

  // DB server in custer or single-server
  if (auto& viewTypes = server.getFeature<ViewTypesFeature>();
      ServerState::instance()->isCoordinator()) {
    res = viewTypes.emplace(kViewType, IResearchViewCoordinator::factory());
  } else if (ServerState::instance()->isDBServer() ||
             ServerState::instance()->isSingleServer()) {
    res = viewTypes.emplace(kViewType, IResearchView::factory());
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_FAILED,
        std::string("Invalid role for arangosearch view creation."));
  }

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        basics::StringUtils::concatT(
            "failure registering arangosearch view factory: ",
            res.errorMessage()));
  }
}

Result transactionDataSourceRegistrationCallback(LogicalDataSource& dataSource,
                                                 transaction::Methods& trx) {
  if (LogicalView::category() != dataSource.category()) {
    return {};  // not a view
  }

// TODO FIXME find a better way to look up a LogicalView
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* view = dynamic_cast<LogicalView*>(&dataSource);
#else
  auto* view = static_cast<LogicalView*>(&dataSource);
#endif

  if (!view) {
    LOG_TOPIC("f42f8", WARN, arangodb::iresearch::TOPIC)
        << "failure to get LogicalView while processing a TransactionState by "
           "IResearchFeature for name '"
        << dataSource.name() << "'";

    return {TRI_ERROR_INTERNAL};
  }

  if (ViewType::kSearch != view->type()) {
    return {};  // not a search view
  }

  // TODO FIXME find a better way to look up an IResearch View
  auto& impl = LogicalView::cast<IResearchView>(*view);

  return {impl.apply(trx) ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL};
}

void registerTransactionDataSourceRegistrationCallback() {
  if (ServerState::instance()->isSingleServer()) {
    transaction::Methods::addDataSourceRegistrationCallback(
        &transactionDataSourceRegistrationCallback);
  }
}

IResearchLogTopic LIBIRESEARCH("libiresearch");

std::string const THREADS_PARAM("--arangosearch.threads");
std::string const THREADS_LIMIT_PARAM("--arangosearch.threads-limit");
std::string const COMMIT_THREADS_PARAM("--arangosearch.commit-threads");
std::string const COMMIT_THREADS_IDLE_PARAM(
    "--arangosearch.commit-threads-idle");
std::string const CONSOLIDATION_THREADS_PARAM(
    "--arangosearch.consolidation-threads");
std::string const CONSOLIDATION_THREADS_IDLE_PARAM(
    "--arangosearch.consolidation-threads-idle");

void IResearchLogTopic::log_appender(void* /*context*/, const char* function,
                                     const char* file, int line,
                                     irs::logger::level_t level,
                                     const char* message, size_t message_len) {
  auto const arangoLevel = static_cast<LogLevel>(level + 1);
  std::string msg(message, message_len);
  Logger::log("9afd3", function, file, line, arangoLevel, LIBIRESEARCH.id(),
              msg);
}

}  // namespace

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class IResearchAsync
/// @brief helper class for holding thread groups
////////////////////////////////////////////////////////////////////////////////
class IResearchAsync {
 public:
  using ThreadPool = irs::async_utils::thread_pool;

  ~IResearchAsync() { stop(); }

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
    try {
      _0.stop(true);
    } catch (...) {
    }
    try {
      _1.stop(true);
    } catch (...) {
    }
  }

 private:
  ThreadPool _0{0, 0, IR_NATIVE_STRING("ARS-0")};
  ThreadPool _1{0, 0, IR_NATIVE_STRING("ARS-1")};
};  // IResearchAsync

bool isFilter(aql::Function const& func) noexcept {
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

bool isScorer(aql::Function const& func) noexcept {
  return func.implementation == &dummyScorerFunc;
}

IResearchFeature::IResearchFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature(server, std::string{name()}),
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

void IResearchFeature::beginShutdown() { _running.store(false); }

void IResearchFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  _running.store(false);
  options->addSection("arangosearch", std::string{name()}.append(" feature"));
  options
      ->addOption(THREADS_PARAM,
                  "the exact number of threads to use for asynchronous "
                  "tasks (0 == autodetect)",
                  new options::UInt32Parameter(&_threads))
      .setDeprecatedIn(30705);
  options
      ->addOption(THREADS_LIMIT_PARAM,
                  "upper limit to the autodetected number of threads to use "
                  "for asynchronous tasks (0 == use default)",
                  new options::UInt32Parameter(&_threadsLimit))
      .setDeprecatedIn(30705);
  options
      ->addOption(CONSOLIDATION_THREADS_PARAM,
                  "upper limit to the allowed number of consolidation threads "
                  "(0 == autodetect)",
                  new options::UInt32Parameter(&_consolidationThreads))
      .setIntroducedIn(30705);
  options
      ->addOption(CONSOLIDATION_THREADS_IDLE_PARAM,
                  "upper limit to the allowed number of idle threads to use "
                  "for consolidation tasks (0 == autodetect)",
                  new options::UInt32Parameter(&_consolidationThreadsIdle))
      .setIntroducedIn(30705);
  options
      ->addOption(COMMIT_THREADS_PARAM,
                  "upper limit to the allowed number of commit threads "
                  "(0 == autodetect)",
                  new options::UInt32Parameter(&_commitThreads))
      .setIntroducedIn(30705);
  options
      ->addOption(COMMIT_THREADS_IDLE_PARAM,
                  "upper limit to the allowed number of idle threads to use "
                  "for commit tasks (0 == autodetect)",
                  new options::UInt32Parameter(&_commitThreadsIdle))
      .setIntroducedIn(30705);
}

void IResearchFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& args = options->processingResult();
  bool const threadsSet = args.touched(THREADS_PARAM);
  bool const threadsLimitSet = args.touched(THREADS_LIMIT_PARAM);
  bool const commitThreadsSet = args.touched(COMMIT_THREADS_PARAM);
  bool const commitThreadsIdleSet = args.touched(COMMIT_THREADS_IDLE_PARAM);
  bool const consolidationThreadsSet =
      args.touched(CONSOLIDATION_THREADS_PARAM);
  bool const consolidationThreadsIdleSet =
      args.touched(CONSOLIDATION_THREADS_IDLE_PARAM);

  uint32_t threadsLimit = static_cast<uint32_t>(4 * NumberOfCores::getValue());

  if ((threadsLimitSet || threadsSet) && !commitThreadsSet &&
      !consolidationThreadsSet) {
    // backwards compatibility
    threadsLimit = std::min(threadsLimit, _threadsLimit);
    uint32_t const threads = computeThreadsCount(_threads, threadsLimit, 4);
    _commitThreads = std::max(threads / 2, 1U);
    _consolidationThreads = _commitThreads;
  } else {
    _commitThreads = computeThreadsCount(_commitThreads, threadsLimit, 6);
    _consolidationThreads =
        computeThreadsCount(_consolidationThreads, threadsLimit, 6);
  }

  _commitThreadsIdle =
      commitThreadsIdleSet
          ? computeIdleThreadsCount(_commitThreadsIdle, _commitThreads)
          : _commitThreads;

  _consolidationThreadsIdle =
      consolidationThreadsIdleSet
          ? computeIdleThreadsCount(_consolidationThreadsIdle,
                                    _consolidationThreads)
          : _consolidationThreads;

  _running.store(false);
}

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

  registerRecoveryHelper(server());

  // register filters
  if (server().hasFeature<aql::AqlFunctionFeature>()) {
    auto& functions = server().getFeature<aql::AqlFunctionFeature>();
    registerFilters(functions);
    registerScorers(functions);
  } else {
    LOG_TOPIC("462d7", WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'AQLFunctions' while registering "
           "arangosearch filters";
  }

  // ensure no tasks are scheduled and no threads are started
  TRI_ASSERT(std::make_tuple(size_t(0), size_t(0), size_t(0)) ==
             stats(ThreadGroup::_0));
  TRI_ASSERT(std::make_tuple(size_t(0), size_t(0), size_t(0)) ==
             stats(ThreadGroup::_1));

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

    if (!submitTask(ThreadGroup::_0) || !submitTask(ThreadGroup::_1)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_SYS_ERROR,
          "failed to initialize ArangoSearch maintenance threads");
    }

    TRI_ASSERT(std::make_tuple(size_t(0), size_t(1), size_t(0)) ==
               stats(ThreadGroup::_0));
    TRI_ASSERT(std::make_tuple(size_t(0), size_t(1), size_t(0)) ==
               stats(ThreadGroup::_1));
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
    _async->get(ThreadGroup::_1)
        .limits(_consolidationThreads, _consolidationThreadsIdle);

    LOG_TOPIC("c1b63", INFO, arangodb::iresearch::TOPIC)
        << "ArangoSearch maintenance: "
        << "[" << _commitThreadsIdle << ".." << _commitThreads
        << "] commit thread(s), "
        << "[" << _consolidationThreadsIdle << ".." << _consolidationThreads
        << "] consolidation thread(s)";

    {
      auto lock = irs::make_unique_lock(_startState->mtx);
      if (!_startState->cv.wait_for(
              lock, 60s, [this]() { return _startState->counter == 2; })) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_SYS_ERROR,
            "failed to start ArangoSearch maintenance threads");
      }
    }

    // this can destroy the state instance, so we have to ensure that our lock
    // on _startState->mutex is already destroyed here!
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

bool IResearchFeature::queue(ThreadGroup id,
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

  if (!server().isStopping()) {
    // do not log error at shutdown
    LOG_TOPIC("c1b66", ERR, arangodb::iresearch::TOPIC)
        << "Failed to submit a task to thread group '"
        << std::to_string(std::underlying_type_t<ThreadGroup>(id)) << "'";
  }

  return false;
}

std::tuple<size_t, size_t, size_t> IResearchFeature::stats(
    ThreadGroup id) const {
  return _async->get(id).stats();
}

std::pair<size_t, size_t> IResearchFeature::limits(ThreadGroup id) const {
  return _async->get(id).limits();
}

template<typename Engine, typename std::enable_if_t<
                              std::is_base_of_v<StorageEngine, Engine>, int>>
IndexTypeFactory& IResearchFeature::factory() {
  TRI_ASSERT(_factories.find(std::type_index(typeid(Engine))) !=
             _factories.end());
  return *_factories.find(std::type_index(typeid(Engine)))->second;
}
template IndexTypeFactory& IResearchFeature::factory<ClusterEngine>();
template IndexTypeFactory& IResearchFeature::factory<RocksDBEngine>();

}  // namespace iresearch
}  // namespace arangodb
