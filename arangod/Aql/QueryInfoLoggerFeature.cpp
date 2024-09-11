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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "QueryInfoLoggerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Aql/Query.h"
#include "Aql/QueryOptions.h"
#include "Aql/QueryString.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/conversions.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

using namespace arangodb;

namespace arangodb::aql {

class QueryInfoLoggerThread final : public ServerThread<ArangodServer> {
 public:
  explicit QueryInfoLoggerThread(ArangodServer& server,
                                 size_t maxBufferedQueries,
                                 uint64_t pushInterval,
                                 uint64_t cleanupInterval, double retentionTime)
      : ServerThread<ArangodServer>(server, "QueryInfoLogger"),
        _maxBufferedQueries(maxBufferedQueries),
        _pushInterval(pushInterval),
        _cleanupInterval(cleanupInterval),
        _retentionTime(retentionTime) {}

  ~QueryInfoLoggerThread() { shutdown(); }

  void beginShutdown() override {
    Thread::beginShutdown();

    {
      Scheduler::WorkHandle workItem;
      {
        std::lock_guard<std::mutex> guard(_workItemMutex);
        workItem = std::move(_workItem);
        TRI_ASSERT(_workItem == nullptr);
      }
      // we have to move the shared_ptr under the lock, but we must call cancel
      // _outside_ the lock, because cancel executes the callback, which will
      // itself acquire the lock.
      if (workItem) {
        workItem->cancel();
      }
    }

    {
      std::lock_guard guard{_mutex};
      _condition.notify_one();
    }
  }

  void log(std::shared_ptr<velocypack::String> query) {
    bool doNotify = false;
    {
      std::lock_guard guard{_mutex};

      if (_queries.size() < _maxBufferedQueries) {
        _queries.emplace_back(std::move(query));
        if (_queries.size() >= _maxBufferedQueries / 4) {
          // only notify in case we have added the query,
          // and we have accumulated enough data
          doNotify = true;
        }
      }
    }

    if (doNotify) {
      _condition.notify_one();
    }
  }

