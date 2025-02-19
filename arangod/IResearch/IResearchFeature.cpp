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
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchFeature.h"

#include "Basics/DownCast.h"
#include "Basics/StaticStrings.h"
#include <utils/source_location.hpp>

#include "search/scorers.hpp"
#include "utils/assert.hpp"
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
#include "Basics/NumberOfCores.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#ifdef USE_ENTERPRISE
#include "Cluster/ClusterMethods.h"
#endif
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "CrashHandler/CrashHandler.h"
#include "Containers/SmallVector.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "IResearch/Containers.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchExecutionPool.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchRocksDBLink.h"
#include "IResearch/IResearchRocksDBRecoveryHelper.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/Search.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "Logger/Topics.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/LogicalView.h"

#include <absl/strings/str_cat.h>

using namespace std::chrono_literals;

namespace arangodb::aql {
class Query;
}  // namespace arangodb::aql

namespace arangodb::iresearch {

namespace {

DECLARE_GAUGE(arangodb_search_num_out_of_sync_links, uint64_t,
              "Number of arangosearch links/indexes currently out of sync");

DECLARE_GAUGE(
    arangodb_search_execution_threads_demand, IResearchExecutionPool,
    "Number of Arangosearch parallel execution threads requested by queries.");

#ifdef USE_ENTERPRISE

DECLARE_GAUGE(arangodb_search_columns_cache_size, LimitedResourceManager,
              "ArangoSearch columns cache usage in bytes");
#endif

// Log topic implementation for IResearch
class IResearchLogTopic final : public LogTopic {
 public:
  IResearchLogTopic() : LogTopic(logger::topic::LibIResearch{}) {
    setIResearchLogLevel(this->level());
  }

  void setLogLevel(LogLevel level) final {
    LogTopic::setLogLevel(level);
    setIResearchLogLevel(level);
  }

 private:
  static constexpr LogLevel kDefaultLevel = LogLevel::INFO;

