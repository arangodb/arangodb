////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Agency/Store.h"
#include "Agency/AgencyComm.h"

TEST(StoreTest, store_preconditions) {
  using namespace arangodb::consensus;

  Node node("node");
  VPackBuilder foo, baz, pi;
  foo.add(VPackValue("bar"));
  baz.add(VPackValue(13));
  pi.add(VPackValue(3.14159265359));
  auto fooNode = std::make_shared<Node>("foo");
  auto bazNode = std::make_shared<Node>("baz");
  auto piNode = std::make_shared<Node>("pi");
  *fooNode = foo.slice();
  *bazNode = baz.slice();
  *piNode = pi.slice();

  node.addChild("foo", fooNode);
  node.addChild("baz", bazNode);
  node.addChild("pi", piNode);
  node.addChild("foo1", fooNode);
  node.addChild("baz1", bazNode);
  node.addChild("pi1", piNode);

  VPackOptions opts;
  opts.buildUnindexedObjects = true;
  VPackBuilder other(&opts);
  {
    VPackObjectBuilder o(&other);
    other.add("pi1", VPackValue(3.14159265359));
    other.add("foo", VPackValue("bar"));
    other.add("pi", VPackValue(3.14159265359));
    other.add("baz1", VPackValue(13));
    other.add("foo1", VPackValue("bar"));
    other.add("baz", VPackValue(13));
  }

  ASSERT_EQ(node, other.slice());
}

