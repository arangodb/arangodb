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
#include "Transaction/SmartContext.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Status.h"

#include "RestHandler/RestTransactionHandler.h"

#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "gtest/gtest.h"

#include "../IResearch/common.h"
#include "../IResearch/RestHandlerMock.h"
#include "ManagerSetup.h"

using namespace arangodb;
using arangodb::basics::VelocyPackHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

class RestTransactionHandlerTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::TransactionManagerSetup setup;
  TRI_vocbase_t vocbase;

  transaction::Manager* mgr;

  std::unique_ptr<GeneralRequestMock> requestPtr;
  GeneralRequestMock& request;
  std::unique_ptr<GeneralResponseMock> responcePtr;
  GeneralResponseMock& responce;
  arangodb::RestTransactionHandler handler;
  velocypack::Parser parser;

  RestTransactionHandlerTest()
      : vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(setup.server.server())),
        mgr(transaction::ManagerFeature::manager()),
        requestPtr(std::make_unique<GeneralRequestMock>(vocbase)),
        request(*requestPtr),
        responcePtr(std::make_unique<GeneralResponseMock>()),
        responce(*responcePtr),
        handler(setup.server.server(), requestPtr.release(), responcePtr.release()),
        parser(request._payload) {
    EXPECT_TRUE(vocbase.collections(false).empty());
  }

  ~RestTransactionHandlerTest() { mgr->garbageCollect(true); }
};

TEST_F(RestTransactionHandlerTest, parsing_errors) {
  request.setRequestType(arangodb::rest::RequestType::POST);
  request.addSuffix("begin");
  parser.parse("{ \"write\": [33] }");

  arangodb::RestStatus status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::BAD, responce.responseCode());
  VPackSlice slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::BAD) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
               slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
               TRI_ERROR_BAD_PARAMETER ==
                   slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
}

TEST_F(RestTransactionHandlerTest, collection_not_found_ro) {
  request.setRequestType(arangodb::rest::RequestType::POST);
  request.addSuffix("begin");
  parser.parse("{ \"collections\":{\"read\": [\"33\"]}}");

  arangodb::RestStatus status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, responce.responseCode());
  VPackSlice slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
               slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
               TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                   slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
}

TEST_F(RestTransactionHandlerTest, collection_not_found_write) {
  request.setRequestType(arangodb::rest::RequestType::POST);
  request.addSuffix("begin");
  parser.parse("{ \"collections\":{\"write\": [\"33\"]}}");

  arangodb::RestStatus status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, responce.responseCode());
  VPackSlice slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
               slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
               TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                   slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
}

TEST_F(RestTransactionHandlerTest, collection_not_found_exclusive) {
  request.setRequestType(arangodb::rest::RequestType::POST);
  request.addSuffix("begin");
  parser.parse("{ \"collections\":{\"exclusive\": [\"33\"]}}");

  arangodb::RestStatus status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, responce.responseCode());
  VPackSlice slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
               slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
               TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                   slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
}

TEST_F(RestTransactionHandlerTest, simple_transaction_abort) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  request.setRequestType(arangodb::rest::RequestType::POST);
  request.addSuffix("begin");
  parser.parse("{ \"collections\":{\"read\": [\"42\"]}}");

  arangodb::RestStatus status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::CREATED, responce.responseCode());
  VPackSlice slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::CREATED) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               false == slice.get(arangodb::StaticStrings::Error).getBoolean()));

  EXPECT_TRUE(slice.hasKey("result"));
  std::string tid = slice.get("result").get("id").copyString();
  ASSERT_NE(std::stol(tid), 0);
  EXPECT_TRUE(slice.get("result").get("status").isEqualString("running"));

  // GET status
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.clearSuffixes();
  request.addSuffix(tid);

  status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::OK) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               false == slice.get(arangodb::StaticStrings::Error).getBoolean()));

  EXPECT_TRUE(slice.hasKey("result"));
  EXPECT_EQ(slice.get("result").get("id").copyString(), tid);
  EXPECT_TRUE(slice.get("result").get("status").isEqualString("running"));

  // DELETE abort trx

  request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

  status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::OK) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               false == slice.get(arangodb::StaticStrings::Error).getBoolean()));

  EXPECT_TRUE(slice.hasKey("result"));
  EXPECT_EQ(slice.get("result").get("id").copyString(), tid);
  EXPECT_TRUE(slice.get("result").get("status").isEqualString("aborted"));
}

TEST_F(RestTransactionHandlerTest, simple_transaction_and_commit) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  request.setRequestType(arangodb::rest::RequestType::POST);
  request.addSuffix("begin");
  parser.parse("{ \"collections\":{\"read\": [\"42\"]}}");

  arangodb::RestStatus status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::CREATED, responce.responseCode());
  VPackSlice slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::CREATED) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               false == slice.get(arangodb::StaticStrings::Error).getBoolean()));

  EXPECT_TRUE(slice.hasKey("result"));
  std::string tid = slice.get("result").get("id").copyString();
  ASSERT_NE(std::stol(tid), 0);
  EXPECT_TRUE(slice.get("result").get("status").isEqualString("running"));

  // PUT commit trx
  request.setRequestType(arangodb::rest::RequestType::PUT);
  request.clearSuffixes();
  request.addSuffix(tid);

  status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::OK) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               false == slice.get(arangodb::StaticStrings::Error).getBoolean()));

  EXPECT_TRUE(slice.hasKey("result"));
  EXPECT_EQ(slice.get("result").get("id").copyString(), tid);
  EXPECT_TRUE(slice.get("result").get("status").isEqualString("committed"));
}

TEST_F(RestTransactionHandlerTest, permission_denied_read_only) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Internal, "dummy",
                                "testVocbase", arangodb::auth::Level::RO,
                                arangodb::auth::Level::RO, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);

  request.setRequestType(arangodb::rest::RequestType::POST);
  request.addSuffix("begin");
  parser.parse("{ \"collections\":{\"write\": [\"42\"]}}");

  arangodb::RestStatus status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  VPackSlice slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::FORBIDDEN) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
               slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
               TRI_ERROR_ARANGO_READ_ONLY ==
                   slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
}

TEST_F(RestTransactionHandlerTest, permission_denied_forbidden) {
  std::shared_ptr<LogicalCollection> coll;
  {
    auto json =
        VPackParser::fromJson("{ \"name\": \"testCollection\", \"id\": 42 }");
    coll = vocbase.createCollection(json->slice());
  }
  ASSERT_NE(coll, nullptr);

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Internal, "dummy",
                                "testVocbase", arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);

  request.setRequestType(arangodb::rest::RequestType::POST);
  request.addSuffix("begin");
  parser.parse("{ \"collections\":{\"write\": [\"42\"]}}");

  arangodb::RestStatus status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  VPackSlice slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::FORBIDDEN) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
               slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
               TRI_ERROR_FORBIDDEN ==
                   slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
}