  static void setIResearchLogLevel(LogLevel level);
};

static IResearchLogTopic LIBIRESEARCH{};

template<LogLevel Level>
static void log(irs::SourceLocation&& source, std::string_view message) {
  Logger::log("9afd3", source.func.data(), source.file.data(),
              static_cast<int>(source.line), Level, LIBIRESEARCH.id(), message);
}

static constexpr std::array<irs::log::Callback, 6> kLogs = {
    &log<LogLevel::FATAL>, &log<LogLevel::ERR>,   &log<LogLevel::WARN>,
    &log<LogLevel::INFO>,  &log<LogLevel::DEBUG>, &log<LogLevel::TRACE>,
};

void IResearchLogTopic::setIResearchLogLevel(LogLevel level) {
  if (level == LogLevel::DEFAULT) {
    level = kDefaultLevel;
  }
  for (size_t i = 0; i != kLogs.size(); ++i) {
    if (i < static_cast<size_t>(level)) {
      irs::log::SetCallback(static_cast<irs::log::Level>(i), kLogs[i]);
    } else {
      irs::log::SetCallback(static_cast<irs::log::Level>(i), nullptr);
    }
  }
}

std::string const THREADS_PARAM("--arangosearch.threads");
std::string const THREADS_LIMIT_PARAM("--arangosearch.threads-limit");
std::string const COMMIT_THREADS_PARAM("--arangosearch.commit-threads");
std::string const COMMIT_THREADS_IDLE_PARAM(
    "--arangosearch.commit-threads-idle");
std::string const CONSOLIDATION_THREADS_PARAM(
    "--arangosearch.consolidation-threads");
std::string const CONSOLIDATION_THREADS_IDLE_PARAM(
    "--arangosearch.consolidation-threads-idle");
std::string const FAIL_ON_OUT_OF_SYNC(
    "--arangosearch.fail-queries-on-out-of-sync");
std::string const SKIP_RECOVERY("--arangosearch.skip-recovery");
std::string const CACHE_LIMIT("--arangosearch.columns-cache-limit");
std::string const CACHE_ONLY_LEADER("--arangosearch.columns-cache-only-leader");
std::string const SEARCH_THREADS_LIMIT(
    "--arangosearch.execution-threads-limit");
std::string const SEARCH_DEFAULT_PARALLELISM(
    "--arangosearch.default-parallelism");

aql::AqlValue dummyFunc(aql::ExpressionContext*, aql::AstNode const& node,
                        std::span<aql::AqlValue const>) {
  THROW_ARANGO_EXCEPTION_FORMAT(
      TRI_ERROR_NOT_IMPLEMENTED,
      "ArangoSearch function '%s' is designed to be used only within a "
      "corresponding SEARCH statement of ArangoSearch view. Please ensure "
      "function signature is correct.",
      aql::functions::getFunctionName(node).data());
}

aql::AqlValue offsetInfoFunc(aql::ExpressionContext* ctx,
                             aql::AstNode const& node,
                             std::span<aql::AqlValue const> args) {
#ifdef USE_ENTERPRISE
  return dummyFunc(ctx, node, args);
#else
  return aql::functions::NotImplementedEE(ctx, node, args);
#endif
}

// Function body for ArangoSearch context functions ANALYZER/BOOST.
// Just returns its first argument as outside ArangoSearch context
// there is nothing to do with search stuff, but optimization could roll.
aql::AqlValue contextFunc(aql::ExpressionContext* ctx, aql::AstNode const&,
                          std::span<aql::AqlValue const> args) {
  TRI_ASSERT(ctx);
  TRI_ASSERT(!args.empty());  // ensured by function signature

  aql::AqlValueMaterializer materializer(&ctx->trx().vpackOptions());
  return aql::AqlValue{materializer.slice(args[0])};
}

// Register invalid argument warning
inline aql::AqlValue errorAqlValue(aql::ExpressionContext* ctx,
                                   char const* afn) {
  aql::functions::registerInvalidArgumentWarning(ctx, afn);
  return aql::AqlValue{aql::AqlValueHintNull{}};
}

// Executes STARTS_WITH function with const parameters locally the same way
// it will be done in ArangoSearch at runtime
// This will allow optimize out STARTS_WITH call if all arguments are const
aql::AqlValue startsWithFunc(aql::ExpressionContext* ctx, aql::AstNode const&,
                             std::span<aql::AqlValue const> args) {
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
                           std::span<aql::AqlValue const> args) {
  static char const* AFN = "MIN_MATCH";

  TRI_ASSERT(args.size() > 1);  // ensured by function signature
  auto& minMatchValue = args.back();
  if (ADB_UNLIKELY(!minMatchValue.isNumber())) {
    return errorAqlValue(ctx, AFN);
  }

  auto matchesLeft = minMatchValue.toInt64();
  auto const argsCount = args.size() - 1;
  for (size_t i = 0; i < argsCount && matchesLeft > 0; ++i) {
    auto& currValue = args[i];
    if (currValue.toBoolean()) {
      matchesLeft--;
    }
  }

  return aql::AqlValue(aql::AqlValueHintBool(matchesLeft == 0));
}

aql::AqlValue dummyScorerFunc(aql::ExpressionContext*, aql::AstNode const& node,
                              std::span<aql::AqlValue const>) {
  THROW_ARANGO_EXCEPTION_FORMAT(
      TRI_ERROR_NOT_IMPLEMENTED,
      "ArangoSearch scorer function '%s' are designed to "
      "be used only outside SEARCH statement within a context of ArangoSearch "
      "view. Please ensure function signature is correct.",
      aql::functions::getFunctionName(node).data());
}

uint32_t computeThreadsCount(uint32_t threads, uint32_t threadsLimit,
                             uint32_t div) noexcept {
  TRI_ASSERT(div);
  // arbitrary limit on the upper bound of threads in pool
  constexpr uint32_t MAX_THREADS = 8;
  constexpr uint32_t MIN_THREADS = 1;  // at least one thread is required

  return std::max(
      MIN_THREADS,
      std::min(threadsLimit ? threadsLimit : MAX_THREADS,
               threads ? threads : uint32_t(NumberOfCores::getValue()) / div));
}

Result upgradeArangoSearchLinkCollectionName(
    TRI_vocbase_t& vocbase, velocypack::Slice /*upgradeParams*/) {
  using application_features::ApplicationServer;
  if (!ServerState::instance()->isDBServer()) {
    return {};  // not applicable for other ServerState roles
  }
  auto& selector = vocbase.server().getFeature<EngineSelectorFeature>();
  auto& clusterInfo =
      vocbase.server().getFeature<ClusterFeature>().clusterInfo();
  // persist collection names in links
  for (auto& collection : vocbase.collections(false)) {
    auto indexes = collection->getPhysical()->getReadyIndexes();
    std::string clusterCollectionName;
    if (!collection->shardIds()->empty()) {
      if (auto maybeShardID = ShardID::shardIdFromString(collection->name());
          maybeShardID.ok()) {
        unsigned tryCount{60};
        // Only try to do this loop on valid shard names. ALl others have no
        // chance to succeed.
        do {
          LOG_TOPIC("423b3", TRACE, arangodb::iresearch::TOPIC)
              << " Checking collection '" << collection->name()
              << "' in database '" << vocbase.name() << "'";
          // we use getCollectionNameForShard as getCollectionNT here is still
          // not available but shard-collection mapping is loaded eventually
          clusterCollectionName =
              clusterInfo.getCollectionNameForShard(maybeShardID.get());
          if (!clusterCollectionName.empty()) {
            break;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } while (--tryCount);
      }
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
        TRI_ASSERT(index != nullptr);
        if (index->type() == Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK) {
#ifdef ARANGODB_USE_GOOGLE_TESTS
          auto* indexPtr = dynamic_cast<IResearchLink*>(index.get());
          TRI_ASSERT(indexPtr != nullptr);
          auto id = indexPtr->index().id().id();
#else
          auto* indexPtr = basics::downCast<IResearchRocksDBLink>(index.get());
          auto const id = indexPtr->id().id();
#endif
          LOG_TOPIC("d6edb", TRACE, arangodb::iresearch::TOPIC)
              << "Checking collection name '" << clusterCollectionName
              << "' for link " << id;
          if (indexPtr->setCollectionName(clusterCollectionName)) {
            LOG_TOPIC("b269d", INFO, arangodb::iresearch::TOPIC)
                << "Setting collection name '" << clusterCollectionName
                << "' for link " << id;
            if (selector.engineName() == RocksDBEngine::kEngineName) {
              auto& engine = selector.engine<RocksDBEngine>();
              auto builder = collection->toVelocyPackIgnore(
                  {"path", "statusString"},
                  LogicalDataSource::Serialization::PersistenceWithInProgress);
              auto res = engine.writeCreateCollectionMarker(
                  vocbase.id(), collection->id(), builder.slice(),
                  RocksDBLogValue::Empty());
              if (res.fail()) {
                LOG_TOPIC("50ace", WARN, arangodb::iresearch::TOPIC)
                    << "Unable to store updated link information on upgrade "
                       "for collection '"
                    << clusterCollectionName << "' for link " << id << ": "
                    << res.errorMessage();
              }
#ifdef ARANGODB_USE_GOOGLE_TESTS
              // for unit tests just ignore write to storage
            } else if (selector.engineName() != "Mock") {
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
    } else {
      LOG_TOPIC("d61d3", WARN, arangodb::iresearch::TOPIC)
          << "Failed to find collection name for shard '" << collection->name()
          << "'!";
    }
  }
  return {};
}

Result upgradeSingleServerArangoSearchView0_1(
    TRI_vocbase_t& vocbase, velocypack::Slice /*upgradeParams*/) {
  using application_features::ApplicationServer;

  if (!ServerState::instance()->isSingleServer() &&
      !ServerState::instance()->isDBServer()) {
    return {};  // not applicable for other ServerState roles
  }

  for (auto& view : vocbase.views()) {
    if (!basics::downCast<IResearchView>(view.get())) {
      continue;  // not an IResearchView
    }

    velocypack::Builder builder;

    builder.openObject();
    // get JSON with meta + 'version'
    Result res = view->properties(
        builder, LogicalDataSource::Serialization::Persistence);
    builder.close();

    if (!res.ok()) {
      LOG_TOPIC("c5dc4", WARN, arangodb::iresearch::TOPIC)
          << "failure to generate persisted definition while upgrading "
             "IResearchView from version 0 to version 1";

      return res;  // definition generation failure
    }

    auto versionSlice =
        builder.slice().get(arangodb::iresearch::StaticStrings::VersionField);

    if (!versionSlice.isNumber<uint32_t>()) {
      auto msg =
          "failure to find 'version' field while upgrading IResearchView "
          "from version 0 to version 1";
      LOG_TOPIC("eae1c", WARN, arangodb::iresearch::TOPIC) << msg;

      return {TRI_ERROR_INTERNAL, std::move(msg)};  // required field is missing
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

      return res;  // definition generation failure
    }

    auto& server = vocbase.server();
    if (!server.hasFeature<DatabasePathFeature>()) {
      auto msg =
          "failure to find feature 'DatabasePath' while upgrading "
          "IResearchView from version 0 to version 1";
      LOG_TOPIC("67c7e", WARN, arangodb::iresearch::TOPIC) << msg;
      return {TRI_ERROR_INTERNAL,
              std::move(msg)};  // required feature is missing
    }
    auto& dbPathFeature = server.getFeature<DatabasePathFeature>();

    // original algorithm for computing data-store path
    std::filesystem::path dataPath{dbPathFeature.directory()};
    dataPath /= "databases";
    dataPath /= absl::StrCat("database-", vocbase.id());
    dataPath /=
        absl::StrCat(StaticStrings::ViewArangoSearchType, "-", view->id().id());

    res = view->drop();  // drop view (including all links)

    if (!res.ok()) {
      LOG_TOPIC("cb9d1", WARN, arangodb::iresearch::TOPIC)
          << "failure to drop view while upgrading IResearchView from "
             "version "
             "0 to version 1";

      return res;  // view drom failure
    }

    // .........................................................................
    // non-recoverable state below here
    // .........................................................................

    // non-version 0 IResearchView implementations no longer drop from vocbase
    // on db-server, do it explicitly
    if (ServerState::instance()->isDBServer()) {
      res = storage_helper::drop(*view);

      if (!res.ok()) {
        LOG_TOPIC("bfb3d", WARN, arangodb::iresearch::TOPIC)
            << "failure to drop view from vocbase while upgrading "
               "IResearchView from version 0 to version 1";

        return res;  // view drom failure
      }
    }

    if (ServerState::instance()->isSingleServer() ||
        ServerState::instance()->isDBServer()) {
      bool exists;

      // remove any stale data-store
      if (!irs::file_utils::exists_directory(exists, dataPath.c_str()) ||
          (exists && !irs::file_utils::remove(dataPath.c_str()))) {
        auto msg = absl::StrCat(
            "failure to remove old data-store path while upgrading "
            "IResearchView from version 0 to version 1, view definition: ",
            builder.slice().toString());
        LOG_TOPIC("9ab42", WARN, arangodb::iresearch::TOPIC) << msg;
        return {TRI_ERROR_INTERNAL,
                std::move(msg)};  // data-store removal failure
      }
    }

    if (ServerState::instance()->isDBServer()) {
      continue;  // no need to recreate per-cid view
    }

    // recreate view
    res = arangodb::iresearch::IResearchView::factory().create(
        view, vocbase, builder.slice(), false);

    if (!res.ok()) {
      LOG_TOPIC("f8d20", WARN, arangodb::iresearch::TOPIC)
          << "failure to recreate view while upgrading IResearchView from "
             "version 0 to version 1, error: "
          << res.errorNumber() << " " << res.errorMessage()
          << ", view definition: " << builder.slice().toString();

      return res;  // data-store removal failure
    }
  }

  return {};
}

void registerFilters(aql::AqlFunctionFeature& functions) {
  using arangodb::iresearch::addFunction;

  constexpr auto flags = aql::Function::makeFlags(
      aql::Function::Flags::Deterministic, aql::Function::Flags::Cacheable,
      aql::Function::Flags::CanRunOnDBServerCluster,
      aql::Function::Flags::CanRunOnDBServerOneShard,
      aql::Function::Flags::CanUseInAnalyzer);

  constexpr auto flagsNoAnalyzer = aql::Function::makeFlags(
      aql::Function::Flags::Deterministic, aql::Function::Flags::Cacheable,
      aql::Function::Flags::CanRunOnDBServerCluster,
      aql::Function::Flags::CanRunOnDBServerOneShard);

  // (attribute, ["analyzer"|"type"|"string"|"numeric"|"bool"|"null"]).
  // cannot be used in analyzers!
  addFunction(functions, {"EXISTS", ".|.,.", flagsNoAnalyzer, &dummyFunc});

  // (attribute, [ '[' ] prefix [, prefix, ... ']' ] [,
  // scoring-limit|min-match-count ] [, scoring-limit ])
  addFunction(functions, {"STARTS_WITH", ".,.|.,.", flags, &startsWithFunc});

  // (attribute, input [, offset, input... ] [, analyzer])
  // cannot be used in analyzers!
  addFunction(functions, {"PHRASE", ".,.|.+", flagsNoAnalyzer, &dummyFunc});

  // (filter expression [, filter expression, ... ], min match count)
  addFunction(functions, {"MIN_MATCH", ".,.|.+", flags, &minMatchFunc});

  // (filter expression, boost)
  addFunction(functions, {"BOOST", ".,.", flags, &contextFunc});

  // (filter expression, analyzer)
  // cannot be used in analyzers!
  addFunction(functions, {"ANALYZER", ".,.", flagsNoAnalyzer, &contextFunc});
}

template<typename T>
void registerSingleFactory(IndexTypeFactory& factory, ArangodServer& server) {
  if (!server.hasFeature<T>()) {
    return;
  }
  auto& engine = server.getFeature<T>();
  auto& engineFactory = const_cast<IndexFactory&>(engine.indexFactory());
  // TODO(MBkkt) remove std::string and update IndexFactory interface
  auto r = engineFactory.emplace(
      std::string{StaticStrings::ViewArangoSearchType}, factory);
  if (!r.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        r.errorNumber(),
        absl::StrCat("failure registering IResearch link factory with index "
                     "factory from feature '",
                     engine.name(), "': ", r.errorMessage()));
  }
}

void registerFunctions(aql::AqlFunctionFeature& functions) {
  arangodb::iresearch::addFunction(
      functions,
      {"OFFSET_INFO", ".,.",
       aql::Function::makeFlags(aql::Function::Flags::Deterministic,
                                aql::Function::Flags::Cacheable,
                                aql::Function::Flags::CanRunOnDBServerCluster,
                                aql::Function::Flags::CanRunOnDBServerOneShard,
                                aql::Function::Flags::NoEval),
       &offsetInfoFunc});
}

void registerScorers(aql::AqlFunctionFeature& functions) {
  // positional arguments (attribute [<scorer-specific properties>...]);
  std::string_view constexpr args(".|+");

  irs::scorers::visit(
      [&functions, &args](std::string_view name,
                          irs::type_info const& args_format) -> bool {
        // ArangoDB, for API consistency, only supports scorers configurable
        // via jSON
        if (irs::type<irs::text_format::json>::id() != args_format.id()) {
          return true;
        }

        auto upperName = static_cast<std::string>(name);

        // AQL function external names are always in upper case
        std::transform(upperName.begin(), upperName.end(), upperName.begin(),
                       ::toupper);

        // scorers are not usable in analyzers
        arangodb::iresearch::addFunction(
            functions, {std::move(upperName), args.data(),
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

void registerUpgradeTasks(ArangodServer& server) {
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

void registerViewFactory(ArangodServer& server) {
  Result r;
  auto check = [&] {
    if (!r.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          r.errorNumber(),
          absl::StrCat("failure registering arangosearch view factory: ",
                       r.errorMessage()));
    }
  };
  // DB server in custer or single-server
  auto& viewTypes = server.getFeature<ViewTypesFeature>();
  if (ServerState::instance()->isCoordinator()) {
    r = viewTypes.emplace(
        arangodb::iresearch::StaticStrings::ViewArangoSearchType,
        IResearchViewCoordinator::factory());
    check();
    r = viewTypes.emplace(
        arangodb::iresearch::StaticStrings::ViewSearchAliasType,
        Search::factory());
  } else if (ServerState::instance()->isSingleServer()) {
    r = viewTypes.emplace(
        arangodb::iresearch::StaticStrings::ViewArangoSearchType,
        IResearchView::factory());
    check();
    r = viewTypes.emplace(
        arangodb::iresearch::StaticStrings::ViewSearchAliasType,
        Search::factory());
  } else if (ServerState::instance()->isDBServer()) {
    r = viewTypes.emplace(
        arangodb::iresearch::StaticStrings::ViewArangoSearchType,
        IResearchView::factory());
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                   "Invalid role for view creation.");
  }
  check();
}

Result transactionDataSourceRegistrationCallback(LogicalDataSource& dataSource,
                                                 transaction::Methods& trx) {
  if (LogicalView::category() != dataSource.category()) {
    return {};  // not a view
  }
  // TODO FIXME find a better way to look up a LogicalView
  auto* view = basics::downCast<LogicalView>(&dataSource);
  if (!view) {
    LOG_TOPIC("f42f8", WARN, arangodb::iresearch::TOPIC)
        << "failure to get LogicalView while processing a TransactionState "
           "by "
           "IResearchFeature for name '"
        << dataSource.name() << "'";

    return {TRI_ERROR_INTERNAL};
  }

  if (view->type() == ViewType::kSearchAlias) {
    auto& impl = basics::downCast<Search>(*view);
    return {impl.apply(trx) ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL};
  } else if (view->type() == ViewType::kArangoSearch) {
    auto& impl = basics::downCast<IResearchView>(*view);
    return {impl.apply(trx) ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL};
  }
  return {};  // not a needed view
}

void registerTransactionDataSourceRegistrationCallback() {
  if (ServerState::instance()->isSingleServer()) {
    transaction::Methods::addDataSourceRegistrationCallback(
        &transactionDataSourceRegistrationCallback);
  }
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

class AssertionCallbackSetter {
 public:
  AssertionCallbackSetter() noexcept {
    irs::assert::SetCallback(&assertCallback);
  }

 private:
  [[noreturn]] static void assertCallback(irs::SourceLocation&& source,
                                          std::string_view message) noexcept {
    CrashHandler::assertionFailure(source.file.data(),
                                   static_cast<int>(source.line),
                                   source.func.data(), message.data(), "");
  }
};

[[maybe_unused]] AssertionCallbackSetter setAssert;

#endif

}  // namespace

class IResearchAsync {
 public:
  using ThreadPool = irs::async_utils::ThreadPool<>;

  ~IResearchAsync() { stop(); }

  ThreadPool& get(ThreadGroup id) noexcept {
    return ThreadGroup::_0 == id ? _0 : _1;
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
  ThreadPool _0;
  ThreadPool _1;
};

bool isFilter(aql::Function const& func) noexcept {
  return func.implementation == &dummyFunc ||
         func.implementation == &contextFunc ||
         func.implementation == &minMatchFunc ||
         func.implementation == &startsWithFunc ||
         func.implementation == &aql::functions::MinHashMatch ||
         func.implementation == &aql::functions::GeoContains ||
         func.implementation == &aql::functions::GeoInRange ||
         func.implementation == &aql::functions::GeoIntersects ||
         func.implementation == &aql::functions::GeoDistance ||
         func.implementation == &aql::functions::LevenshteinMatch ||
         func.implementation == &aql::functions::Like ||
         func.implementation == &aql::functions::NgramMatch ||
         func.implementation == &aql::functions::InRange;
}

bool isScorer(aql::Function const& func) noexcept {
  // cppcheck-suppress throwInNoexceptFunction
  return func.implementation == &dummyScorerFunc;
}

bool isOffsetInfo(aql::Function const& func) noexcept {
  // cppcheck-suppress throwInNoexceptFunction
  return func.implementation == &offsetInfoFunc;
}

IResearchFeature::IResearchFeature(Server& server)
    : ArangodFeature{server, *this},
      _async(std::make_unique<IResearchAsync>()),
      _outOfSyncLinks(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_search_num_out_of_sync_links{})),
#ifdef USE_ENTERPRISE
      _columnsCacheMemoryUsed(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_search_columns_cache_size{})),
#endif
      _searchExecutionPool(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_search_execution_threads_demand{})) {
  setOptional(true);
#ifdef USE_V8
  startsAfter<application_features::V8FeaturePhase>();
#else
  startsAfter<application_features::ClusterFeaturePhase>();
#endif
  startsAfter<IResearchAnalyzerFeature>();
  startsAfter<aql::AqlFunctionFeature>();
}

void IResearchFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("arangosearch", absl::StrCat(name(), " feature"));

  options
      ->addOption(THREADS_PARAM,
                  "The exact number of threads to use for asynchronous "
                  "tasks (0 = auto-detect).",
                  new options::UInt32Parameter(&_threads))
      .setDeprecatedIn(30705)
      .setLongDescription(R"(From version 3.7.5 on, you should set the commit
and consolidation thread counts separately via the following options instead:

- `--arangosearch.commit-threads`
- `--arangosearch.consolidation-threads`

If either `--arangosearch.commit-threads` or
`--arangosearch.consolidation-threads` is set, then `--arangosearch.threads` and
`arangosearch.threads-limit` are ignored. If only the legacy options are set,
then the commit and consolidation thread counts are calculated as follows:

- Maximum: The smaller value out of `--arangosearch.threads` and
  `arangosearch.threads-limit` divided by 2, but at least 1.
- Minimum: the maximum divided by 2, but at least 1.)");

  options
      ->addOption(
          THREADS_LIMIT_PARAM,
          "The upper limit to the auto-detected number of threads to use "
          "for asynchronous tasks (0 = use default).",
          new options::UInt32Parameter(&_threadsLimit))
      .setDeprecatedIn(30705)
      .setLongDescription(R"(From version 3.7.5 on, you should set the commit
and consolidation thread counts separately via the following options instead:

- `--arangosearch.commit-threads`
- `--arangosearch.consolidation-threads`

If either `--arangosearch.commit-threads` or
`--arangosearch.consolidation-threads` is set, then `--arangosearch.threads` and
`arangosearch.threads-limit` are ignored. If only the legacy options are set,
then the commit and consolidation thread counts are calculated as follows:

- Maximum: The smaller value out of `--arangosearch.threads` and
  `arangosearch.threads-limit` divided by 2, but at least 1.
- Minimum: the maximum divided by 2, but at least 1.)");

  options
      ->addOption(
          CONSOLIDATION_THREADS_PARAM,
          "The upper limit to the allowed number of consolidation threads "
          "(0 = auto-detect).",
          new options::UInt32Parameter(&_consolidationThreads))
      .setLongDescription(R"(The option value must fall in the range
`[ 1..arangosearch.consolidation-threads ]`. Set it to `0` to automatically
choose a sensible number based on the number of cores in the system.)");

  options
      ->addOption(
          CONSOLIDATION_THREADS_IDLE_PARAM,
          "The upper limit to the allowed number of idle threads to use "
          "for consolidation tasks (0 = auto-detect).",
          new options::UInt32Parameter(&_deprecatedOptions))
      .setDeprecatedIn(3'11'06)
      .setDeprecatedIn(3'12'00);

  options
      ->addOption(COMMIT_THREADS_PARAM,
                  "The upper limit to the allowed number of commit threads "
                  "(0 = auto-detect).",
                  new options::UInt32Parameter(&_commitThreads))
      .setLongDescription(R"(The option value must fall in the range
`[ 1..4 * NumberOfCores ]`. Set it to `0` to automatically choose a sensible
number based on the number of cores in the system.)");

  options
      ->addOption(
          COMMIT_THREADS_IDLE_PARAM,
          "The upper limit to the allowed number of idle threads to use "
          "for commit tasks (0 = auto-detect)",
          new options::UInt32Parameter(&_deprecatedOptions))
      .setLongDescription(R"(The option value must fall in the range
`[ 1..arangosearch.commit-threads ]`. Set it to `0` to automatically choose a
sensible number based on the number of cores in the system.)")
      .setDeprecatedIn(3'11'06)
      .setDeprecatedIn(3'12'00);

  options
      ->addOption(
          SKIP_RECOVERY,  // TODO: Move parts of the descriptions to
                          // longDescription?
          "Skip the data recovery for the specified View link or inverted "
          "index on startup. The value for this option needs to have the "
          "format '<collection-name>/<index-id>' or "
          "'<collection-name>/<index-name>'. You can use the option multiple "
          "times, for each View link and inverted index to skip the recovery "
          "for. The pseudo-value 'all' disables the recovery for all View "
          "links and inverted indexes. The links/indexes skipped during the "
          "recovery are marked as out-of-sync when the recovery completes. You "
          "need to recreate them manually afterwards.\n"
          "WARNING: Using this option causes data of affected links/indexes to "
          "become incomplete or more incomplete until they have been manually "
          "recreated.",
          new options::VectorParameter<options::StringParameter>(
              &_skipRecoveryItems))
      .setIntroducedIn(30904);

  options
      ->addOption(FAIL_ON_OUT_OF_SYNC,
                  "Whether retrieval queries on out-of-sync "
                  "View links and inverted indexes should fail.",
                  new options::BooleanParameter(&_failQueriesOnOutOfSync))
      .setIntroducedIn(30904)
      .setLongDescription(R"(If set to `true`, any data retrieval queries on
out-of-sync links/indexes fail with the error 'collection/view is out of sync'
(error code 1481).

If set to `false`, queries on out-of-sync links/indexes are answered normally,
but the returned data may be incomplete.)");

#ifdef USE_ENTERPRISE
  auto& manager =
      basics::downCast<LimitedResourceManager>(_columnsCacheMemoryUsed);
  options
      ->addOption(CACHE_LIMIT,
                  "The limit (in bytes) for ArangoSearch columns cache "
                  "(0 = no caching).",
                  new options::UInt64Parameter(&manager.limit),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnSingle,
                                            options::Flags::OnDBServer,
                                            options::Flags::Enterprise))
      .setIntroducedIn(3'09'05);
  options
      ->addOption(CACHE_ONLY_LEADER,
                  "Cache ArangoSearch columns only for leader shards.",
                  new options::BooleanParameter(&_columnsCacheOnlyLeader),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnDBServer,
                                            options::Flags::Enterprise))
      .setIntroducedIn(3'10'06);
#endif
  options
      ->addOption(SEARCH_THREADS_LIMIT,
                  "The maximum number of threads that can be used to process "
                  "ArangoSearch indexes during a SEARCH operation of a query.",
                  new options::UInt32Parameter(&_searchExecutionThreadsLimit),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnDBServer,
                                            options::Flags::OnSingle))
      .setIntroducedIn(3'11'06)
      .setIntroducedIn(3'12'00);
  options
      ->addOption(SEARCH_DEFAULT_PARALLELISM,
                  "Default parallelism for ArangoSearch queries",
                  new options::UInt32Parameter(&_defaultParallelism),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnDBServer,
                                            options::Flags::OnSingle))
      .setIntroducedIn(3'11'06)
      .setIntroducedIn(3'12'00);
}

void IResearchFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  // validate all entries in _skipRecoveryItems for formal correctness
  auto checkFormat = [](auto const& item) {
    auto r = item.find('/');
    if (r == std::string_view::npos) {
      return false;
    }
    r = item.find('/', r);
    if (r == std::string_view::npos) {
      return true;
    }
    return false;
  };
  for (auto const& item : _skipRecoveryItems) {
    if (item != "all" && checkFormat(item)) {
      LOG_TOPIC("b9f28", FATAL, arangodb::iresearch::TOPIC)
          << "invalid format for '" << SKIP_RECOVERY
          << "' parameter. expecting '"
          << "<collection-name>/<index-id>' or "
             "'<collection-name>/<index-name>' or "
          << "'all', got: '" << item << "'";
      FATAL_ERROR_EXIT();
    }
  }

