////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "RestHandler/RestCursorHandler.h"
#include "RestServer/QueryRegistryFeature.h"

#include "IResearch/RestHandlerMock.h"
#include "Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::tests;

class QueryCursorTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    server = std::make_unique<mocks::MockRestAqlServer>();
  }
  static void TearDownTestCase() { server.reset(); }

 protected:
  static inline std::unique_ptr<mocks::MockRestAqlServer> server;
};

namespace {
arangodb::velocypack::SharedSlice operator"" _vpack(const char* json, size_t len) {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  options.validateUtf8Strings = true;
  VPackParser parser(&options);
  parser.parse(json, len);
  return parser.steal()->sharedSlice();
}
}  // namespace

TEST_F(QueryCursorTest, resultCursorResultArrayIndexSingleBatch) {
  auto& vocbase = server->getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(arangodb::rest::RequestType::POST);
  fakeRequest->_payload.add(R"json(
    {
      "query": "FOR i IN 1..1000 RETURN CONCAT('', i)"
    }
  )json"_vpack);

  auto* registry = arangodb::QueryRegistryFeature::registry();

  auto testee =
      std::make_shared<arangodb::RestCursorHandler>(server->server(),
                                                    fakeRequest.release(),
                                                    fakeResponse.release(), registry);

  testee->execute();

  fakeResponse.reset(dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));

  auto const responseBodySlice = fakeResponse->_payload.slice();

  ASSERT_TRUE(responseBodySlice.isObject());

  auto resultSlice = responseBodySlice.get("result").resolveExternal();
  ASSERT_FALSE(resultSlice.isNone());
  ASSERT_TRUE(resultSlice.isArray())
      << "Expected array, but got " << valueTypeName(resultSlice.type());
  // should be a compact array
  ASSERT_EQ(0x13, resultSlice.head());
}

TEST_F(QueryCursorTest, resultCursorResultArrayIndexTwoBatches) {
  auto& vocbase = server->getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(arangodb::rest::RequestType::POST);
  fakeRequest->_payload.add(R"json(
    {
      "query": "FOR i IN 1..2000 RETURN CONCAT('', i)"
    }
  )json"_vpack);

  auto* registry = arangodb::QueryRegistryFeature::registry();

  auto testee =
      std::make_shared<arangodb::RestCursorHandler>(server->server(),
                                                    fakeRequest.release(),
                                                    fakeResponse.release(), registry);

  testee->execute();

  fakeResponse.reset(dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));

  auto const responseBodySlice = fakeResponse->_payload.slice();

  ASSERT_TRUE(responseBodySlice.isObject());

  auto resultSlice = responseBodySlice.get("result").resolveExternal();
  ASSERT_FALSE(resultSlice.isNone());
  ASSERT_TRUE(resultSlice.isArray())
      << "Expected array, but got " << valueTypeName(resultSlice.type());
  // should be a compact array
  ASSERT_EQ(0x13, resultSlice.head());
}

TEST_F(QueryCursorTest, streamingCursorResultArrayIndexSingleBatch) {
  auto& vocbase = server->getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(arangodb::rest::RequestType::POST);
  fakeRequest->_payload.add(R"json(
    {
      "query": "FOR i IN 1..1000 RETURN CONCAT('', i)",
      "options": { "stream": true }
    }
  )json"_vpack);

  auto* registry = arangodb::QueryRegistryFeature::registry();

  auto testee =
      std::make_shared<arangodb::RestCursorHandler>(server->server(),
                                                    fakeRequest.release(),
                                                    fakeResponse.release(), registry);

  testee->execute();

  fakeResponse.reset(dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));

  auto const responseBodySlice = fakeResponse->_payload.slice();

  ASSERT_TRUE(responseBodySlice.isObject());

  auto const resultSlice = responseBodySlice.get("result").resolveExternal();
  ASSERT_FALSE(resultSlice.isNone());
  ASSERT_TRUE(resultSlice.isArray())
      << "Expected array, but got " << valueTypeName(resultSlice.type());
  // should be a compact array
  ASSERT_EQ(0x13, resultSlice.head());
}

TEST_F(QueryCursorTest, streamingCursorResultArrayIndexTwoBatches) {
  auto& vocbase = server->getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(arangodb::rest::RequestType::POST);
  fakeRequest->_payload.add(R"json(
    {
      "query": "FOR i IN 1..2000 RETURN CONCAT('', i)",
      "options": { "stream": true }
    }
  )json"_vpack);

  auto* registry = arangodb::QueryRegistryFeature::registry();

  auto testee =
      std::make_shared<arangodb::RestCursorHandler>(server->server(),
                                                    fakeRequest.release(),
                                                    fakeResponse.release(), registry);

  testee->execute();

  fakeResponse.reset(dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));
  // this is necessary to reset the wakeup handler, which otherwise holds a
  // shared_ptr to testee.
  testee->shutdownExecute(true);

  auto const responseBodySlice = fakeResponse->_payload.slice();

  {  // release the query, so the AQL feature doesn't wait on it during shutdown
    auto const idSlice = responseBodySlice.get("id");
    ASSERT_FALSE(idSlice.isNone());
    ASSERT_TRUE(idSlice.isString());
    auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
    fakeRequest->setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    fakeRequest->addSuffix(idSlice.copyString());
    auto fakeResponse = std::make_unique<GeneralResponseMock>();
    auto restHandler =
        std::make_shared<arangodb::RestCursorHandler>(server->server(),
                                                      fakeRequest.release(),
                                                      fakeResponse.release(), registry);
    restHandler->execute();
    fakeResponse.reset(
        dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));
  }

  testee.reset();

  ASSERT_TRUE(responseBodySlice.isObject());

  auto const resultSlice = responseBodySlice.get("result").resolveExternal();
  ASSERT_FALSE(resultSlice.isNone());
  ASSERT_TRUE(resultSlice.isArray())
      << "Expected array, but got " << valueTypeName(resultSlice.type());
  // should be a compact array
  ASSERT_EQ(0x13, resultSlice.head());
}