 protected:
  void run() override {
    enum class PrepareState {
      // must check if _queries system collection exists
      kCheckCollection,
      // _queries collection is present - must check if persistent index
      // on _queries collection exists
      kCheckIndex,
      // _queries collection and index are present - no checks necessary
      kCheckNothing,
    };

    std::chrono::time_point<std::chrono::steady_clock> lastCleanup{};
    PrepareState prepareState = PrepareState::kCheckCollection;

    // interval in which garbage collection for "old" query entries
    // runs.
    auto gcCheckInterval = std::chrono::milliseconds(_cleanupInterval);
    auto pushInterval = std::chrono::milliseconds(_pushInterval);

    while (true) {
      decltype(_queries) queries;

      try {
        // schedule a cleanup task
        if (auto now = std::chrono::steady_clock::now();
            now > lastCleanup + gcCheckInterval) {
          std::unique_lock<std::mutex> guard(_workItemMutex);
          if (!_workItem) {
            lastCleanup = now;

            queueGarbageCollection(guard);
          }
        }

        std::unique_lock guard{_mutex};

        if (_queries.empty()) {
          if (isStopping()) {
            // exit thread loop
            break;
          }

          // only sleep if we have no queries left to process
          _condition.wait_for(guard, pushInterval);
        } else {
          std::swap(queries, _queries);

          guard.unlock();

          if (!queries.empty()) {
            switch (prepareState) {
              case PrepareState::kCheckCollection:
                if (Result res = ensureCollection(); res.fail()) {
                  // cannot create collection
                  break;
                }
                prepareState = PrepareState::kCheckIndex;
                [[fallthrough]];
              case PrepareState::kCheckIndex:
                if (Result res = ensureIndex(); res.fail()) {
                  // cannot create index
                  LOG_TOPIC("09631", WARN, Logger::QUERIES)
                      << "unable to create index in _queries system "
                         "collection: "
                      << res.errorMessage();
                  break;
                }
                prepareState = PrepareState::kCheckNothing;
                [[fallthrough]];
              case PrepareState::kCheckNothing:
                if (Result res = insertQueries(queries); res.ok()) {
                  queries.clear();
                } else {
                  LOG_TOPIC("fc9b6", WARN, Logger::QUERIES)
                      << "unable to log queries in _queries system collection: "
                      << res.errorMessage();
                }
            }
          }
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("f7b98", ERR, Logger::QUERIES)
            << "caught exception in QueryInfoLoggerThread: " << ex.what();
      }
    }

    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }

 private:
  // queue garbage collection for "old" queries in _queries collection.
  // must be called with the _workItemMutex held
  void queueGarbageCollection(std::unique_lock<std::mutex>& guard) {
    TRI_ASSERT(guard.owns_lock());

    Scheduler* scheduler = SchedulerFeature::SCHEDULER;

    if (scheduler == nullptr) {
      return;
    }

    if (isStopping()) {
      return;
    }

    auto offset = std::chrono::seconds(1);
    if (ServerState::instance()->isCoordinator()) {
      // there may be multiple coordinators all trying to issue the
      // garbage collection query at around the very same time. use a
      // random offset here to make this less likely.
      offset = std::chrono::seconds(RandomGenerator::interval(uint32_t(5)));
    }

    auto workItem = scheduler->queueDelayed(
        "queries-gc", RequestLane::CLIENT_SLOW, offset, [this](bool canceled) {
          if (!canceled && !isStopping()) {
            if (Result res = basics::catchToResult(
                    [this]() { return cleanupCollection(); });
                res.fail() && res.isNot(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) &&
                res.isNot(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) &&
                res.isNot(TRI_ERROR_ARANGO_CONFLICT) &&
                res.isNot(TRI_ERROR_ARANGO_READ_ONLY)) {
              LOG_TOPIC("7a99c", WARN, Logger::QUERIES)
                  << "unable to perform garbage collection "
                     "of logged queries: "
                  << res.errorMessage();
            }
          }

          std::lock_guard<std::mutex> guard(_workItemMutex);
          _workItem.reset();
        });

    _workItem = std::move(workItem);
  }

  // return pointer to _system database, if it exists.
  SystemDatabaseFeature::ptr getSystemDatabase() {
    return server().hasFeature<SystemDatabaseFeature>()
               ? server().getFeature<SystemDatabaseFeature>().use()
               : nullptr;
  }

  // run a cleanup query on the _queries collection, in order to purge
  // old entries.
  Result cleanupCollection() {
    // give ourselves full permissions to access the collection
    ExecContextSuperuserScope scope;

    auto vocbase = getSystemDatabase();

    if (vocbase == nullptr) {
      return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
              "unable to find _system database"};
    }

    // note: we are using ignoreErrors: true here so that concurrent updates/
    // removals of the same documents does not cause the query to fail.
    constexpr std::string_view removeQuery =
        "/*queries cleanup*/ FOR doc IN @@collection FILTER doc.started < "
        "@minDate REMOVE doc IN @@collection OPTIONS {ignoreErrors: true}";

    std::string minDate = TRI_StringTimeStamp(TRI_microtime() - _retentionTime,
                                              Logger::getUseLocalTime());

    auto bindVars = std::make_shared<VPackBuilder>();
    bindVars->openObject();
    bindVars->add("@collection", VPackValue(StaticStrings::QueriesCollection));
    bindVars->add("minDate", VPackValue(minDate));
    bindVars->close();

    LOG_TOPIC("87971", TRACE, Logger::QUERIES)
        << "running queries collection cleanup query with bind parameters "
        << bindVars->slice().toJson();

    aql::QueryOptions options;
    // don't let query runtime restrictions affect the removal query
    options.maxRuntime = 0.0;
    // no need to make the remove query appear in the audit log
    options.skipAudit = true;

    auto origin = transaction::OperationOriginInternal{"queries cleanup"};
    auto query = aql::Query::create(
        transaction::StandaloneContext::create(*vocbase, origin),
        aql::QueryString(removeQuery), std::move(bindVars), options);

    aql::QueryResult queryResult = query->executeSync();

    LOG_TOPIC_IF("05da1", TRACE, Logger::QUERIES,
                 queryResult.fail() &&
                     queryResult.isNot(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND))
        << "queries collection cleanup query failed with: "
        << queryResult.result.errorMessage();

    return queryResult.result;
  }

  // ensure that the _queries collection is created inside the _system
  // database.
  Result ensureCollection() {
    // give ourselves full permissions to create the collection
    ExecContextSuperuserScope scope;

    auto vocbase = getSystemDatabase();

    if (vocbase == nullptr) {
      return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
              "unable to find _system database"};
    }