  auto const& args = options->processingResult();
  bool const threadsSet = args.touched(THREADS_PARAM);
  bool const threadsLimitSet = args.touched(THREADS_LIMIT_PARAM);
  bool const commitThreadsSet = args.touched(COMMIT_THREADS_PARAM);
  bool const consolidationThreadsSet =
      args.touched(CONSOLIDATION_THREADS_PARAM);

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

  if (!args.touched(SEARCH_THREADS_LIMIT)) {
    _searchExecutionThreadsLimit =
        static_cast<uint32_t>(2 * NumberOfCores::getValue());
  }
}

void IResearchFeature::prepare() {
  TRI_ASSERT(isEnabled());

  // load all known codecs
  ::irs::formats::init();

  // load all known scorers
  ::irs::scorers::init();

  // register 'arangosearch' index
  registerIndexFactory();

  // register 'arangosearch' view
  registerViewFactory(server());

  // register 'arangosearch' Transaction DataSource registration callback
  registerTransactionDataSourceRegistrationCallback();

  registerRecoveryHelper();

  // register filters
  if (server().hasFeature<aql::AqlFunctionFeature>()) {
    auto& functions = server().getFeature<aql::AqlFunctionFeature>();
    registerFilters(functions);
    registerScorers(functions);
    registerFunctions(functions);
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
}

void IResearchFeature::start() {
  TRI_ASSERT(isEnabled());

  // register tasks after UpgradeFeature::prepare() has finished
  registerUpgradeTasks(server());

  // ensure that at least 1 worker for each group is started
  if (ServerState::instance()->isDBServer() ||
      ServerState::instance()->isSingleServer()) {
    TRI_ASSERT(_commitThreads);
    TRI_ASSERT(_consolidationThreads);

    _async->get(ThreadGroup::_0)
        .start(_commitThreads, IR_NATIVE_STRING("ARS-0"));
    _async->get(ThreadGroup::_1)
        .start(_consolidationThreads, IR_NATIVE_STRING("ARS-1"));
    _searchExecutionPool.setLimit(_searchExecutionThreadsLimit);

    LOG_TOPIC("c1b63", INFO, TOPIC)
        << "ArangoSearch maintenance: [" << _commitThreads << ".."
        << _commitThreads << "] commit thread(s), [" << _consolidationThreads
        << ".." << _consolidationThreads
        << "] consolidation thread(s). ArangoSearch execution parallel threads "
           "limit: "
        << _searchExecutionThreadsLimit;

#ifdef USE_ENTERPRISE
    auto& manager =
        basics::downCast<LimitedResourceManager>(_columnsCacheMemoryUsed);
    LOG_TOPIC("c2c74", INFO, TOPIC)
        << "ArangoSearch columns cache limit: " << manager.limit;
#endif
  }
}

void IResearchFeature::stop() {
  TRI_ASSERT(isEnabled());
  _async->stop();
  _searchExecutionPool.stop();
}

void IResearchFeature::unprepare() { TRI_ASSERT(isEnabled()); }

std::filesystem::path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                       TRI_vocbase_t& database) {
  std::filesystem::path path{dbPathFeature.directory()};
  path /= "databases";
  path /= absl::StrCat("database-", database.id());
  return path;
}

void cleanupDatabase(TRI_vocbase_t& database) {
  auto const& feature = database.server().getFeature<DatabasePathFeature>();
  auto path = getPersistedPath(feature, database);
  std::error_code error;
  std::filesystem::remove_all(path);
  if (error) {
    LOG_TOPIC("bad02", ERR, TOPIC)
        << "Failed to remove arangosearch path for database (id '"
        << database.id() << "' name: '" << database.name() << "') with error '"
        << error.message() << "'";
  }
}

bool IResearchFeature::queue(ThreadGroup id,
                             std::chrono::steady_clock::duration delay,
                             fu2::unique_function<void()>&& fn) {
  try {
    if (_async->get(id).run(std::move(fn), delay)) {
      return true;
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("c1b64", WARN, arangodb::iresearch::TOPIC)
        << "Caught exception while sumbitting a task to thread group '"
        << std::underlying_type_t<ThreadGroup>(id) << "' error '" << e.what()
        << "'";
  } catch (...) {
    LOG_TOPIC("c1b65", WARN, arangodb::iresearch::TOPIC)
        << "Caught an exception while sumbitting a task to thread group '"
        << std::underlying_type_t<ThreadGroup>(id) << "'";
  }

  if (!server().isStopping()) {
    // do not log error at shutdown
    LOG_TOPIC("c1b66", ERR, arangodb::iresearch::TOPIC)
        << "Failed to submit a task to thread group '"
        << std::underlying_type_t<ThreadGroup>(id) << "'";
  }

  return false;
}

std::tuple<size_t, size_t, size_t> IResearchFeature::stats(
    ThreadGroup id) const {
  return _async->get(id).stats();
}

std::pair<size_t, size_t> IResearchFeature::limits(ThreadGroup id) const {
  auto threads = _async->get(id).threads();
  return {threads, threads};
}

void IResearchFeature::trackOutOfSyncLink() noexcept { ++_outOfSyncLinks; }

void IResearchFeature::untrackOutOfSyncLink() noexcept {
  uint64_t previous = _outOfSyncLinks.fetch_sub(1);
  TRI_ASSERT(previous > 0);
}

bool IResearchFeature::failQueriesOnOutOfSync() const noexcept {
  TRI_IF_FAILURE("ArangoSearch::FailQueriesOnOutOfSync") {
    // here to test --arangosearch.fail-queries-on-out-of-sync
    return true;
  }
  return _failQueriesOnOutOfSync;
}

void IResearchFeature::registerRecoveryHelper() {
  if (!_skipRecoveryItems.empty()) {
    LOG_TOPIC("e36f2", WARN, arangodb::iresearch::TOPIC)
        << "arangosearch recovery explicitly disabled via the '"
        << SKIP_RECOVERY << "' startup option for the following links/indexes: "
        << _skipRecoveryItems
        << ". all affected links/indexes that are touched during "
           "recovery will be marked as out of sync and should be recreated "
           "manually when the recovery is finished.";
  }

  _recoveryHelper = std::make_shared<IResearchRocksDBRecoveryHelper>(
      server(), _skipRecoveryItems);
  auto res = RocksDBEngine::registerRecoveryHelper(_recoveryHelper);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        absl::StrCat("failed to register RocksDB recovery helper: ",
                     res.errorMessage()));
  }
}

