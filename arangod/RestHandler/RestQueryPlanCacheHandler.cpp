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
////////////////////////////////////////////////////////////////////////////////

#include "RestQueryPlanCacheHandler.h"
#include "Aql/QueryPlanCache.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::rest;

RestQueryPlanCacheHandler::RestQueryPlanCacheHandler(ArangodServer& server,
                                                     GeneralRequest* request,
                                                     GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestQueryPlanCacheHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  switch (type) {
    case rest::RequestType::GET:
      readPlans();
      break;
    case rest::RequestType::DELETE_REQ:
      clearCache();
      break;
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_NOT_IMPLEMENTED,
                    "illegal method for /_api/query-plan-cache");
      break;
  }

  return RestStatus::DONE;
}

void RestQueryPlanCacheHandler::clearCache() {
  if (!ExecContext::current().canUseDatabase(auth::Level::RW)) {
    generateError(
        rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
        "not allowed to clear this database's query plan cache entries");
    return;
  }

  _vocbase.queryPlanCache().invalidateAll();

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code,
             VPackValue(static_cast<int>(rest::ResponseCode::OK)));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

void RestQueryPlanCacheHandler::readPlans() {
  if (!ExecContext::current().canUseDatabase(auth::Level::RO)) {
    generateError(
        rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
        "not allowed to retrieve this database's query plan cache entries");
    return;
  }

  auto filter = [](aql::QueryPlanCache::Key const& key,
                   aql::QueryPlanCache::Value const& value) -> bool {
    if (ExecContext::isAuthEnabled() && !ExecContext::current().isSuperuser()) {
      // check if non-superusers have at least read permissions on all
      // collections/views used in the query
      for (auto const& dataSource : value.dataSources) {
        if (!ExecContext::current().canUseCollection(dataSource.second.name,
                                                     auth::Level::RO)) {
          return false;
        }
      }
    }
    return true;
  };

  VPackBuilder result;
  _vocbase.queryPlanCache().toVelocyPack(result, filter);
  generateResult(rest::ResponseCode::OK, result.slice());
}