    std::shared_ptr<LogicalCollection> collection;
    if (Result res = methods::Collections::lookup(
            *vocbase, StaticStrings::QueriesCollection, collection);
        res.fail()) {
      OperationOptions options{};
      res = methods::Collections::createSystem(
          *vocbase, options, StaticStrings::QueriesCollection,
          /*isNewDatabase*/ false, collection);

      if (res.fail() && res.isNot(TRI_ERROR_ARANGO_DUPLICATE_NAME)) {
        return {res.errorNumber(),
                absl::StrCat("unable to create _queries system collection: ",
                             res.errorMessage())};
      }
    }
    // success
    return {};
  }

  // create an index on the _queries collection, so that we can
  // easily find expired entries later
  Result ensureIndex() {
    // give ourselves full permissions to create the index
    ExecContextSuperuserScope scope;

    auto vocbase = getSystemDatabase();

    if (vocbase == nullptr) {
      return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
              "unable to find _system database"};
    }

    std::shared_ptr<LogicalCollection> collection;

    if (Result res = methods::Collections::lookup(
            *vocbase, StaticStrings::QueriesCollection, collection);
        res.fail()) {
      // collection not found
      return {res.errorNumber(),
              absl::StrCat("unable to find _queries system collection: ",
                           res.errorMessage())};
    }

    TRI_ASSERT(collection != nullptr);

    velocypack::Builder body;
    {
      VPackObjectBuilder b(&body);
      body.add(StaticStrings::IndexType, VPackValue("persistent"));
      body.add(StaticStrings::IndexSparse, VPackValue(false));
      body.add(StaticStrings::IndexUnique, VPackValue(false));
      body.add(StaticStrings::IndexEstimates, VPackValue(false));

      body.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
      // the actual indexed field
      body.add(VPackValue("started"));

      body.close();  // "fields"
    }

    velocypack::Builder out;
    Result res =
        methods::Indexes::ensureIndex(*collection, body.slice(), true, out)
            .waitAndGet();
    return res;
  }

  // insert a batch of queries into the _queries system collection
  Result insertQueries(
      std::vector<std::shared_ptr<velocypack::String>> const& queries) {
    TRI_ASSERT(!queries.empty());

    // give ourselves full permissions to write into the system collection
    ExecContextSuperuserScope scope;

    auto vocbase = getSystemDatabase();

    if (vocbase == nullptr) {
      return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
              "unable to find _system database"};
    }

    auto origin = transaction::OperationOriginInternal{
        "storing queries in system collection"};
    auto ctx = transaction::StandaloneContext::create(*vocbase, origin);
    SingleCollectionTransaction trx(std::move(ctx),
                                    StaticStrings::QueriesCollection,
                                    AccessMode::Type::WRITE);
    if (auto res = trx.begin(); !res.ok()) {
      return res;
    }

    velocypack::Buffer<uint8_t> buffer;
    velocypack::Builder body(buffer);
    body.openArray(/*unindexed*/ true);
    for (auto const& it : queries) {
      body.add(it->slice());
    }
    body.close();

    OperationOptions options;
    options.waitForSync = false;
    options.silent = true;

    OperationResult result =
        trx.insert(StaticStrings::QueriesCollection, body.slice(), options);
    return trx.finish(result.result);
  }

  // maximum number of queries that can be buffered in memory before
  // being inserted into the _queries system collection.
  size_t const _maxBufferedQueries;

  // interval (in milliseconds) with which buffered queries will be pushed out
  // to the _queries system collection. to amortize the cost of writing query
  // information into the system collection, one can increase the push interval.
  // this increases the chances of multiple queries being written to the system
  // collection with a single write operation. it will delay writing to the
  // system collection however.
  uint64_t const _pushInterval;

  // interval (in milliseconds) with which outdated query information is purged
  // from the _queries system collection.
  uint64_t const _cleanupInterval;

  // retention time (in seconds) for which queries are kept in the _queries
  // system collection before they are purged.
  double const _retentionTime;

  // mutex and condition variable that protect _queries.
  std::mutex _mutex;
  std::condition_variable _condition;

  // query slices, buffered in memory. the slices are owned by the
  // vector. protected by _mutex
  std::vector<std::shared_ptr<velocypack::String>> _queries;

  // mutex that protects _workItem.
  std::mutex _workItemMutex;
  // scheduler work item for a garbage collection task that is
  // periodically scheduled for the _queries collection.
  // the work item is protected by _workItemMutex.
  Scheduler::WorkHandle _workItem;
};