void IResearchFeature::registerIndexFactory() {
  _clusterFactory = IResearchLinkCoordinator::createFactory(server());
  registerSingleFactory<ClusterEngine>(*_clusterFactory, server());
  _rocksDBFactory = IResearchRocksDBLink::createFactory(server());
  registerSingleFactory<RocksDBEngine>(*_rocksDBFactory, server());
}

#ifdef USE_ENTERPRISE
#ifdef ARANGODB_USE_GOOGLE_TESTS
int64_t IResearchFeature::columnsCacheUsage() const noexcept {
  auto& manager =
      basics::downCast<LimitedResourceManager>(_columnsCacheMemoryUsed);
  return manager.load();
}
void IResearchFeature::setCacheUsageLimit(uint64_t limit) noexcept {
  auto& manager =
      basics::downCast<LimitedResourceManager>(_columnsCacheMemoryUsed);
  manager.limit = limit;
}
#endif

bool IResearchFeature::columnsCacheOnlyLeaders() const noexcept {
  TRI_ASSERT(ServerState::instance()->isDBServer() || !_columnsCacheOnlyLeader);
  return _columnsCacheOnlyLeader;
}
#endif

template<typename Engine>
IndexTypeFactory& IResearchFeature::factory() {
  if constexpr (std::is_same_v<Engine, ClusterEngine>) {
    return *_clusterFactory;
  } else {
    static_assert(std::is_same_v<Engine, RocksDBEngine>);
    return *_rocksDBFactory;
  }
}
template IndexTypeFactory& IResearchFeature::factory<ClusterEngine>();
template IndexTypeFactory& IResearchFeature::factory<RocksDBEngine>();

}  // namespace arangodb::iresearch
