////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/fuerte.h>
#include <fuerte/loop.h>
#include <fuerte/helper.h>
#include <thread>
#include <velocypack/velocypack-aliases.h>


#include "gtest/gtest.h"
#include "connection_test.h"

// Tesuite checks the thread-safety properties of the connection
// implementations. Try to send requests on the same connection object
// concurrently

// need a new class to use test fixture
class ConcurrentConnectionF : public ConnectionTestF {

  virtual void SetUp() override {
    ConnectionTestF::SetUp();
    dropCollection("concurrent"); // ignore response
    ASSERT_EQ(createCollection("concurrent"), fu::StatusOK);
  }
  
  virtual void TearDown() override {
    // drop the collection
    ASSERT_EQ(dropCollection("concurrent"), fu::StatusOK);
    ConnectionTestF::TearDown();
  }
};

TEST_P(ConcurrentConnectionF, ApiVersionParallel) {
  fu::WaitGroup wg;
  std::atomic<size_t> counter(0);
  auto cb = [&](fu::Error error, std::unique_ptr<fu::Request> req, std::unique_ptr<fu::Response> res) {
    fu::WaitGroupDone done(wg);
    if (error != fu::Error::NoError) {
      ASSERT_TRUE(false) << fu::to_string(error);
    } else {
      ASSERT_EQ(res->statusCode(), fu::StatusOK);
      auto slice = res->slices().front();
      auto version = slice.get("version").copyString();
      auto server = slice.get("server").copyString();
      ASSERT_EQ(server, "arango");
      ASSERT_EQ(version[0], _major_arango_version);
      counter.fetch_add(1, std::memory_order_relaxed);
    }
  };
  
  std::vector<std::shared_ptr<fu::Connection>> connections;
  connections.push_back(_connection);
  for (size_t i = 1; i < threads(); i++) {
    connections.push_back(createConnection());
  }
  
  std::vector<std::thread> joins;
  for (size_t t = 0; t < threads(); t++) {
    const size_t rep = repeat();
    wg.add((unsigned)rep);
    joins.emplace_back([=]{
      for (size_t i = 0; i < rep; i++) {
        auto request = fu::createRequest(fu::RestVerb::Get, "/_api/version");
        auto& conn = *connections[i % connections.size()];
        while (conn.requestsLeft() >= 24) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        conn.sendRequest(std::move(request), cb);
      }
    });
  }
  ASSERT_TRUE(wg.wait_for(std::chrono::seconds(300))); // wait for all threads to return

  // wait for all threads to end
  for (std::thread& t : joins) {
    t.join();
  }
  
  ASSERT_EQ(repeat() * threads(), counter);
}

TEST_P(ConcurrentConnectionF, CreateDocumentsParallel){
  
  std::atomic<size_t> counter(0);
  fu::WaitGroup wg;
  auto cb = [&](fu::Error error, std::unique_ptr<fu::Request> req,
                std::unique_ptr<fu::Response> res) {
    counter.fetch_add(1, std::memory_order_relaxed);
    fu::WaitGroupDone done(wg);
    if (error != fu::Error::NoError) {
      ASSERT_TRUE(false) << fu::to_string(error);
    } else {
      ASSERT_EQ(res->statusCode(), fu::StatusAccepted);
      auto slice = res->slices().front();
      ASSERT_TRUE(slice.get("_id").isString());
      ASSERT_TRUE(slice.get("_key").isString());
      ASSERT_TRUE(slice.get("_rev").isString());
    }
  };
  
  VPackBuilder builder;
  builder.openObject();
  builder.add("hello", VPackValue("world"));
  builder.close();
  
  std::vector<std::shared_ptr<fu::Connection>> connections;
  connections.push_back(_connection);
  for (size_t i = 1; i < threads(); i++) {
    connections.push_back(createConnection());
  }
  
  std::vector<std::thread> joins;
  for (size_t t = 0; t < threads(); t++) {
    wg.add(repeat());
    joins.emplace_back([=]{
      for (size_t i = 0; i < repeat(); i++) {
        auto request = fu::createRequest(fu::RestVerb::Post, "/_api/document/concurrent");
        request->addVPack(builder.slice());
        auto& conn = *connections[i % connections.size()];
        while (conn.requestsLeft() >= 24) {
           std::this_thread::sleep_for(std::chrono::milliseconds(50));
         }
        conn.sendRequest(std::move(request), cb);
      }
    });
  }
  ASSERT_TRUE(wg.wait_for(std::chrono::seconds(300))); // wait for all threads to return

  // wait for all threads to end
  for (std::thread& t : joins) {
    t.join();
  }
  
  ASSERT_EQ(repeat() * threads(), counter);
}

static const ConnectionTestParams params[] = {
  {/*._protocol=*/fu::ProtocolType::Http,/* ._threads=*/2, /*._repeat=*/500},
  {/*._protocol=*/fu::ProtocolType::Http2,/* ._threads=*/2, /*._repeat=*/500},
  {/*._protocol=*/fu::ProtocolType::Vst,/* ._threads=*/2, /*._repeat=*/500},
  {/*._protocol=*/fu::ProtocolType::Http,/* ._threads=*/4, /*._repeat=*/5000},
  {/*._protocol=*/fu::ProtocolType::Http2,/* ._threads=*/4, /*._repeat=*/5000},
  {/*._protocol=*/fu::ProtocolType::Vst,/* ._threads=*/4, /*._repeat=*/5000},
};

INSTANTIATE_TEST_CASE_P(ConcurrentRequestsTests, ConcurrentConnectionF,
  ::testing::ValuesIn(params));

