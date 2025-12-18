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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include <Basics/StaticStrings.h>
#include <thread>

#include "RestServer/AqlFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/ApiRecordingFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;

namespace {
// number of leases currently handed out
// the highest bit of the value has a special meaning
// and must be masked out.
// if it is set, then a new lease can be acquired.
// if not set, then no new lease can be acquired.
std::atomic<uint64_t> leases{0};
static constexpr uint64_t readyBit = 0x8000000000000000ULL;
}  // namespace

namespace arangodb {

AqlFeature::AqlFeature(Server& server) : ArangodFeature{server, *this} {
  setOptional(false);
  startsAfter<application_features::ClusterFeaturePhase>();
  startsAfter<QueryRegistryFeature>();
}

AqlFeature::~AqlFeature() {
  // always clean up here
  ::leases.fetch_and(~::readyBit);
}

bool AqlFeature::lease() noexcept {
  uint64_t previous = ::leases.fetch_add(1);
  if ((previous & ::readyBit) == 0) {
    // oops, no lease can be acquired yet.
    // revert the increase and return false
    ::leases.fetch_sub(1);
    return false;
  }
  return true;
}

void AqlFeature::unlease() noexcept { ::leases.fetch_sub(1); }

void AqlFeature::start() {
  ::leases.fetch_or(::readyBit);

  LOG_TOPIC("cf921", DEBUG, Logger::QUERIES) << "AQL feature started";
}

void AqlFeature::stop() {
  ::leases.fetch_and(~::readyBit);

  LOG_TOPIC("8ed81", DEBUG, Logger::QUERIES) << "AQL feature stopped";

  // Wait until all AQL queries are done
  auto queryRegistry = QueryRegistryFeature::registry();
  TRI_ASSERT(queryRegistry != nullptr);
  unsigned i = 0;
  while (true) {
    try {
      queryRegistry->destroyAll();
    } catch (...) {
      // ignore errors here. if it fails, we'll try again in next round
    }

    uint64_t m = ::leases.load();
    TRI_ASSERT((m & ::readyBit) == 0);

    size_t n = queryRegistry->numberRegisteredQueries();

    if (n == 0 && m == 0) {
      break;
    }
    LOG_TOPIC_IF("63d54", INFO, Logger::QUERIES, (i++ % 64) == 0)
        << "AQLFeature shutdown, waiting for " << n
        << " registered queries to terminate and for " << m
        << " feature leases to be released";
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}

ApiRecordingFeature* AqlFeature::apiRecordingFeature() {
  // This can return `nullptr` if there is no ApiRecordingFeature, as it is
  // for example the case in Tests.
  if (_apiRecordingFeature == nullptr) {
    if (server().hasFeature<arangodb::ApiRecordingFeature>()) {
      _apiRecordingFeature =
          &(server().getFeature<arangodb::ApiRecordingFeature>());
    }
  }
  return _apiRecordingFeature;
}

std::shared_ptr<arangodb::aql::Query> AqlFeature::createQuery(
    std::shared_ptr<arangodb::transaction::Context> ctx,
    arangodb::aql::QueryString queryString,
    std::shared_ptr<velocypack::Builder> bindParameters,
    arangodb::aql::QueryOptions options, Scheduler* scheduler) {
  TRI_ASSERT(ctx != nullptr);
  // workaround to enable make_shared on a class with a protected constructor
  struct MakeSharedQuery final : arangodb::aql::Query {
    MakeSharedQuery(std::shared_ptr<transaction::Context> ctx,
                    arangodb::aql::QueryString queryString,
                    std::shared_ptr<velocypack::Builder> bindParameters,
                    arangodb::aql::QueryOptions options, Scheduler* scheduler)
        : Query{std::move(ctx), std::move(queryString),
                std::move(bindParameters), std::move(options), scheduler} {}

    ~MakeSharedQuery() final {
      // Destroy this query, otherwise it's still
      // accessible while the query is being destructed,
      // which can result in a data race on the vptr
      destroy();
    }
  };
  TRI_ASSERT(ctx != nullptr);

  // Record the Query:
  arangodb::ApiRecordingFeature* apiRecordingFeat = apiRecordingFeature();
  if (apiRecordingFeat != nullptr &&
      (arangodb::ServerState::instance()->isCoordinator() ||
       arangodb::ServerState::instance()->isSingleServer())) {
    velocypack::SharedSlice bindParamsCopy;
    if (bindParameters != nullptr && !bindParameters->isEmpty() &&
        !bindParameters->slice().isNone() &&
        bindParameters->slice().byteSize() < 1024) {
      bindParamsCopy = velocypack::SharedSlice(bindParameters->bufferRef());
    } else {
      VPackBuilder builder;
      { VPackObjectBuilder guard(&builder); }
      bindParamsCopy = velocypack::SharedSlice{*builder.steal()};
    }
    apiRecordingFeat->recordAQLQuery(
        queryString.string(), ctx->vocbase().name(), std::move(bindParamsCopy));
  }
  return std::make_shared<MakeSharedQuery>(
      std::move(ctx), std::move(queryString), std::move(bindParameters),
      std::move(options), scheduler);
}

}  // namespace arangodb
