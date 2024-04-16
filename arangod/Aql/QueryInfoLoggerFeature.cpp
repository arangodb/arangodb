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
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/SystemDatabaseFeature.h"
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
                                 size_t maxBufferedQueries)
      : ServerThread<ArangodServer>(server, "QueryInfoLogger"),
        _maxBufferedQueries(maxBufferedQueries) {}

  ~QueryInfoLoggerThread() { shutdown(); }

  void beginShutdown() override {
    Thread::beginShutdown();

    std::lock_guard guard{_mutex};
    _condition.notify_one();
  }

  void log(std::shared_ptr<velocypack::String> query) {
    {
      std::lock_guard guard{_mutex};

      if (_queries.size() < _maxBufferedQueries) {
        _queries.emplace_back(std::move(query));
      }
    }
    _condition.notify_one();
  }

 protected:
  void run() override {
    enum class PrepareState {
      // must check if _queries system collection exists
      kCheckCollection,
      // collection is present - must check if TTL index on _queries system
      // collection exists
      kCheckIndex,
      // collection and index are present - no checks necessary
      kCheckNothing,
    };

    PrepareState prepareState = PrepareState::kCheckCollection;

    while (true) {
      decltype(_queries) queries;

      try {
        std::unique_lock guard{_mutex};

        if (_queries.empty()) {
          if (isStopping()) {
            // exit thread loop
            break;
          }
          // only sleep if we have no queries left to process
          _condition.wait_for(guard, std::chrono::seconds{10});
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
  }

 private:
  Result ensureCollection() {
    ExecContextSuperuserScope scope;

    std::string const& name = StaticStrings::QueriesCollection;

    SystemDatabaseFeature::ptr vocbase =
        server().hasFeature<SystemDatabaseFeature>()
            ? server().getFeature<SystemDatabaseFeature>().use()
            : nullptr;

    if (vocbase == nullptr) {
      return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
              "unable to find _system database"};
    }

    std::shared_ptr<LogicalCollection> collection;
    if (Result res = methods::Collections::lookup(*vocbase, name, collection);
        res.fail()) {
      OperationOptions options{};
      res = methods::Collections::createSystem(
          *vocbase, options, name, /*isNewDatabase*/ false, collection);

      if (res.fail()) {
        return {res.errorNumber(),
                absl::StrCat("unable to create _queries system collection: ",
                             res.errorMessage())};
      }
    }
    // success
    return {};
  }

  Result ensureIndex() {
    ExecContextSuperuserScope scope;

    std::string const& name = StaticStrings::QueriesCollection;

    SystemDatabaseFeature::ptr vocbase =
        server().hasFeature<SystemDatabaseFeature>()
            ? server().getFeature<SystemDatabaseFeature>().use()
            : nullptr;

    if (vocbase == nullptr) {
      return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
              "unable to find _system database"};
    }

    std::shared_ptr<LogicalCollection> collection;

    if (Result res = methods::Collections::lookup(*vocbase, name, collection);
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
      body.add(StaticStrings::IndexType, VPackValue("ttl"));
      body.add(StaticStrings::IndexSparse, VPackValue(false));
      body.add(StaticStrings::IndexUnique, VPackValue(false));
      body.add(StaticStrings::IndexEstimates, VPackValue(false));

      body.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
      body.add(VPackValue("started"));
      body.close();  // "fields"

      body.add(StaticStrings::IndexExpireAfter, VPackValue(86400));
    }

    velocypack::Builder out;
    Result res =
        methods::Indexes::ensureIndex(*collection, body.slice(), true, out)
            .get();
    return res;
  }

  Result insertQueries(
      std::vector<std::shared_ptr<velocypack::String>> const& queries) {
    TRI_ASSERT(!queries.empty());

    ExecContextSuperuserScope scope;

    SystemDatabaseFeature::ptr vocbase =
        server().hasFeature<SystemDatabaseFeature>()
            ? server().getFeature<SystemDatabaseFeature>().use()
            : nullptr;

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
    Result res = trx.begin();

    if (!res.ok()) {
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

  size_t const _maxBufferedQueries;

  // mutex and condition variable that protect _queries
  std::mutex _mutex;
  std::condition_variable _condition;

  std::vector<std::shared_ptr<velocypack::String>> _queries;
};

QueryInfoLoggerFeature::QueryInfoLoggerFeature(Server& server)
    : ArangodFeature{server, *this}, _maxBufferedQueries(1024) {
  setOptional(true);
  startsAfter<DatabaseFeaturePhase>();
  startsAfter<RocksDBEngine>();
}

QueryInfoLoggerFeature::~QueryInfoLoggerFeature() { stopThread(); }

void QueryInfoLoggerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--query.logger-max-buffered-queries",
                  "The maximum number of buffered queries to log.",
                  new options::UInt64Parameter(&_maxBufferedQueries),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnCoordinator,
                                            options::Flags::OnSingle))
      .setIntroducedIn(31300);
}

void QueryInfoLoggerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {}

void QueryInfoLoggerFeature::start() {
  if (!ServerState::instance()->isSingleServerOrCoordinator()) {
    // this feature is only available on single servers and coordinators
    return;
  }

  _loggerThread =
      std::make_unique<QueryInfoLoggerThread>(server(), _maxBufferedQueries);

  if (!_loggerThread->start()) {
    LOG_TOPIC("cfbe5", FATAL, Logger::STARTUP)
        << "could not start query info logger thread";
    FATAL_ERROR_EXIT();
  }
}

void QueryInfoLoggerFeature::stop() { stopThread(); }

void QueryInfoLoggerFeature::log(
    std::shared_ptr<velocypack::String> const& query) {
  if (_loggerThread != nullptr) {
    _loggerThread->log(query);
  }
}

void QueryInfoLoggerFeature::stopThread() { _loggerThread.reset(); }

}  // namespace arangodb::aql