TEST(StoreTest, store_split) {
  using namespace arangodb::consensus;

  ASSERT_EQ(std::vector<std::string>(), Store::split(""));
  ASSERT_EQ(std::vector<std::string>(), Store::split("/"));
  ASSERT_EQ(std::vector<std::string>(), Store::split("//"));
  ASSERT_EQ(std::vector<std::string>(), Store::split("///"));
  
  ASSERT_EQ((std::vector<std::string>{"a"}), Store::split("a"));
  ASSERT_EQ((std::vector<std::string>{"a c"}), Store::split("a c"));
  ASSERT_EQ((std::vector<std::string>{"foobar"}), Store::split("foobar"));
  ASSERT_EQ((std::vector<std::string>{"foo bar"}), Store::split("foo bar"));
  
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("a/b/c"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("/a/b/c"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("/a/b/c/"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("//a/b/c"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("//a/b/c/"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("a/b/c//"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("//a/b/c//"));
  ASSERT_EQ((std::vector<std::string>{"a", "b", "c"}), Store::split("//a/b/c//"));
  ASSERT_EQ((std::vector<std::string>{"a"}), Store::split("//////a"));
  ASSERT_EQ((std::vector<std::string>{"a"}), Store::split("a//////////////"));
  ASSERT_EQ((std::vector<std::string>{"a"}), Store::split("/////////////a//////////////"));
  ASSERT_EQ((std::vector<std::string>{"foobar"}), Store::split("//////foobar"));
  ASSERT_EQ((std::vector<std::string>{"foobar"}), Store::split("foobar//////////////"));
  ASSERT_EQ((std::vector<std::string>{"foobar"}), Store::split("/////////////foobar//////////////"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("a/c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("a//c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("a///c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a//c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a//c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a///c"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a//c/"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a//c//"));
  ASSERT_EQ((std::vector<std::string>{"a", "c"}), Store::split("/a///c//"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("foo/bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("foo//bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("foo///bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo//bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo//bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo///bar"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo//bar/"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo//bar//"));
  ASSERT_EQ((std::vector<std::string>{"foo", "bar"}), Store::split("/foo///bar//"));
  
  ASSERT_EQ((std::vector<std::string>{"foo", "bar", "baz"}), Store::split("/foo///bar//baz"));
}

TEST(StoreTest, store_normalize) {
  using namespace arangodb::consensus;

  EXPECT_EQ("/", Store::normalize(""));
  EXPECT_EQ("/", Store::normalize("/"));
  EXPECT_EQ("/", Store::normalize("//"));
  EXPECT_EQ("/", Store::normalize("////"));
  EXPECT_EQ("/a", Store::normalize("a"));
  EXPECT_EQ("/a", Store::normalize("/a"));
  EXPECT_EQ("/a", Store::normalize("/a/"));
  EXPECT_EQ("/a", Store::normalize("//a/"));
  EXPECT_EQ("/a", Store::normalize("//a//"));
  EXPECT_EQ("/a/b", Store::normalize("a/b"));
  EXPECT_EQ("/a/b", Store::normalize("a/b/"));
  EXPECT_EQ("/a/b", Store::normalize("/a/b"));
  EXPECT_EQ("/a/b", Store::normalize("//a/b"));
  EXPECT_EQ("/a/b", Store::normalize("/a//b"));
  EXPECT_EQ("/a/b", Store::normalize("/a/b/"));
  EXPECT_EQ("/a/b", Store::normalize("/a/b//"));
  EXPECT_EQ("/a/b", Store::normalize("//a//b//"));
  EXPECT_EQ("/a/b/c", Store::normalize("a/b/c"));
  EXPECT_EQ("/a/b/c", Store::normalize("a/b/c/"));
  EXPECT_EQ("/a/b/c", Store::normalize("/a/b/c"));
  EXPECT_EQ("/a/b/c", Store::normalize("a//b/c"));
  EXPECT_EQ("/a/b/c", Store::normalize("a/b//c"));
  EXPECT_EQ("/mutter", Store::normalize("mutter"));
  EXPECT_EQ("/mutter", Store::normalize("/mutter"));
  EXPECT_EQ("/mutter", Store::normalize("//mutter"));
  EXPECT_EQ("/mutter", Store::normalize("mutter/"));
  EXPECT_EQ("/mutter", Store::normalize("mutter//"));
  EXPECT_EQ("/mutter", Store::normalize("/mutter//"));
  EXPECT_EQ("/mutter", Store::normalize("//mutter//"));
  EXPECT_EQ("/der/hund", Store::normalize("der/hund"));
  EXPECT_EQ("/der/hund", Store::normalize("/der/hund"));
  EXPECT_EQ("/der/hund", Store::normalize("der/hund/"));
  EXPECT_EQ("/der/hund", Store::normalize("/der/hund/"));
  EXPECT_EQ("/der/hund", Store::normalize("der/////hund"));
  EXPECT_EQ("/der/hund", Store::normalize("der/hund/////"));
  EXPECT_EQ("/der/hund", Store::normalize("////der/hund"));
  EXPECT_EQ("/der/hund/der/schwitzt", Store::normalize("der/hund/der/schwitzt"));
  EXPECT_EQ("/der/hund/der/schwitzt", Store::normalize("der/hund/der/schwitzt/"));
  EXPECT_EQ("/der/hund/der/schwitzt", Store::normalize("/der/hund/der/schwitzt/"));
}

  // arangodb::AgencyComm comm;
  // arangodb::AgencyReadTransaction readTransaction("/x");
  // comm.sendTransactionWithFailover(readTransaction);

std::string const agencyLeader {"http://agencyLeader"};

struct SimpleJson {
  SimpleJson(std::string name, SimpleJson value) {}
  SimpleJson(const char * value) {}
  SimpleJson(double value) {}
  SimpleJson() {}

  bool operator==(SimpleJson const &other) const {
    return true;
  }
};

struct AgencyResult {
  int statusCode {200};
  std::vector<SimpleJson> bodyParsed;
};

AgencyResult accessAgency(std::string const &op, std::vector<SimpleJson> const &list, double timeout = 2.0) {
  return {};
}

std::vector<SimpleJson> readAndCheck(std::vector<SimpleJson> const &list) {
  auto res = accessAgency("read", list);
  if (res.statusCode != 200) {
    throw std::runtime_error("Unexpected read failure");
  }
  return res.bodyParsed;
}

std::vector<SimpleJson> writeAndCheck(std::vector<SimpleJson> const &list, double timeout = 60) {
  auto res = accessAgency("write", list, timeout);
  if (res.statusCode != 200) {
    throw std::runtime_error("Unexpected write failure");
  }
  return res.bodyParsed;
}

#define ASSERT_EQ_J(x, y) ASSERT_EQ(x, std::vector<SimpleJson> y)

AgencyResult request(std::string url, std::string method, bool followRedirect = false) {
  return {};
}

/*
////////////////////////////////////////////////////////////////////////////////
/// @brief Test transact interface
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, poll) {
      auto cur = accessAgency("write",{{{"/", {"op", "delete"}}}});

      auto ret = request(agencyLeader + "/_api/agency/poll",
                         "GET", true);
      ASSERT_EQ(ret.statusCode, 200);
      ret.bodyParsed = JSON.parse(ret.body);
      ASSERT_TRUE(ret.bodyParsed.hasOwnProperty("result"));
      auto result = ret.bodyParsed.result;
      ASSERT_TRUE(result.hasOwnProperty("readDB"));
      ASSERT_EQ(result.readDB, {});
      ASSERT_TRUE(result.hasOwnProperty("commitIndex"));
      auto ci = result.commitIndex;
      ASSERT_TRUE(result.hasOwnProperty("firstIndex"));
      ASSERT_EQ(result.firstIndex, 0);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=0",
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      ASSERT_TRUE(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      ASSERT_TRUE(result.hasOwnProperty("readDB"));
      ASSERT_EQ(result.readDB, {});
      ASSERT_TRUE(result.hasOwnProperty("commitIndex"));
      ASSERT_EQ(result.commitIndex, ci);
      ASSERT_TRUE(result.hasOwnProperty("firstIndex"));
      ASSERT_EQ(result.firstIndex, 0);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=1",
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      ASSERT_TRUE(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      ASSERT_TRUE(result.hasOwnProperty("log"));
      auto log = result.log;
      ASSERT_TRUE(Array.isArray(log));
      ASSERT_EQ(result.commitIndex, ci);
      ASSERT_EQ(result.firstIndex, 1);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=1",
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      ASSERT_TRUE(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      ASSERT_TRUE(result.hasOwnProperty("log"));
      log = result.log;
      ASSERT_TRUE(Array.isArray(log));
      ASSERT_EQ(result.commitIndex, ci);
      ASSERT_EQ(result.firstIndex, 1);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=" + ci,
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      ASSERT_TRUE(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      ASSERT_TRUE(result.hasOwnProperty("log"));
      log = result.log;
      ASSERT_EQ(log.length, 1);
      ASSERT_TRUE(Array.isArray(log));
      ASSERT_EQ(result.commitIndex, ci);
      ASSERT_EQ(result.firstIndex, ci);


      ret = request({url: agencyLeader + "/_api/agency/poll?index=" + ci,
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      ASSERT_TRUE(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      ASSERT_TRUE(result.hasOwnProperty("log"));
      log = result.log;
      ASSERT_EQ(log.length, 1);
      ASSERT_TRUE(Array.isArray(log));
      ASSERT_EQ(result.commitIndex, ci);
      ASSERT_EQ(result.firstIndex, ci);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=" + (ci + 1),
                     method: "GET", headers: {"X-arango-async": "store"}});

      ASSERT_EQ(ret.statusCode, 202);
      ASSERT_TRUE(ret.headers.hasOwnProperty("x-arango-async-id"));
      auto job = ret.headers["x-arango-async-id"];

      auto wr = accessAgency("write",[[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];
      auto tries = 0;
      while (++tries <60) {
        // jobs are processed asynchronously, so we need to poll here
        // until the job has been processed
        ret = request({url: agencyLeader + "/_api/job/" + job,
                       method: "GET", followRedirect: true});
        if (ret.statusCode !== 204) {
          break;
        }
        wait(0.5);
      }
      ASSERT_EQ(ret.statusCode, 200);
      ret = request({url: agencyLeader + "/_api/job/" + job, method: "PUT"});
      ASSERT_EQ(ret.statusCode, 200);
      ret.bodyParsed = JSON.parse(ret.body);
      result = ret.bodyParsed.result;

      ASSERT_TRUE(result.hasOwnProperty("log"));
      ASSERT_TRUE(result.hasOwnProperty("commitIndex"));
      ASSERT_TRUE(result.hasOwnProperty("firstIndex"));
      ASSERT_EQ(result.firstIndex, ci+1);
      ASSERT_EQ(result.commitIndex, ci+1);

      // Multiple writes
      ci = result.commitIndex;
      ret = request({url: agencyLeader + "/_api/agency/poll?index=" + (ci + 1),
                     method: "GET", followRedirect: true, headers: {"X-arango-async": "store"}});
      ASSERT_TRUE(ret.headers.hasOwnProperty("x-arango-async-id"));
      job = ret.headers["x-arango-async-id"];
      wr = accessAgency("write",[[{"/": {"op":"delete"}}],[{"/": {"op":"delete"}}],[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];
      ret = request({url: agencyLeader + "/_api/job/" + job,
                     method: "PUT", followRedirect: true, body: {}});
      ASSERT_EQ(ret.statusCode, 200);
      ret.bodyParsed = JSON.parse(ret.body);
      result = ret.bodyParsed.result;
      ASSERT_EQ(result.firstIndex, ci+1);
      ASSERT_EQ(result.commitIndex, ci+3);
      ci = result.commitIndex;
    }
////////////////////////////////////////////////////////////////////////////////
/// @brief Test transact interface
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, transact) {

      auto cur = accessAgency("write",[[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];
      ASSERT_EQ(readAndCheck([["/x"]]), [{}]);
      auto res = transactAndCheck([["/x"],[{"/x":12}]],200);
      ASSERT_EQ(res, [{},++cur]);
      res = transactAndCheck([["/x"],[{"/x":12}]],200);
      ASSERT_EQ(res, [{x:12},++cur]);
      res = transactAndCheck([["/x"],[{"/x":12}],["/x"]],200);
      ASSERT_EQ(res, [{x:12},++cur,{x:12}]);
      res = transactAndCheck([["/x"],[{"/x":12}],["/x"]],200);
      ASSERT_EQ(res, [{x:12},++cur,{x:12}]);
      res = transactAndCheck([["/x"],[{"/x":{"op":"increment"}}],["/x"]],200);
      ASSERT_EQ(res, [{x:12},++cur,{x:13}]);
      res = transactAndCheck(
        [["/x"],[{"/x":{"op":"increment"}}],["/x"],[{"/x":{"op":"increment"}}]],
        200);
      ASSERT_EQ(res, [{x:13},++cur,{x:14},++cur]);
      res = transactAndCheck(
        [[{"/x":{"op":"increment"}}],[{"/x":{"op":"increment"}}],["/x"]],200);
      ASSERT_EQ(res, [++cur,++cur,{x:17}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
    }


*/
TEST(StoreTest, test_infrastructure) {
  ASSERT_EQ(std::vector<SimpleJson>{},            std::vector<SimpleJson>());
  ASSERT_EQ(std::vector<SimpleJson>{{}},          std::vector<SimpleJson>{{}});
  {
    std::vector<SimpleJson> v1{{"A", 123}};
    std::vector<SimpleJson> v2{{"A", 123}};
    ASSERT_EQ(v1,  v2);
  }
  {
    std::vector<SimpleJson> v1{{{"A", 123}, {"B", 456}}};
    std::vector<SimpleJson> v2{{{"A", 123}, {"B", 456}}};
    ASSERT_EQ(v1,  v2);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single top level key
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, single_top_level) {
      ASSERT_EQ(readAndCheck({"/x"}), std::vector<SimpleJson>());
      writeAndCheck({{"x",12}});
      ASSERT_EQ(readAndCheck({{"/x"}}),  (std::vector<SimpleJson>{{"x", 12}}));
      writeAndCheck({{"x",SimpleJson{"op","delete"}}});
      ASSERT_EQ((readAndCheck({{"/x"}})), std::vector<SimpleJson>());
    }

/*
////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single non-top level key
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, single_non_top_level) {
      ASSERT_EQ(readAndCheck([["/x/y"]]), [{}]);
      writeAndCheck([[{"x/y":12}]]);
      ASSERT_EQ(readAndCheck([["/x/y"]]), [{x:{y:12}}]);
      writeAndCheck([[{"x/y":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/x"]]), [{x:{}}]);
      writeAndCheck([[{"x":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/x"]]), [{}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief test preconditions
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, precondition) {
      writeAndCheck([[{"/a":12}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:12}]);
      writeAndCheck([[{"/a":13},{"/a":12}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:13}]);
      auto res = accessAgency("write", [[{"/a":14},{"/a":12}]]); // fail precond {a:12}
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(res.bodyParsed, {"results":[0]});
      writeAndCheck([[{a:{op:"delete"}}]]);
      // fail precond oldEmpty
      res = accessAgency("write",[[{"a":14},{"a":{"oldEmpty":false}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(res.bodyParsed, {"results":[0]});
      writeAndCheck([[{"a":14},{"a":{"oldEmpty":true}}]]); // precond oldEmpty
      writeAndCheck([[{"a":14},{"a":{"old":14}}]]);        // precond old
      // fail precond old
      res = accessAgency("write",[[{"a":14},{"a":{"old":13}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(res.bodyParsed, {"results":[0]});
      writeAndCheck([[{"a":14},{"a":{"isArray":false}}]]); // precond isArray
      // fail precond isArray
      res = accessAgency("write",[[{"a":14},{"a":{"isArray":true}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(res.bodyParsed, {"results":[0]});
      // check object precondition
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":12}}]]);
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":13}},{"a":{"old":{"b":{"c":12}}}}]]);
      ASSERT_EQ(res.statusCode, 200);
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":14}},{"/a":{"old":{"b":{"c":12}}}}]]);
      ASSERT_EQ(res.statusCode, 412);
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":14}},{"/a":{"old":{"b":{"c":13}}}}]]);
      ASSERT_EQ(res.statusCode, 200);
      // multiple preconditions
      res = accessAgency("write",[[{"/a":1,"/b":true,"/c":"c"},{"/a":{"oldEmpty":false}}]]);
      ASSERT_EQ(readAndCheck([["/a","/b","c"]]), [{a:1,b:true,c:"c"}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":true}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:1}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"oldEmpty":true},"/b":{"oldEmpty":false}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:1}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":false},"/c":{"oldEmpty":true}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:1}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      ASSERT_EQ(res.statusCode, 200);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:2}]);
      res = accessAgency("write",[[{"/a":3},{"/a":{"old":2},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      ASSERT_EQ(res.statusCode, 200);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:3}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"old":2},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:3}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"old":3},"/b":{"oldEmpty":false},"/c":{"isArray":true}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:3}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"old":3},"/b":{"oldEmpty":false},"/c":{"isArray":false}}]]);
      ASSERT_EQ(res.statusCode, 200);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:2}]);
      // in precondition & multiple
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":[1,2]},"d":false}]]);
      res = accessAgency("write",[[{"/b":2},{"/a/b/c":{"in":3}}]]);
      ASSERT_EQ(res.statusCode, 200);
      ASSERT_EQ(readAndCheck([["/b"]]), [{b:2}]);
      res = accessAgency("write",[[{"/b":3},{"/a/e":{"in":3}}]]);
      ASSERT_EQ(res.statusCode, 412);
      ASSERT_EQ(readAndCheck([["/b"]]), [{b:2}]);
      res = accessAgency("write",[[{"/b":3},{"/a/e":{"in":3},"/a/b/c":{"in":3}}]]);
      ASSERT_EQ(res.statusCode, 412);
      res = accessAgency("write",[[{"/b":3},{"/a/e":{"in":3},"/a/b/c":{"in":3}}]]);
      ASSERT_EQ(res.statusCode, 412);
      res = accessAgency("write",[[{"/b":3},{"/a/b/c":{"in":3},"/a/e":{"in":3}}]]);
      ASSERT_EQ(res.statusCode, 412);
      res = accessAgency("write",[[{"/b":3},{"/a/b/c":{"in":3},"/a/e":{"in":2}}]]);
      ASSERT_EQ(res.statusCode, 200);
      ASSERT_EQ(readAndCheck([["/b"]]), [{b:3}]);
      // Permute order of keys and objects within precondition
      SimpleJson localObj =
          {"foo" : "bar",
           "baz" : {
             "_id": "5a00203e4b660989b2ae5493", "index": 0,
             "guid": "7a709cc2-1479-4079-a0a3-009cbe5674f4",
             "isActive": true, "balance": "$3,072.23",
             "picture": "http://placehold.it/32x32",
             "age": 21, "eyeColor": "green", "name":
             { "first": "Durham", "last": "Duke" },
             "tags": ["anim","et","id","do","est",1.0,-1024,1024]
           },
           "qux" : ["3.14159265359",3.14159265359]
          };
      auto test;
      auto localKeys = [];
      for (auto i in localObj.baz) {
        localKeys.push(i);
      }
      auto permuted;
      res = accessAgency(
        "write",
        [[localObj,
          {"foo":localObj.bar,
           "baz":{"old":localObj.baz},
           "qux":localObj.qux}]]);
      ASSERT_EQ(res.statusCode, 412);

      res = writeAndCheck([[localObj]]);
      res = writeAndCheck([[localObj, {"foo":localObj.foo,"baz":{"old":localObj.baz},"qux":localObj.qux}]]);
      res = writeAndCheck(
        [[localObj, {"baz":{"old":localObj.baz},"foo":localObj.foo,"qux":localObj.qux}]]);
      res = writeAndCheck(
        [[localObj, {"baz":{"old":localObj.baz},"qux":localObj.qux,"foo":localObj.foo}]]);
      res = writeAndCheck(
        [[localObj, {"qux":localObj.qux,"baz":{"old":localObj.baz},"foo":localObj.foo}]]);

      for (auto j in localKeys) {
        permuted = {};
        shuffle(localKeys);
        for (auto k in localKeys) {
          permuted[localKeys[k]] = localObj.baz[localKeys[k]];
        }
        res = writeAndCheck(
          [[localObj, {"baz":{"old":permuted},"foo":localObj.foo,"qux":localObj.qux}]]);
        res = writeAndCheck(
          [[localObj, {"foo":localObj.foo,"qux":localObj.qux,"baz":{"old":permuted}}]]);
        res = writeAndCheck(
          [[localObj, {"qux":localObj.qux,"baz":{"old":permuted},"foo":localObj.foo}]]);
      }

      // Permute order of keys and objects within arrays in preconditions
      writeAndCheck([[{"a":[{"b":12,"c":13}]}]]);
      writeAndCheck([[{"a":[{"b":12,"c":13}]},{"a":[{"b":12,"c":13}]}]]);
      writeAndCheck([[{"a":[{"b":12,"c":13}]},{"a":[{"c":13,"b":12}]}]]);

      localObj = {"b":"Hello world!", "c":3.14159265359, "d":314159265359, "e": -3};
      auto localObk = {"b":1, "c":1.0, "d": 100000000001, "e": -1};
      localKeys  = [];
      for (auto l in localObj) {
        localKeys.push(l);
      }
      permuted = {};
      auto per2 = {};
      writeAndCheck([[ { "a" : [localObj,localObk] } ]]);
      writeAndCheck([[ { "a" : [localObj,localObk] }, {"a" : [localObj,localObk] }]]);
      for (auto m = 0; m < 7; m++) {
        permuted = {};
        shuffle(localKeys);
        for (k in localKeys) {
          permuted[localKeys[k]] = localObj[localKeys[k]];
          per2 [localKeys[k]] = localObk[localKeys[k]];
        }
        writeAndCheck([[ { "a" : [localObj,localObk] }, {"a" : [permuted,per2] }]]);
        res = accessAgency("write",
                           [[ { "a" : [localObj,localObk] }, {"a" : [per2,permuted] }]]);
        ASSERT_EQ(res.statusCode, 412);
      }

      res = accessAgency("write", [[{"a":12},{"a":{"intersectionEmpty":""}}]]);
      ASSERT_EQ(res.statusCode, 412);
      res = accessAgency("write", [[{"a":12},{"a":{"intersectionEmpty":[]}}]]);
      ASSERT_EQ(res.statusCode, 200);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":[]}}]]);
      ASSERT_EQ(res.statusCode, 200);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":[false,"Pi"]}}]]);
      ASSERT_EQ(res.statusCode, 412);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":["Pi",false]}}]]);
      ASSERT_EQ(res.statusCode, 412);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":[false,false,false]}}]]);
      ASSERT_EQ(res.statusCode, 412);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":["pi",3.1415926535]}}]]);
      ASSERT_EQ(res.statusCode, 200);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"instersectionEmpty":[]}}]]);
      ASSERT_EQ(res.statusCode, 412);

    }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief test clientIds
  ////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, client_ids) {
      auto res;
      auto cur;

      res = accessAgency("write", [[{"a":12}]]).bodyParsed;
      cur = res.results[0];

      writeAndCheck([[{"/a":12}]]);
      auto id = [guid(),guid(),guid(),guid(),guid(),guid(),
                guid(),guid(),guid(),guid(),guid(),guid(),
                guid(),guid(),guid()];
      auto query = [{"a":12},{"a":13},{"a":13}];
      auto pre = [{},{"a":12},{"a":12}];
      cur += 2;

      auto wres = accessAgency("write", [[query[0], pre[0], id[0]]]);
      res = accessAgency("inquire",[id[0]]);
      wres.bodyParsed.inquired = true;
      ASSERT_EQ(res.bodyParsed.results, wres.bodyParsed.results);

      wres = accessAgency("write", [[query[1], pre[1], id[0]]]);
      res = accessAgency("inquire",[id[0]]);
      ASSERT_EQ(res.bodyParsed.results, wres.bodyParsed.results);
      cur++;

      wres = accessAgency("write",[[query[1], pre[1], id[2]]]);
      ASSERT_EQ(wres.statusCode,412);
      res = accessAgency("inquire",[id[2]]);
      ASSERT_EQ(res.statusCode,404);
      ASSERT_EQ(res.bodyParsed, {"results":[0],"inquired":true});
      ASSERT_EQ(res.bodyParsed.results, wres.bodyParsed.results);

      wres = accessAgency("write",[[query[0], pre[0], id[3]],
                                   [query[1], pre[1], id[3]]]);
      ASSERT_EQ(wres.statusCode,200);
      cur += 2;
      res = accessAgency("inquire",[id[3]]);
      ASSERT_EQ(res.bodyParsed, {"results":[cur],"inquired":true});
      ASSERT_EQ(res.bodyParsed.results[0], wres.bodyParsed.results[1]);
      ASSERT_EQ(res.statusCode,200);


      wres = accessAgency("write",[[query[0], pre[0], id[4]],
                                   [query[1], pre[1], id[4]],
                                   [query[2], pre[2], id[4]]]);
      ASSERT_EQ(wres.statusCode,412);
      cur += 2;
      res = accessAgency("inquire",[id[4]]);
      ASSERT_EQ(res.bodyParsed, {"results":[cur],"inquired":true});
      ASSERT_EQ(res.bodyParsed.results[0], wres.bodyParsed.results[1]);
      ASSERT_EQ(res.statusCode,200);

      wres = accessAgency("write",[[query[0], pre[0], id[5]],
                                   [query[2], pre[2], id[5]],
                                   [query[1], pre[1], id[5]]]);
      ASSERT_EQ(wres.statusCode,412);
      cur += 2;
      res = accessAgency("inquire",[id[5]]);
      ASSERT_EQ(res.bodyParsed, {"results":[cur],"inquired":true});
      ASSERT_EQ(res.bodyParsed.results[0], wres.bodyParsed.results[1]);
      ASSERT_EQ(res.statusCode,200);

      wres = accessAgency("write",[[query[2], pre[2], id[6]],
                                   [query[0], pre[0], id[6]],
                                   [query[1], pre[1], id[6]]]);
      ASSERT_EQ(wres.statusCode,412);
      cur += 2;
      res = accessAgency("inquire",[id[6]]);
      ASSERT_EQ(res.bodyParsed, {"results":[cur],"inquired":true});
      ASSERT_EQ(res.bodyParsed.results[0], wres.bodyParsed.results[2]);
      ASSERT_EQ(res.statusCode,200);

      wres = accessAgency("write",[[query[2], pre[2], id[7]],
                                  [query[0], pre[0], id[8]],
                                  [query[1], pre[1], id[9]]]);
      ASSERT_EQ(wres.statusCode,412);
      cur += 2;
      res = accessAgency("inquire",[id[7],id[8],id[9]]);
      ASSERT_EQ(res.statusCode,404);
      ASSERT_EQ(res.bodyParsed.results, wres.bodyParsed.results);

    }

////////////////////////////////////////////////////////////////////////////////
/// @brief test document/transaction assignment
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, document) {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      ASSERT_EQ(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"a":{"_id":"576d1b7becb6374e24ed5a04","index":0,"guid":"60ffa50e-0211-4c60-a305-dcc8063ae2a5","isActive":true,"balance":"$1,050.96","picture":"http://placehold.it/32x32","age":30,"eyeColor":"green","name":{"first":"Maura","last":"Rogers"},"company":"GENESYNK","email":"maura.rogers@genesynk.net","phone":"+1(804)424-2766","address":"501RiverStreet,Wollochet,Vermont,6410","about":"Temporsintofficiaipsumidnullalaboreminimlaborisinlaborumincididuntexcepteurdolore.Sunteumagnadolaborumsunteaquisipsumaliquaaliquamagnaminim.Cupidatatadproidentullamconisietofficianisivelitculpaexcepteurqui.Suntautemollitconsecteturnulla.Commodoquisidmagnaestsitelitconsequatdoloreupariaturaliquaetid.","registered":"Friday,November28,20148:01AM","latitude":"-30.093679","longitude":"10.469577","tags":["laborum","proident","est","veniam","sunt"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"CarverDurham"},{"id":1,"name":"DanielleMalone"},{"id":2,"name":"ViolaBell"}],"greeting":"Hello,Maura!Youhave9unreadmessages.","favoriteFruit":"banana"}}],[{"!!@#$%^&*)":{"_id":"576d1b7bb2c1af32dd964c22","index":1,"guid":"e6bda5a9-54e3-48ea-afd7-54915fec48c2","isActive":false,"balance":"$2,631.75","picture":"http://placehold.it/32x32","age":40,"eyeColor":"blue","name":{"first":"Jolene","last":"Todd"},"company":"QUANTASIS","email":"jolene.todd@quantasis.us","phone":"+1(954)418-2311","address":"818ButlerStreet,Berwind,Colorado,2490","about":"Commodoesseveniamadestirureutaliquipduistempor.Auteeuametsuntessenisidolorfugiatcupidatatsintnulla.Sitanimincididuntelitculpasunt.","registered":"Thursday,June12,201412:08AM","latitude":"-7.101063","longitude":"4.105685","tags":["ea","est","sunt","proident","pariatur"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"SwansonMcpherson"},{"id":1,"name":"YoungTyson"},{"id":2,"name":"HinesSandoval"}],"greeting":"Hello,Jolene!Youhave5unreadmessages.","favoriteFruit":"strawberry"}}],[{"1234567890":{"_id":"576d1b7b79527b6201ed160c","index":2,"guid":"2d2d7a45-f931-4202-853d-563af252ca13","isActive":true,"balance":"$1,446.93","picture":"http://placehold.it/32x32","age":28,"eyeColor":"blue","name":{"first":"Pickett","last":"York"},"company":"ECSTASIA","email":"pickett.york@ecstasia.me","phone":"+1(901)571-3225","address":"556GrovePlace,Stouchsburg,Florida,9119","about":"Idnulladolorincididuntirurepariaturlaborumutmolliteavelitnonveniaminaliquip.Adametirureesseanimindoloreduisproidentdeserunteaconsecteturincididuntconsecteturminim.Ullamcoessedolorelitextemporexcepteurexcepteurlaboreipsumestquispariaturmagna.ExcepteurpariaturexcepteuradlaborissitquieiusmodmagnalaborisincididuntLoremLoremoccaecat.","registered":"Thursday,January28,20165:20PM","latitude":"-56.18036","longitude":"-39.088125","tags":["ad","velit","fugiat","deserunt","sint"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"BarryCleveland"},{"id":1,"name":"KiddWare"},{"id":2,"name":"LangBrooks"}],"greeting":"Hello,Pickett!Youhave10unreadmessages.","favoriteFruit":"strawberry"}}],[{"@":{"_id":"576d1b7bc674d071a2bccc05","index":3,"guid":"14b44274-45c2-4fd4-8c86-476a286cb7a2","isActive":true,"balance":"$1,861.79","picture":"http://placehold.it/32x32","age":27,"eyeColor":"brown","name":{"first":"Felecia","last":"Baird"},"company":"SYBIXTEX","email":"felecia.baird@sybixtex.name","phone":"+1(821)498-2971","address":"571HarrisonAvenue,Roulette,Missouri,9284","about":"Adesseofficianisiexercitationexcepteurametconsecteturessequialiquaquicupidatatincididunt.Nostrudullamcoutlaboreipsumduis.ConsequatsuntlaborumadLoremeaametveniamesseoccaecat.","registered":"Monday,December21,20156:50AM","latitude":"0.046813","longitude":"-13.86172","tags":["velit","qui","ut","aliquip","eiusmod"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"CeliaLucas"},{"id":1,"name":"HensonKline"},{"id":2,"name":"ElliottWalker"}],"greeting":"Hello,Felecia!Youhave9unreadmessages.","favoriteFruit":"apple"}}],[{"|}{[]αв¢∂єƒgαв¢∂єƒg":{"_id":"576d1b7be4096344db437417","index":4,"guid":"f789235d-b786-459f-9288-0d2f53058d02","isActive":false,"balance":"$2,011.07","picture":"http://placehold.it/32x32","age":28,"eyeColor":"brown","name":{"first":"Haney","last":"Burks"},"company":"SPACEWAX","email":"haney.burks@spacewax.info","phone":"+1(986)587-2735","address":"197OtsegoStreet,Chesterfield,Delaware,5551","about":"Quisirurenostrudcupidatatconsequatfugiatvoluptateproidentvoluptate.Duisnullaadipisicingofficiacillumsuntlaborisdeseruntirure.Laborumconsecteturelitreprehenderitestcillumlaboresintestnisiet.Suntdeseruntexercitationutauteduisaliquaametetquisvelitconsecteturirure.Auteipsumminimoccaecatincididuntaute.Irureenimcupidatatexercitationutad.Minimconsecteturadipisicingcommodoanim.","registered":"Friday,January16,20155:29AM","latitude":"86.036358","longitude":"-1.645066","tags":["occaecat","laboris","ipsum","culpa","est"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"SusannePacheco"},{"id":1,"name":"SpearsBerry"},{"id":2,"name":"VelazquezBoyle"}],"greeting":"Hello,Haney!Youhave10unreadmessages.","favoriteFruit":"apple"}}]]);
      ASSERT_EQ(readAndCheck([["/!!@#$%^&*)/address"]]),[{"!!@#$%^&*)":{"address": "818ButlerStreet,Berwind,Colorado,2490"}}]);
    }



////////////////////////////////////////////////////////////////////////////////
/// @brief test arrays
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, arrays) {
      writeAndCheck([[{"/":[]}]]);
      ASSERT_EQ(readAndCheck([["/"]]),[[]]);
      writeAndCheck([[{"/":[1,2,3]}]]);
      ASSERT_EQ(readAndCheck([["/"]]),[[1,2,3]]);
      writeAndCheck([[{"/a":[1,2,3]}]]);
      ASSERT_EQ(readAndCheck([["/"]]),[{a:[1,2,3]}]);
      writeAndCheck([[{"1":["C","C++","Java","Python"]}]]);
      ASSERT_EQ(readAndCheck([["/1"]]),[{1:["C","C++","Java","Python"]}]);
      writeAndCheck([[{"1":["C",2.0,"Java","Python"]}]]);
      ASSERT_EQ(readAndCheck([["/1"]]),[{1:["C",2.0,"Java","Python"]}]);
      writeAndCheck([[{"1":["C",2.0,"Java",{"op":"set","new":12,"ttl":7}]}]]);
      ASSERT_EQ(readAndCheck([["/1"]]),[{"1":["C",2,"Java",{"op":"set","new":12,"ttl":7}]}]);
      writeAndCheck([[{"1":["C",2.0,"Java",{"op":"set","new":12,"ttl":7,"Array":[12,3]}]}]]);
      ASSERT_EQ(readAndCheck([["/1"]]),[{"1":["C",2,"Java",{"op":"set","new":12,"ttl":7,"Array":[12,3]}]}]);
      writeAndCheck([[{"2":[[],[],[],[],[[[[[]]]]]]}]]);
      ASSERT_EQ(readAndCheck([["/2"]]),[{"2":[[],[],[],[],[[[[[]]]]]]}]);
      writeAndCheck([[{"2":[[[[[[]]]]],[],[],[],[[]]]}]]);
      ASSERT_EQ(readAndCheck([["/2"]]),[{"2":[[[[[[]]]]],[],[],[],[[]]]}]);
      writeAndCheck([[{"2":[[[[[["Hello World"],"Hello World"],1],2.0],"C"],[1],[2],[3],[[1,2],3],4]}]]);
      ASSERT_EQ(readAndCheck([["/2"]]),[{"2":[[[[[["Hello World"],"Hello World"],1],2.0],"C"],[1],[2],[3],[[1,2],3],4]}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple transaction
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, transaction) {

    testTransaction : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,4]},"e":12},"d":false}],
                     [{"a":{"b":{"c":[1,2,3]}}}]]);
      ASSERT_EQ(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{}},{a:{b:{c:[1,2,3]},d:false}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "new" operator
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, op_set_new) {
      writeAndCheck([[{"a/z":{"op":"set","new":12}}]]);
      ASSERT_EQ(readAndCheck([["/a/z"]]), [{"a":{"z":12}}]);
      writeAndCheck([[{"a/y":{"op":"set","new":12, "ttl": 1}}]]);
      ASSERT_EQ(readAndCheck([["/a/y"]]), [{"a":{"y":12}}]);
      wait(1.1);
      ASSERT_EQ(readAndCheck([["/a/y"]]), [{a:{}}]);
      writeAndCheck([[{"/a/y":{"op":"set","new":12, "ttl": 3}}]]);
      ASSERT_EQ(readAndCheck([["a/y"]]), [{"a":{"y":12}}]);
      wait(3.1);
      ASSERT_EQ(readAndCheck([["/a/y"]]), [{a:{}}]);
      writeAndCheck([[{"foo/bar":{"op":"set","new":{"baz":12}}}]]);
      ASSERT_EQ(readAndCheck([["/foo/bar/baz"]]),
                  [{"foo":{"bar":{"baz":12}}}]);
      ASSERT_EQ(readAndCheck([["/foo/bar"]]), [{"foo":{"bar":{"baz":12}}}]);
      ASSERT_EQ(readAndCheck([["/foo"]]), [{"foo":{"bar":{"baz":12}}}]);
      writeAndCheck([[{"foo/bar":{"op":"set","new":{"baz":12},"ttl":3}}]]);
      wait(3.1);
      ASSERT_EQ(readAndCheck([["/foo"]]), [{"foo":{}}]);
      ASSERT_EQ(readAndCheck([["/foo/bar"]]), [{"foo":{}}]);
      ASSERT_EQ(readAndCheck([["/foo/bar/baz"]]), [{"foo":{}}]);
      writeAndCheck([[{"a/u":{"op":"set","new":25, "ttl": 3}}]]);
      ASSERT_EQ(readAndCheck([["/a/u"]]), [{"a":{"u":25}}]);
      writeAndCheck([[{"a/u":{"op":"set","new":26}}]]);
      ASSERT_EQ(readAndCheck([["/a/u"]]), [{"a":{"u":26}}]);
      wait(3.0);  // key should still be there
      ASSERT_EQ(readAndCheck([["/a/u"]]), [{"a":{"u":26}}]);
      writeAndCheck([
        [{ "/a/u": { "op":"set", "new":{"z":{"z":{"z":"z"}}}, "ttl":30 }}]]);

      // temporary to make sure we remain with same leader.
      auto tmp = agencyLeader;
      auto leaderErr = false;

      let res = request({url: agencyLeader + "/_api/agency/stores",
                         method: "GET", followRedirect: true});
      if (res.statusCode === 200) {
        res.bodyParsed = JSON.parse(res.body);
        if (res.bodyParsed.read_db[0].a !== undefined) {
          ASSERT_TRUE(res.bodyParsed.read_db[1]["/a/u"] >= 0);
        } else {
          leaderErr = true; // not leader
        }
      } else {
        ASSERT_TRUE(false); // no point in continuing
      }

      // continue ttl test only, if we have not already lost
      // touch with the leader
      if (!leaderErr) {
        writeAndCheck([
          [{ "/a/u": { "op":"set", "new":{"z":{"z":{"z":"z"}}} }}]]);

        res = request({url: agencyLeader + "/_api/agency/stores",
                       method: "GET", followRedirect: true});

        // only, if agency is still led by same guy/girl
        if (agencyLeader === tmp) {
          if (res.statusCode === 200) {
            res.bodyParsed = JSON.parse(res.body);
            console.warn(res.bodyParsed.read_db[0]);
            if (res.bodyParsed.read_db[0].a !== undefined) {
              ASSERT_TRUE(res.bodyParsed.read_db[1]["/a/u"] === undefined);
            } else {
              leaderErr = true;
            }
          } else {
            ASSERT_TRUE(false); // no point in continuing
          }
        } else {
          leaderErr = true;
        }
      }

      if (leaderErr) {
        require("console").warn("on the record: status code was " + res.statusCode + " couldn't test proper implementation of TTL at this point. not going to startle the chickens over this however and assume rare leader change within.");
      }
     }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "push" operator
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, op_push) {
      writeAndCheck([[{"/a/b/c":{"op":"push","new":"max"}}]]);
      ASSERT_EQ(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3,"max"]}}}]);
      writeAndCheck([[{"/a/euler":{"op":"push","new":2.71828182845904523536}}]]);
      ASSERT_EQ(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"/a/euler":{"op":"set","new":2.71828182845904523536}}]]);
      ASSERT_EQ(readAndCheck([["/a/euler"]]),
                  [{a:{euler:2.71828182845904523536}}]);
      writeAndCheck([[{"/a/euler":{"op":"push","new":2.71828182845904523536}}]]);
      ASSERT_EQ(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);

      writeAndCheck([[{"/version":{"op":"set", "new": {"c": ["hello"]}, "ttl":3}}]]);
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);
      writeAndCheck([[{"/version/c":{"op":"push", "new":"world"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["hello","world"]}}]);
      wait(3.1);
      ASSERT_EQ(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"push", "new":"hello"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);

    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "remove" operator
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, op_remove) {
      writeAndCheck([[{"/a/euler":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/a/euler"]]), [{a:{}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "prepend" operator
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, op_prepend) {
      writeAndCheck([[{"/a/b/c":{"op":"prepend","new":3.141592653589793}}]]);
      ASSERT_EQ(readAndCheck([["/a/b/c"]]),
                  [{a:{b:{c:[3.141592653589793,1,2,3,"max"]}}}]);
      writeAndCheck(
        [[{"/a/euler":{"op":"prepend","new":2.71828182845904523536}}]]);
      ASSERT_EQ(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck(
        [[{"/a/euler":{"op":"set","new":2.71828182845904523536}}]]);
      ASSERT_EQ(readAndCheck([["/a/euler"]]),
                  [{a:{euler:2.71828182845904523536}}]);
      writeAndCheck(
        [[{"/a/euler":{"op":"prepend","new":2.71828182845904523536}}]]);
      ASSERT_EQ(readAndCheck(
        [["/a/euler"]]), [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"/a/euler":{"op":"prepend","new":1.25}}]]);
      ASSERT_EQ(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[1.25,2.71828182845904523536]}}]);

      writeAndCheck([[{"/version":{"op":"set", "new": {"c": ["hello"]}, "ttl":3}}]]);
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);
      writeAndCheck([[{"/version/c":{"op":"prepend", "new":"world"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["world","hello"]}}]);
      wait(3.1);
      ASSERT_EQ(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"prepend", "new":"hello"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);

    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "shift" operator
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, op_shift) {
      writeAndCheck([[{"/a/f":{"op":"shift"}}]]); // none before
      ASSERT_EQ(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/e":{"op":"shift"}}]]); // on empty array
      ASSERT_EQ(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/b/c":{"op":"shift"}}]]); // on existing array
      ASSERT_EQ(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3,"max"]}}}]);
      writeAndCheck([[{"/a/b/d":{"op":"shift"}}]]); // on existing scalar
      ASSERT_EQ(readAndCheck([["/a/b/d"]]), [{a:{b:{d:[]}}}]);

      writeAndCheck([[{"/version":{"op":"set", "new": {"c": ["hello","world"]}, "ttl":3}}]]);
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["hello","world"]}}]);
      writeAndCheck([[{"/version/c":{"op":"shift"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["world"]}}]);
      wait(3.1);
      ASSERT_EQ(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"shift"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:[]}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "pop" operator
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, op_pop) {
      writeAndCheck([[{"/a/f":{"op":"pop"}}]]); // none before
      ASSERT_EQ(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/e":{"op":"pop"}}]]); // on empty array
      ASSERT_EQ(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/b/c":{"op":"pop"}}]]); // on existing array
      ASSERT_EQ(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3]}}}]);
      writeAndCheck([[{"a/b/d":1}]]); // on existing scalar
      writeAndCheck([[{"/a/b/d":{"op":"pop"}}]]); // on existing scalar
      ASSERT_EQ(readAndCheck([["/a/b/d"]]), [{a:{b:{d:[]}}}]);

      writeAndCheck([[{"/version":{"op":"set", "new": {"c": ["hello","world"]}, "ttl":3}}]]);
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["hello","world"]}}]);
      writeAndCheck([[{"/version/c":{"op":"pop"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);
      wait(3.1);
      ASSERT_EQ(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"pop"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:[]}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "pop" operator
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, op_erase) {

      writeAndCheck([[{"/version":{"op":"delete"}}]]);

      writeAndCheck([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]]); // none before
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[0,1,2,3,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":3}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[0,1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":3}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[0,1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":0}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":1}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":2}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":4}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":5}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":9}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[6,7,8]}]);
      writeAndCheck([[{"a":{"op":"erase","val":7}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[6,8]}]);
      writeAndCheck([[{"a":{"op":"erase","val":6}}],
                     [{"a":{"op":"erase","val":8}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[]}]);

      writeAndCheck([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]]); // none before
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[0,1,2,3,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":3}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[0,1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":0}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":0}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":2}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[2,4,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":4}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[2,4,6,7,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":2}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[2,4,7,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":2}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[2,4,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":0}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[4,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":1}}],
                     [{"a":{"op":"erase","pos":0}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[]}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "pop" operator
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, op_replace) {
      writeAndCheck([[{"/version":{"op":"delete"}}]]); // clear
      writeAndCheck([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[0,1,2,3,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":3,"new":"three"}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[0,1,2,"three",4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":1,"new":[1]}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]), [{a:[0,[1],2,"three",4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":[1],"new":[1,2,3]}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]),
                  [{a:[0,[1,2,3],2,"three",4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":[1,2,3],"new":[1,2,3]}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]),
                  [{a:[0,[1,2,3],2,"three",4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":4,"new":[1,2,3]}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]),
                  [{a:[0,[1,2,3],2,"three",[1,2,3],5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":9,"new":[1,2,3]}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]),
                  [{a:[0,[1,2,3],2,"three",[1,2,3],5,6,7,8,[1,2,3]]}]);
      writeAndCheck([[{"a":{"op":"replace","val":[1,2,3],"new":{"a":0}}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]),
                  [{a:[0,{a:0},2,"three",{a:0},5,6,7,8,{a:0}]}]);
      writeAndCheck([[{"a":{"op":"replace","val":{"a":0},"new":"a"}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]),
                  [{a:[0,"a",2,"three","a",5,6,7,8,"a"]}]);
      writeAndCheck([[{"a":{"op":"replace","val":"a","new":"/a"}}]]);
      ASSERT_EQ(readAndCheck([["/a"]]),
                  [{a:[0,"/a",2,"three","/a",5,6,7,8,"/a"]}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "increment" operator
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, op_increment) {
      writeAndCheck([[{"/version":{"op":"delete"}}]]);
      writeAndCheck([[{"/version":{"op":"increment"}}]]); // none before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:1}]);
      writeAndCheck([[{"/version":{"op":"increment"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:2}]);
      writeAndCheck([[{"/version":{"op":"set", "new": {"c":12}, "ttl":3}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:12}}]);
      writeAndCheck([[{"/version/c":{"op":"increment"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:13}}]);
      wait(3.1);
      ASSERT_EQ(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"increment"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:1}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "decrement" operator
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, op_decrement) {
      writeAndCheck([[{"/version":{"op":"delete"}}]]);
      writeAndCheck([[{"/version":{"op":"decrement"}}]]); // none before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:-1}]);
      writeAndCheck([[{"/version":{"op":"decrement"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:-2}]);
      writeAndCheck([[{"/version":{"op":"set", "new": {"c":12}, "ttl":3}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:12}}]);
      writeAndCheck([[{"/version/c":{"op":"decrement"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:11}}]);
      wait(3.1);
      ASSERT_EQ(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"decrement"}}]]); // int before
      ASSERT_EQ(readAndCheck([["version"]]), [{version:{c:-1}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "op" keyword in other places than as operator
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, op_in_strange_places) {
      writeAndCheck([[{"/op":12}]]);
      ASSERT_EQ(readAndCheck([["/op"]]), [{op:12}]);
      writeAndCheck([[{"/op":{op:"delete"}}]]);
      writeAndCheck([[{"/op/a/b/c":{"op":"set","new":{"op":13}}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:13}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"increment"}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:14}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"decrement"}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:13}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"pop"}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[]}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"increment"}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:1}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"shift"}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[]}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"decrement"}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:-1}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"push","new":-1}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[-1]}}}}}]);
      writeAndCheck([[{"/op/a/b/d":{"op":"set","new":{"ttl":14}}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:14}}}}}]);
      writeAndCheck([[{"/op/a/b/d/ttl":{"op":"increment"}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:15}}}}}]);
      writeAndCheck([[{"/op/a/b/d/ttl":{"op":"decrement"}}]]);
      ASSERT_EQ(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:14}}}}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief op delete on top node
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, operators_on_root_node) {
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);
      writeAndCheck([[{"/":{"op":"increment"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [1]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"/":{"op":"decrement"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [-1]);
      writeAndCheck([[{"/":{"op":"push","new":"Hello"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [["Hello"]]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"/":{"op":"push","new":"Hello"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [["Hello"]]);
      writeAndCheck([[{"/":{"op":"pop"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"pop"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"push","new":"Hello"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [["Hello"]]);
      writeAndCheck([[{"/":{"op":"shift"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"shift"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"prepend","new":"Hello"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [["Hello"]]);
      writeAndCheck([[{"/":{"op":"shift"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"pop"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test observe / unobserve
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, observe) {
      auto res, before, after, clean;
      auto trx = [{"/a":"a"}, {"a":{"oldEmpty":true}}];

      // In the beginning
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      ASSERT_EQ(200, res.statusCode);
      clean = JSON.parse(res.body);

      // Don't create empty object for observation
      writeAndCheck([[{"/a":{"op":"observe", "url":"https://google.com"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);
      res = accessAgency("write",[trx]);
      ASSERT_EQ(res.statusCode, 200);
      res = accessAgency("write",[trx]);
      ASSERT_EQ(res.statusCode, 412);

      writeAndCheck([[{"/":{"op":"delete"}}]]);
      auto c = agencyConfig().term;

      // No duplicate entries in
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      ASSERT_EQ(200, res.statusCode);
      before = JSON.parse(res.body);
      writeAndCheck([[{"/a":{"op":"observe", "url":"https://google.com"}}]]);
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      ASSERT_EQ(200, res.statusCode);
      after = JSON.parse(res.body);
      if (!_.isEqual(before, after)) {
        if (agencyConfig().term === c) {
          ASSERT_EQ(before, after); //peng
        } else {
          require("console").warn("skipping remaining callback tests this time around");
          return; //
        }
      }

      // Normalization
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      ASSERT_EQ(200, res.statusCode);
      before = JSON.parse(res.body);
      writeAndCheck([[{"//////a////":{"op":"observe", "url":"https://google.com"}}]]);
      writeAndCheck([[{"a":{"op":"observe", "url":"https://google.com"}}]]);
      writeAndCheck([[{"a/":{"op":"observe", "url":"https://google.com"}}]]);
      writeAndCheck([[{"/a/":{"op":"observe", "url":"https://google.com"}}]]);
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      ASSERT_EQ(200, res.statusCode);
      after = JSON.parse(res.body);
      if (!_.isEqual(before, after)) {
        if (agencyConfig().term === c) {
          ASSERT_EQ(before, after); //peng
        } else {
          require("console").warn("skipping remaining callback tests this time around");
          return; //
        }
      }

      // Unobserve
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      ASSERT_EQ(200, res.statusCode);
      before = JSON.parse(res.body);
      writeAndCheck([[{"//////a":{"op":"unobserve", "url":"https://google.com"}}]]);
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      ASSERT_EQ(200, res.statusCode);
      after = JSON.parse(res.body);
      ASSERT_EQ(clean, after);
      if (!_.isEqual(clean, after)) {
        if (agencyConfig().term === c) {
          ASSERT_EQ(clean, after); //peng
        } else {
          require("console").warn("skipping remaining callback tests this time around");
          return; //
        }
      }

      writeAndCheck([[{"/":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);

    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test delete / replace / erase should not create new stuff in agency
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, not_create) {
      auto trx = [{"/a":"a"}, {"a":{"oldEmpty":true}}], res;

      // Don't create empty object for observation
      writeAndCheck([[{"a":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);
      res = accessAgency("write",[trx]);
      ASSERT_EQ(res.statusCode, 200);
      res = accessAgency("write",[trx]);
      ASSERT_EQ(res.statusCode, 412);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);

      // Don't create empty object for observation
      writeAndCheck([[{"a":{"op":"replace", "val":1, "new":2}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);
      res = accessAgency("write",[trx]);
      ASSERT_EQ(res.statusCode, 200);
      res = accessAgency("write",[trx]);
      ASSERT_EQ(res.statusCode, 412);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);

      // Don't create empty object for observation
      writeAndCheck([[{"a":{"op":"erase", "val":1}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);
      res = accessAgency("write",[trx]);
      ASSERT_EQ(res.statusCode, 200);
      res = accessAgency("write",[trx]);
      ASSERT_EQ(res.statusCode, 412);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{}]);

    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test that order should not matter
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, order) {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      ASSERT_EQ(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"d":false, "a":{"b":{"c":[1,2,3]},"e":12}}]]);
      ASSERT_EQ(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]]);
      ASSERT_EQ(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]]);
      ASSERT_EQ(readAndCheck([["a/e"],["a/b","d"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test nasty willful attempt to break
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, order_evil) {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      ASSERT_EQ(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"d":false, "a":{"b":{"c":[1,2,3]},"e":12}}]]);
      ASSERT_EQ(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]]);
      ASSERT_EQ(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]]);
      ASSERT_EQ(readAndCheck([["a/e"],["a/b","d"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Test nasty willful attempt to break
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, slash_o_rama) {
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"//////////////////////a/////////////////////b//":
                       {"b///////c":4}}]]);
      ASSERT_EQ(readAndCheck([["/"]]), [{a:{b:{b:{c:4}}}}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"////////////////////////": "Hi there!"}]]);
      ASSERT_EQ(readAndCheck([["/"]]), ["Hi there!"]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck(
        [[{"/////////////////\\/////a/////////////^&%^&$^&%$////////b\\\n//":
           {"b///////c":4}}]]);
      ASSERT_EQ(readAndCheck([["/"]]),
                  [{"\\":{"a":{"^&%^&$^&%$":{"b\\\n":{"b":{"c":4}}}}}}]);
    }

TEST(StoreTest, keys_beginning_with_same_strings) {
      auto res = accessAgency("write",[[{"/bumms":{"op":"set","new":"fallera"}, "/bummsfallera": {"op":"set","new":"lalalala"}}]]);
      ASSERT_EQ(res.statusCode, 200);
      ASSERT_EQ(readAndCheck([["/bumms", "/bummsfallera"]]), [{bumms:"fallera", bummsfallera: "lalalala"}]);
    }

TEST(StoreTest, hidden_agency_write) {
      auto res = accessAgency("write",[[{".agency": {"op":"set","new":"fallera"}}]]);
      ASSERT_EQ(res.statusCode, 403);
    }

TEST(StoreTest, hidden_agency_write_slash) {
      auto res = accessAgency("write",[[{"/.agency": {"op":"set","new":"fallera"}}]]);
      ASSERT_EQ(res.statusCode, 403);
    }

TEST(StoreTest, hidden_agency_write_deep) {
      auto res = accessAgency("write",[[{"/.agency/hans": {"op":"set","new":"fallera"}}]]);
      ASSERT_EQ(res.statusCode, 403);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Compaction
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, log_compaction) {
      // Find current log index and erase all data:
      let cur = accessAgency("write",[[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];

      let count = compactionConfig.compactionStepSize - 100 - cur;
      require("console").topic("agency=info", "Avoiding log compaction for now with", count,
        "keys, from log entry", cur, "on.");
      doCountTransactions(count, 0);

      // Now trigger one log compaction and check all keys:
      let count2 = compactionConfig.compactionStepSize + 100 - (cur + count);
      require("console").topic("agency=info", "Provoking log compaction for now with", count2,
        "keys, from log entry", cur + count, "on.");
      doCountTransactions(count2, count);

      // All tests so far have not really written many log entries in
      // comparison to the compaction interval (with the default settings),
      let count3 = 2 * compactionConfig.compactionStepSize + 100
        - (cur + count + count2);
      require("console").topic("agency=info", "Provoking second log compaction for now with",
        count3, "keys, from log entry", cur + count + count2, "on.");
      doCountTransactions(count3, count + count2);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, huge_transaction_package) {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      auto huge = [];
      for (auto i = 0; i < 20000; ++i) {
        huge.push([{"a":{"op":"increment"}}, {}, "huge" + i]);
      }
      writeAndCheck(huge, 600);
      ASSERT_EQ(readAndCheck([["a"]]), [{"a":20000}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package, inc/dec
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, transaction_with_inc_dec) {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      auto trx = [];
      for (auto i = 0; i < 100; ++i) {
        trx.push([{"a":{"op":"increment"}}, {}, "inc" + i]);
        trx.push([{"a":{"op":"decrement"}}, {}, "dec" + i]);
      }
      writeAndCheck(trx);
      ASSERT_EQ(readAndCheck([["a"]]), [{"a":0}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction, update of same key
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, transaction_update_same_key) {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      auto trx = [];
      trx.push([{"a":"foo"}]);
      trx.push([{"a":"bar"}]);
      writeAndCheck(trx);
      ASSERT_EQ(readAndCheck([["a"]]), [{"a":"bar"}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction, insert and remove of same key
////////////////////////////////////////////////////////////////////////////////

TEST(StoreTest, transaction_insert_remove_same_key) {
    testTransactionInsertRemoveSameKey : function() {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      auto trx = [];
      trx.push([{"a":"foo"}]);
      trx.push([{"a":{"op":"delete"}}]);
      writeAndCheck(trx);
      ASSERT_EQ(readAndCheck([["/a"]]), [{}]);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package, all different keys
////////////////////////////////////////////////////////////////////////////////
TEST(StoreTest, transaction_different_keys) {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      auto huge = [], i;
      for (i = 0; i < 100; ++i) {
        huge.push([{["a" + i]:{"op":"increment"}}, {}, "diff" + i]);
      }
      writeAndCheck(huge);
      for (i = 0; i < 100; ++i) {
        ASSERT_EQ(readAndCheck([["a" + i]]), [{["a" + i]:1}]);
      }
    }
*/
