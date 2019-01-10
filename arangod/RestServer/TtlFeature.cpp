////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <thread>

using namespace arangodb;
using namespace arangodb::options;
        
namespace {
// the AQL query to remove documents
std::string const removeQuery("FOR doc IN @@collection FILTER doc.@indexAttribute >= 0 && doc.@indexAttribute <= @stamp SORT doc.@indexAttribute LIMIT @limit REMOVE doc IN @@collection OPTIONS { ignoreErrors: true }");
}

namespace arangodb {

class TtlThread final : public Thread {
 public:
  TtlThread(std::chrono::milliseconds frequency, 
            uint64_t maxTotalRemoves, 
            uint64_t maxCollectionRemoves) 
       : Thread("TTL"), 
         _frequency(frequency),
         _maxTotalRemoves(maxTotalRemoves),
         _maxCollectionRemoves(maxCollectionRemoves),
         _nextStart(std::chrono::steady_clock::now() + frequency) {}

  ~TtlThread() { shutdown(); }

  void beginShutdown() override {
    Thread::beginShutdown();

    // wake up the thread that may be waiting in run()
    CONDITION_LOCKER(guard, _condition);
    guard.broadcast();
  }

 protected:
  void run() override {
    LOG_TOPIC(TRACE, Logger::FIXME) << "starting TTL background thread with interval " << _frequency.count() << " milliseconds";

    while (!isStopping()) {
      auto const now = std::chrono::steady_clock::now();

      if (now < _nextStart) {
        // wait for our start...
        CONDITION_LOCKER(guard, _condition);
        guard.wait(std::chrono::microseconds(std::chrono::duration_cast<std::chrono::microseconds>(_nextStart - now)));
      }

      if (isStopping()) {
        break;
      }
      
      _nextStart += _frequency;

      try {
        work();
      } catch (std::exception const& ex) {
        LOG_TOPIC(WARN, Logger::FIXME) << "caught exception in TTL background thread: " << ex.what();
      } catch (...) {
        LOG_TOPIC(WARN, Logger::FIXME) << "caught unknown exception in TTL background thread";
      }
    }
  }

 private:
  void work() {
    auto queryRegistryFeature =
        application_features::ApplicationServer::getFeature<QueryRegistryFeature>(
            "QueryRegistry");
    auto queryRegistry = queryRegistryFeature->queryRegistry();

    double const stamp = TRI_microtime();
    uint64_t limitLeft = _maxTotalRemoves; 
    
    // iterate over all databases  
    auto db = DatabaseFeature::DATABASE;
    for (auto const& name : db->getDatabaseNames()) {
      TRI_vocbase_t* vocbase = db->useDatabase(name);

      if (vocbase == nullptr) {
        continue;
      }
      
      // make sure we decrease the reference counter later
      TRI_DEFER(vocbase->release());

      std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections = vocbase->collections(false);

      for (auto const& collection : collections) {
        if (collection->status() != TRI_VOC_COL_STATUS_LOADED) {
          // we only care about collections that are already loaded here.
          // otherwise our check may load them all into memory, and sure this is
          // something we want to avoid here
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
          LOG_TOPIC(DEBUG, Logger::FIXME) << "TTL thread going to work for collection " << collection->name() << ", expireAfter: " << expireAfter << ", stamp: " << (stamp - expireAfter) << ", limit: " << std::min(_maxCollectionRemoves, limitLeft);

          auto bindVars = std::make_shared<VPackBuilder>();
          bindVars->openObject();
          bindVars->add("@collection", VPackValue(collection->name()));
          bindVars->add(VPackValue("indexAttribute"));
          bindVars->openArray();
          for (auto const& it : index->fields()[0]) {
            bindVars->add(VPackValue(it.name));
          }
          bindVars->close();
          bindVars->add("stamp", VPackValue(stamp - expireAfter));
          bindVars->add("limit", VPackValue(std::min(_maxCollectionRemoves, limitLeft)));
          bindVars->close();

          aql::Query query(false, *vocbase, aql::QueryString(::removeQuery), bindVars, nullptr, arangodb::aql::PART_MAIN);
          aql::QueryResult queryResult = query.executeSync(queryRegistry);

          if (queryResult.code != TRI_ERROR_NO_ERROR) {
            // we can probably live with an error here...
            // the thread will try to remove the documents again on next iteration
            LOG_TOPIC(DEBUG, Logger::FIXME) << "error during TTL document removal: " << queryResult.details;
          } else {
            auto extra = queryResult.extra;
            if (extra != nullptr) {
              VPackSlice v = extra->slice().get("stats");
              if (v.isObject()) {
                v = v.get("writesExecuted");
                if (v.isNumber()) {
                  uint64_t removed = v.getNumericValue<uint64_t>();
                  LOG_TOPIC(DEBUG, Logger::FIXME) << "TTL thread removed " << removed << " documents for collection " << collection->name();
                  if (limitLeft >= removed) {
                    limitLeft -= removed;
                  } else { 
                    limitLeft = 0;
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
  arangodb::basics::ConditionVariable _condition;
  
  /// @brief the thread run interval
  std::chrono::milliseconds const _frequency;
  
  uint64_t const _maxTotalRemoves;
  uint64_t const _maxCollectionRemoves;
  
  /// @brief next time the thread should run
  std::chrono::time_point<std::chrono::steady_clock> _nextStart;

  /// @brief a builder object we reuse to save a few memory allocations
  VPackBuilder _builder;
};

}

TtlFeature::TtlFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "Ttl"), 
      _frequency(60 * 1000),
      _maxTotalRemoves(1000000),
      _maxCollectionRemoves(100000) {
  startsAfter("ServerPhase");

  // TODO!
}

TtlFeature::~TtlFeature() {
  shutdownThread();
}

void TtlFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("ttl", "TTL options");

  options->addOption("ttl.frequency", "frequency (in milliseconds) for the TTL background thread invocation. "
                     "a value of 0 turns the TTL background thread off entirely",
                     new UInt64Parameter(&_frequency));
  
  options->addOption("ttl.max-total-removes", "maximum number of documents to remove per invocation of the TTL thread",
                     new UInt64Parameter(&_maxTotalRemoves));
  
  options->addOption("ttl.max-collection-removes", "maximum number of documents to remove per collection",
                     new UInt64Parameter(&_maxCollectionRemoves));
}

void TtlFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // TODO
}

void TtlFeature::start() {
  // the thread will not run on a coordinator or an agency node,
  // just locally on DB servers or single servers
  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isAgent()) {
    return;
  }

  // a frequency of 0 means the thread is not started at all
  if (_frequency == 0) {
    return;
  }

  _thread.reset(new TtlThread(std::chrono::milliseconds(_frequency), _maxTotalRemoves, _maxCollectionRemoves));

  if (!_thread->start()) {
    LOG_TOPIC(FATAL, Logger::ENGINES) << "could not start ttl background thread";
    FATAL_ERROR_EXIT();
  }
}

void TtlFeature::beginShutdown() {
  if (_thread != nullptr) {
    _thread->beginShutdown();
  }
}

void TtlFeature::stop() {
  shutdownThread();
}

void TtlFeature::shutdownThread() noexcept {
  try {
    if (_thread != nullptr) {
      _thread->beginShutdown();
      while (_thread->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  } catch (...) {
  }

  _thread.reset();
}
