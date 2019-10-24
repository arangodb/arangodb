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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
// #include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "velocypack/Iterator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

namespace {

class IIndexNodeTest
  : public ::testing::Test,
    public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  IIndexNodeTest() : server(false) {
  }

};  // IIndexNodeTest

arangodb::CreateDatabaseInfo createInfo(arangodb::application_features::ApplicationServer& server) {
  arangodb::CreateDatabaseInfo info(server);
  info.allowSystemDB(false);
  auto rv = info.load("testVocbase", 2);
  if (rv.fail()) {
    throw std::runtime_error(rv.errorMessage());
  }
  return info;
}

arangodb::aql::QueryResult executeQuery(TRI_vocbase_t& vocbase, std::string const& queryString,
                                        std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                                        std::string const& optionsString = "{}"
) {
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString), bindVars,
                             arangodb::velocypack::Parser::fromJson(optionsString),
                             arangodb::aql::PART_MAIN);

  std::shared_ptr<arangodb::aql::SharedQueryState> ss = query.sharedState();

  arangodb::aql::QueryResult result;
  while (true) {
    auto state = query.execute(arangodb::QueryRegistryFeature::registry(), result);
    if (state == arangodb::aql::ExecutionState::WAITING) {
      ss->waitForAsyncResponse();
    } else {
      break;
    }
  }
  return result;
}

TEST_F(IIndexNodeTest, constructCollection) {
  server.startFeatures();
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, createInfo(server.server()));
  // create collection
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testCollection\", \"id\" : 42 }");
  auto collection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE(collection);
  //auto indexJson = arangodb::velocypack::Parser::fromJson("{\"type\": \"hash\", \"fields\": [\"obj.a\", \"obj.b\", \"obj.c\"]}");
  auto indexJson = arangodb::velocypack::Parser::fromJson("{\"type\": \"hash\", \"fields\": [\"tags.hop[*].foo.fo\", \"tags.hop[*].bar.br\", \"tags.hop[*].baz.bz\"]}");
  bool createdIndex{};
  auto index = collection->createIndex(indexJson->slice(), createdIndex);
  ASSERT_TRUE(createdIndex);
  ASSERT_FALSE(!index);

  std::vector<std::string> const EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());
  EXPECT_TRUE(trx.begin().ok());

  arangodb::OperationOptions opt;
  arangodb::ManagedDocumentResult mmdoc;
  //auto jsonDocument = arangodb::velocypack::Parser::fromJson("{\"obj\" : {\"a\" : \"a_val\", \"b\" : \"b_val\", \"c\" : \"c_val\"}}");
  auto jsonDocument = arangodb::velocypack::Parser::fromJson("{\"tags\": {\"hop\" : [{\"foo\" : {\"fo\" : \"foo_val\"}, \"bar\": {\"br\" : \"bar_val\"}, \"baz\" : {\"bz\" : \"baz_val\"}}]}}");
  //auto jsonDocument = arangodb::velocypack::Parser::fromJson("{\"tags\": {\"hop\" : [{\"foo\" : {\"fo\" : \"foo_val\"}}, {\"bar\": {\"br\" : \"bar_val\"}}, {\"baz\" : {\"bz\" : \"baz_val\"}}]}}");
  auto const res = collection->insert(&trx, jsonDocument->slice(), mmdoc, opt, false);
  EXPECT_TRUE(res.ok());

  EXPECT_TRUE(trx.commit().ok());
  //auto queryString = "FOR d IN testCollection FILTER d.obj.b == 'b_val' SORT d.obj.b LIMIT 10 RETURN d";
  auto queryString = "FOR d IN testCollection FILTER 'foo_val' IN d.tags.hop[*].foo.fo SORT d.tags.hop[*].baz.bz LIMIT 10 RETURN d";
  auto queryResult = ::executeQuery(vocbase, queryString);
  EXPECT_TRUE(queryResult.result.ok()); // commit
  if (queryResult.result.ok()) {
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());
    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(1, resultIt.size());
  }
}
}