QueryInfoLoggerFeature::QueryInfoLoggerFeature(Server& server)
    : ArangodFeature{server, *this},
      _maxBufferedQueries(4096),
      _logEnabled(false),
      _logSystemDatabaseQueries(false),
      _logSlowQueries(true),
      _logProbability(0.1),
      _pushInterval(3'000),
      _cleanupInterval(600'000),
      _retentionTime(28800.0) {
  setOptional(true);
  // we need to be able to run AQL queries here ourselves.
  startsAfter<DatabaseFeaturePhase>();
  startsAfter<RocksDBEngine>();
  startsAfter<CommunicationFeaturePhase>();
}

QueryInfoLoggerFeature::~QueryInfoLoggerFeature() { stopThread(); }

void QueryInfoLoggerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--query.collection-logger-max-buffered-queries",
                  "The maximum number of queries to buffer for query "
                  "collection logging.",
                  new options::UInt64Parameter(&_maxBufferedQueries),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnCoordinator,
                                            options::Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption(
          "--query.collection-logger-enabled",
          "Whether or not to enable logging of queries to a system collection.",
          new options::BooleanParameter(&_logEnabled),
          options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                    options::Flags::OnCoordinator,
                                    options::Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-include-system-database",
                  "Whether or not to include _system database queries in query "
                  "collection logging.",
                  new options::BooleanParameter(&_logSystemDatabaseQueries),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnCoordinator,
                                            options::Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-all-slow-queries",
                  "Whether or not to include all slow queries in query "
                  "collection logging.",
                  new options::BooleanParameter(&_logSlowQueries),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnCoordinator,
                                            options::Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption(
          "--query.collection-logger-probability",
          "The probability with which queries are included in query collection "
          "logging.",
          new options::DoubleParameter(&_logProbability, 1.0, 0.0, 100.0),
          options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                    options::Flags::OnCoordinator,
                                    options::Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-retention-time",
                  "The time duration (in seconds) for with which queries are "
                  "kept in the "
                  "_queries system collection before they are purged.",
                  new options::DoubleParameter(&_retentionTime, 1.0, 1.0),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnCoordinator,
                                            options::Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-push-interval",
                  "The interval (in milliseconds) in which query information "
                  "is flushed to the _queries system collection.",
                  new options::UInt64Parameter(&_pushInterval),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnCoordinator,
                                            options::Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-cleanup-interval",
                  "The interval (in milliseconds) in which query information "
                  "is purged from the _queries system collection.",
                  new options::UInt64Parameter(&_cleanupInterval, 1, 1'000),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnCoordinator,
                                            options::Flags::OnSingle))
      .setIntroducedIn(31202);
}

void QueryInfoLoggerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {}

void QueryInfoLoggerFeature::beginShutdown() {
  if (_loggerThread) {
    _loggerThread->beginShutdown();
  }
}

void QueryInfoLoggerFeature::start() {
  if (!_logEnabled) {
    // feature is turned off. no need to execute the garbage collection
    return;
  }

  if (!ServerState::instance()->isSingleServerOrCoordinator()) {
    // this feature is only available on single servers and coordinators
    return;
  }

  _loggerThread = std::make_unique<QueryInfoLoggerThread>(
      server(), _maxBufferedQueries, _pushInterval, _cleanupInterval,
      _retentionTime);

  if (!_loggerThread->start()) {
    LOG_TOPIC("cfbe5", FATAL, Logger::STARTUP)
        << "could not start query info logger thread";
    FATAL_ERROR_EXIT();
  }
}

void QueryInfoLoggerFeature::stop() { stopThread(); }

// whether or not a particular query should be logged.
bool QueryInfoLoggerFeature::shouldLog(bool isSystem,
                                       bool isSlowQuery) const noexcept {
  if (!_logEnabled) {
    return false;
  }
  if (isSystem && !_logSystemDatabaseQueries) {
    // don't log any queries in _system database
    return false;
  }
  if (isSlowQuery && _logSlowQueries) {
    // log all slow queries
    return true;
  }
  return (_logProbability >= 100.0 ||
          (_logProbability > 0.0 &&
           static_cast<double>(RandomGenerator::interval(uint32_t(100'000))) /
                   1000.0 <=
               _logProbability));
}

// accept a query for logging. the logging itself may be carried out later
// because it is asynchronous.
void QueryInfoLoggerFeature::log(
    std::shared_ptr<velocypack::String> const& query) {
  if (_loggerThread != nullptr) {
    _loggerThread->log(query);
  }
}

void QueryInfoLoggerFeature::stopThread() { _loggerThread.reset(); }

}  // namespace arangodb::aql
