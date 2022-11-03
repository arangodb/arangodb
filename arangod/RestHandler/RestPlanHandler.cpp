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
/// @author Aditya Mukhopadhyay, Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "RestPlanHandler.h"

#include "Aql/Query.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Transaction/StandaloneContext.h"
#include "Logger/LogMacros.h"

#include "Aql/Optimizer2/Inspection/VPackInspection.h"
#include "Aql/Optimizer2/Plan/PlanRPCHandler.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

RestPlanHandler::RestPlanHandler(ArangodServer& server, GeneralRequest* request,
                                 GeneralResponse* response,
                                 aql::QueryRegistry* queryRegistry)
    : RestCursorHandler(server, request, response, queryRegistry) {}

RestStatus RestPlanHandler::execute() {
  // extract the sub-request type
  rest::RequestType const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    std::vector<std::string> const& suffixes = _request->suffixes();

    if (!suffixes.empty()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting POST /_api/plan");
      return RestStatus::FAIL;
    }

    /* RPC Parsing */
    bool parseSuccess;                   // needed to populate the _vpackBuilder
    this->parseVPackBody(parseSuccess);  // needed to populate the _vpackBuilder

    auto res = deserializeWithStatus<optimizer2::plan::PlanRPC>(
        _request->_vpackBuilder->sharedSlice());
    if (!res.ok()) {
      std::string errorMessage =
          "Error: " + res.error() + ", path: " + res.path();
      generateError(rest::ResponseCode::BAD, TRI_ERROR_MALFORMED_JSON,
                    errorMessage);
      return RestStatus::FAIL;
    }
    auto cmd = res.get().parsed;

    /* Valid Body Inputs */
    using GeneratePlanCMD = arangodb::aql::optimizer2::plan::QueryPostBody;
    using ExecutePlanCMD = arangodb::aql::optimizer2::plan::VerbosePlan;

    auto resultRestStatus = std::visit(
        overloaded{
            [this](GeneratePlanCMD const& plan) {
              //
              // Generate the initial non optimized query plan
              //
              VPackBuilder bindBuilder;
              VPackBuilder optionsBuilder;

              if (plan.bindVars.has_value()) {
                bindBuilder = std::move(plan.bindVars.value());
              }
              if (plan.options.has_value()) {
                using QueryOpts = aql::optimizer2::plan::QueryPostOptions;
                QueryOpts x = plan.options.value();
                auto serializedQueryOpts =
                    velocypack::serializeWithStatus<QueryOpts>(x);
                optionsBuilder.add(serializedQueryOpts->slice());
              } else {
                optionsBuilder.openObject();
                optionsBuilder.add("verbosePlans", VPackValue(true));
                optionsBuilder.close();
              }

              auto query = arangodb::aql::Query::create(
                  transaction::StandaloneContext::Create(_vocbase),
                  aql::QueryString(plan.query),
                  std::make_shared<VPackBuilder>(bindBuilder),
                  aql::QueryOptions(optionsBuilder.slice()));

              constexpr bool optimize = false;
              auto queryResult = query->doExplain(optimize);

              if (queryResult.result.fail()) {
                generateError(queryResult.result);
                return RestStatus::FAIL;
              }

              // In case of success
              VPackBuilder result;
              result.openObject();

              /* Copy paste from RestExplainHandler.cpp (Building queryResult)
               * TODO: Unify this - use inspector instead maybe?!
               */
              if (query->queryOptions().allPlans) {
                result.add("plans", queryResult.data->slice());
              } else {
                result.add("plan", queryResult.data->slice());
                result.add("cacheable", VPackValue(queryResult.cached));
              }

              VPackSlice extras = queryResult.extra->slice();
              if (extras.hasKey("warnings")) {
                result.add("warnings", extras.get("warnings"));
              } else {
                result.add("warnings", VPackSlice::emptyArraySlice());
              }
              if (extras.hasKey("stats")) {
                result.add("stats", extras.get("stats"));
              }

              result.add(StaticStrings::Error, VPackValue(false));
              result.add(StaticStrings::Code,
                         VPackValue(static_cast<int>(ResponseCode::OK)));
              result.close();

              generateResult(rest::ResponseCode::OK, result.slice());
              /*
               * Copy paste section end
               */

              return RestStatus::DONE;
            },
            [this](ExecutePlanCMD const& plan) {
              //
              // Execute any query plan
              //
              VPackSlice planSlice = plan.plan.slice();

              // Just use always default options for now...
              VPackBuilder emptyOptions;
              emptyOptions.openObject();
              emptyOptions.close();
              buildOptions(emptyOptions.slice());

              const AccessMode::Type mode = AccessMode::Type::WRITE;
              auto query = Query::createFromPlan(_vocbase,
                                                 createTransactionContext(mode),
                                                 planSlice, _options->slice());

              return registerQueryOrCursor(query);
            },
            [this](auto) {
              //
              // Fallback if command is not known
              //
              generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                            "unsupported body definition");
              return RestStatus::FAIL;
            }},
        cmd);

    return resultRestStatus;
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}