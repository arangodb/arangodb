////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for transaction rest handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Query.h"

#include "Basics/VelocyPackHelper.h"
#include "Transaction/Manager.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/SmartContext.h"
#include "Transaction/Status.h"

#include "RestHandler/RestTransactionHandler.h"

#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "catch.hpp"

#include "../IResearch/RestHandlerMock.h"
#include "ManagerSetup.h"


using namespace arangodb;
using arangodb::basics::VelocyPackHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test transaction Rest Handler
TEST_CASE("RestTransactionHandlerTest", "[transaction]") {
  arangodb::tests::mocks::TransactionManagerSetup setup;
  TRI_ASSERT(transaction::ManagerFeature::manager());
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  
  auto* mgr = transaction::ManagerFeature::manager();
  REQUIRE(mgr != nullptr);
  scopeGuard([&] {
    mgr->garbageCollect(true);
  });
  
  auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::RestTransactionHandler handler(requestPtr.release(), responcePtr.release());
  velocypack::Parser parser(request._payload);
  
  CHECK((vocbase.collections(false).empty()));
  
  SECTION("Parsing errors") {
    request.setRequestType(arangodb::rest::RequestType::POST);
    request.addSuffix("begin");
    parser.parse("{ \"write\": [33] }");
    
    arangodb::RestStatus status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    VPackSlice slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::BAD) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_BAD_PARAMETER == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }
  

  SECTION("Collection Not found RO") {
    request.setRequestType(arangodb::rest::RequestType::POST);
    request.addSuffix("begin");
    parser.parse("{ \"collections\":{\"read\": [\"33\"]}}");
    
    arangodb::RestStatus status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    VPackSlice slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }
  
  SECTION("Collection Not found (write)") {
    request.setRequestType(arangodb::rest::RequestType::POST);
    request.addSuffix("begin");
    parser.parse("{ \"collections\":{\"write\": [\"33\"]}}");
    
    arangodb::RestStatus status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    VPackSlice slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }
  
  SECTION("Collection Not found (exclusive)") {
    request.setRequestType(arangodb::rest::RequestType::POST);
    request.addSuffix("begin");
    parser.parse("{ \"collections\":{\"exclusive\": [\"33\"]}}");
    
    arangodb::RestStatus status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    VPackSlice slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  std::shared_ptr<LogicalCollection> coll;
  {
    auto json = VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  REQUIRE(coll != nullptr);
  
  SECTION("Simple Transaction & Abort") {
    request.setRequestType(arangodb::rest::RequestType::POST);
    request.addSuffix("begin");
    parser.parse("{ \"collections\":{\"read\": [\"42\"]}}");
    
    arangodb::RestStatus status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::CREATED == responce.responseCode()));
    VPackSlice slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::CREATED) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    
    CHECK(slice.hasKey("result"));
    std::string tid = slice.get("result").get("id").copyString();
    REQUIRE(std::stol(tid) != 0);
    CHECK(slice.get("result").get("status").isEqualString("running"));
    
    // GET status
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.clearSuffixes();
    request.addSuffix(tid);
    
    status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    
    CHECK(slice.hasKey("result"));
    CHECK(slice.get("result").get("id").copyString() == tid);
    CHECK(slice.get("result").get("status").isEqualString("running"));
    
    // DELETE abort trx

    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    
    status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    
    CHECK(slice.hasKey("result"));
    CHECK(slice.get("result").get("id").copyString() == tid);
    CHECK(slice.get("result").get("status").isEqualString("aborted"));
  }
  
  SECTION("Simple Transaction & Commit") {
    request.setRequestType(arangodb::rest::RequestType::POST);
    request.addSuffix("begin");
    parser.parse("{ \"collections\":{\"read\": [\"42\"]}}");
    
    arangodb::RestStatus status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::CREATED == responce.responseCode()));
    VPackSlice slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::CREATED) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    
    CHECK(slice.hasKey("result"));
    std::string tid = slice.get("result").get("id").copyString();
    REQUIRE(std::stol(tid) != 0);
    CHECK(slice.get("result").get("status").isEqualString("running"));
    
    
    // PUT commit trx
    request.setRequestType(arangodb::rest::RequestType::PUT);
    request.clearSuffixes();
    request.addSuffix(tid);
    
    status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    
    CHECK(slice.hasKey("result"));
    CHECK(slice.get("result").get("id").copyString() == tid);
    CHECK(slice.get("result").get("status").isEqualString("committed"));
  }
  
  SECTION("Permission denied (read only)") {
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Internal, "dummy", "testVocbase",
                                           arangodb::auth::Level::RO, arangodb::auth::Level::RO) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    
    request.setRequestType(arangodb::rest::RequestType::POST);
    request.addSuffix("begin");
    parser.parse("{ \"collections\":{\"write\": [\"42\"]}}");
    
    arangodb::RestStatus status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
    VPackSlice slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_READ_ONLY == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }
  
  SECTION("Permission denied (forbidden)") {
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Internal, "dummy", "testVocbase",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    
    request.setRequestType(arangodb::rest::RequestType::POST);
    request.addSuffix("begin");
    parser.parse("{ \"collections\":{\"write\": [\"42\"]}}");
    
    arangodb::RestStatus status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
    VPackSlice slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }
}
