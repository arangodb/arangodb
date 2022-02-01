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
/// @author Ignacio Rodriguez
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"

#include "Agency/Store.h"
#include "Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/Compare.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <random>

namespace arangodb {
namespace tests {
namespace store_test_api {

class StoreTestAPI : public ::testing::Test {
 public:
  StoreTestAPI() : _server(), _store(_server.server(), nullptr) {}

 protected:
  arangodb::tests::mocks::MockCoordinator _server;
  arangodb::consensus::Store _store;

  std::shared_ptr<VPackBuilder> read(std::string const& json) {
    try {
      consensus::query_t q{VPackParser::fromJson(json)};
      auto result = std::make_shared<VPackBuilder>();
      _store.readMultiple(q->slice(), *result);
      return result;
    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) +
                               " while trying to read " + json);
    }
  }

  auto write(std::string const& json) {
    try {
      auto q = VPackParser::fromJson(json);
      return _store.applyTransactions(q->slice());
    } catch (std::exception& err) {
      throw std::runtime_error(std::string(err.what()) + " while parsing " +
                               json);
    }
  }

  template<typename TOstream, typename TValueContainer>
  static TOstream& insert_value_array(TOstream& out,
                                      TValueContainer const& src) {
    out << "[";
    if (!src.empty()) {
      std::copy(src.begin(), src.end() - 1,
                std::ostream_iterator<std::string>(out, ", "));
      out << src.back();
    }
    out << "]";
    return out;
  }

  std::vector<consensus::apply_ret_t> write(
      std::vector<std::vector<std::string>> const& operations) {
    std::stringstream ss;
    ss << "[";
    if (!operations.empty()) {
      insert_value_array(ss, operations.front());
      for (auto oi{operations.begin() + 1}; oi != operations.end(); ++oi) {
        ss << ", ";
        insert_value_array(ss, *oi);
      }
    }
    ss << "]";
    return write(ss.str());
  }

  std::vector<consensus::apply_ret_t> transactAndCheck(
      std::string const& json) {
    try {
      auto q = VPackParser::fromJson(json);
      return _store.applyTransactions(q->slice());
    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) +
                               ", transact failed processing " + json);
    }
  }

  void writeAndCheck(std::string const& json) {
    try {
      auto r{write(json)};
      auto applied_all =
          std::all_of(r.begin(), r.end(), [](auto const& result) {
            return result == consensus::apply_ret_t::APPLIED;
          });
      if (!applied_all) {
        throw std::runtime_error("This didn't work: " + json);
      }
    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) + " processing " + json);
    }
  }

  void assertEqual(std::shared_ptr<velocypack::Builder> result,
                   std::string const& expected_result) const {
    try {
      auto expected{VPackParser::fromJson(expected_result)};
      if (!velocypack::NormalizedCompare::equals(result->slice(),
                                                 expected->slice())) {
        throw std::runtime_error(result->toJson() +
                                 " should have been equal to " +
                                 expected->toJson());
      }
    } catch (std::exception const& ex) {
      throw std::runtime_error(std::string(ex.what()) + " comparing to " +
                               expected_result);
    }
  }
};

TEST_F(StoreTestAPI, basic_operations_empty_results) {
  auto q = VPackParser::fromJson(R"=(
        [[{"/": {"op":"delete"}}]]
      )=");
  auto v = _store.applyTransactions(q->slice());
  ASSERT_EQ(1, v.size());
  ASSERT_EQ(consensus::APPLIED, v.front());

  q = VPackParser::fromJson(R"=(
        ["/x"]
      )=");
  VPackBuilder result;
  ASSERT_TRUE(_store.read(q->slice(), result));
  VPackSlice res{result.slice()};
#if 0
  ASSERT_TRUE(res.isArray() && res.length() == 1);
  res = res[0];
#endif
  ASSERT_TRUE(res.isObject() && res.length() == 0);
  auto j = VPackParser::fromJson(R"=(
       {}
     )=");
  ASSERT_TRUE(
      velocypack::NormalizedCompare::equals(j->slice(), result.slice()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single top level key
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, single_top_level) {
  assertEqual(read(R"([["/x"]])"), R"([{}])");
  writeAndCheck(R"([[{"x":12}]])");
  assertEqual(read(R"([["/x"]])"), R"([{"x":12}])");
  writeAndCheck(R"([[{"x":{"op":"delete"}}]])");
  assertEqual(read(R"([["/x"]])"), R"([{}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single non-top level key
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, single_non_top_level) {
  assertEqual(read(R"([["/x/y"]])"), R"([{}])");
  writeAndCheck(R"([[{"x/y":12}]])");
  assertEqual(read(R"([["/x/y"]])"), R"([{"x":{"y":12}}])");
  writeAndCheck(R"([[{"x/y":{"op":"delete"}}]])");
  assertEqual(read(R"([["/x"]])"), R"([{"x":{}}])");
  writeAndCheck(R"([[{"x":{"op":"delete"}}]])");
  assertEqual(read(R"([["/x"]])"), R"([{}])");
}

template<typename TSource>
std::string to_json_object(
    TSource const& src,
    std::function<std::string(typename TSource::const_reference)> extractName,
    std::function<std::string(typename TSource::const_reference)>
        extractValue) {
  bool is_first{true};
  return "{" +
         std::accumulate(src.begin(), src.end(), std::string(),
                         [&is_first, extractName, extractValue](
                             std::string partial, auto const& element) {
                           if (is_first) {
                             is_first = false;
                           } else {
                             partial += ",";
                           }
                           partial += "\"" + extractName(element) +
                                      "\": " + extractValue(element);
                           return partial;
                         }) +
         "}";
}

template<typename TSource>
std::string to_json_object(TSource const& src) {
  return to_json_object(
      src, [](auto const& element) { return element.first; },
      [](auto const& element) { return element.second; });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test preconditions
////////////////////////////////////////////////////////////////////////////////
TEST_F(StoreTestAPI, precondition) {
  writeAndCheck(R"([[{"/a":12}]])");
  assertEqual(read(R"([["/a"]])"), R"([{"a":12}])");
  writeAndCheck(R"([[{"/a":13},{"/a":12}]])");
  assertEqual(read(R"([["/a"]])"), R"([{"a":13}])");
  auto res = write(R"([[{"/a":14},{"/a":12}]])");  // fail precond {"a":12}
  ASSERT_EQ(res.size(), 1);
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  writeAndCheck(R"([[{"a":{"op":"delete"}}]])");

  // fail precond oldEmpty
  res = write(R"([[{"a":14},{"a":{"oldEmpty":false}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  writeAndCheck(R"([[{"a":14},{"a":{"oldEmpty":true}}]])");  // precond oldEmpty
  writeAndCheck(R"([[{"a":14},{"a":{"old":14}}]])");         // precond old

  // fail precond old
  res = write(R"([[{"a":14},{"a":{"old":13}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  writeAndCheck(R"([[{"a":14},{"a":{"isArray":false}}]])");  // precond isArray

  // fail precond isArray
  res = write(R"([[{"a":14},{"a":{"isArray":true}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());

  // check object precondition
  res = write(R"([[{"/a/b/c":{"op":"set","new":12}}]])");
  res = write(
      R"([[{"/a/b/c":{"op":"set","new":13}},{"a":{"old":{"b":{"c":12}}}}]])");
  ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
  res = write(
      R"([[{"/a/b/c":{"op":"set","new":14}},{"/a":{"old":{"b":{"c":12}}}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  res = write(
      R"([[{"/a/b/c":{"op":"set","new":14}},{"/a":{"old":{"b":{"c":13}}}}]])");
  ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());

  // multiple preconditions
  res = write(R"([[{"/a":1,"/b":true,"/c":"c"},{"/a":{"oldEmpty":false}}]])");
  assertEqual(read(R"([["/a","/b","c"]])"), R"([{"a":1,"b":true,"c":"c"}])");
  res =
      write(R"([[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":true}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  assertEqual(read(R"([["/a"]])"), R"([{"a":1}])");
  res =
      write(R"([[{"/a":2},{"/a":{"oldEmpty":true},"/b":{"oldEmpty":false}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  assertEqual(read(R"([["/a"]])"), R"([{"a":1}])");
  res = write(
      R"([[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":false},"/c":{"oldEmpty":true}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  assertEqual(read(R"([["/a"]])"), R"([{"a":1}])");
  res = write(
      R"([[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]])");
  ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
  assertEqual(read(R"([["/a"]])"), R"([{"a":2}])");
  res = write(
      R"([[{"/a":3},{"/a":{"old":2},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]])");
  ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
  assertEqual(read(R"([["/a"]])"), R"([{"a":3}])");
  res = write(
      R"([[{"/a":2},{"/a":{"old":2},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  assertEqual(read(R"([["/a"]])"), R"([{"a":3}])");
  res = write(
      R"([[{"/a":2},{"/a":{"old":3},"/b":{"oldEmpty":false},"/c":{"isArray":true}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  assertEqual(read(R"([["/a"]])"), R"([{"a":3}])");
  res = write(
      R"([[{"/a":2},{"/a":{"old":3},"/b":{"oldEmpty":false},"/c":{"isArray":false}}]])");
  ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
  assertEqual(read(R"([["/a"]])"), R"([{"a":2}])");
  // in precondition & multiple
  writeAndCheck(R"([[{"a":{"b":{"c":[1,2,3]},"e":[1,2]},"d":false}]])");
  res = write(R"([[{"/b":2},{"/a/b/c":{"in":3}}]])");
  ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
  assertEqual(read(R"([["/b"]])"), R"([{"b":2}])");
  res = write(R"([[{"/b":3},{"/a/e":{"in":3}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  assertEqual(read(R"([["/b"]])"), R"([{"b":2}])");
  res = write(R"([[{"/b":3},{"/a/e":{"in":3},"/a/b/c":{"in":3}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  res = write(R"([[{"/b":3},{"/a/e":{"in":3},"/a/b/c":{"in":3}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  res = write(R"([[{"/b":3},{"/a/b/c":{"in":3},"/a/e":{"in":3}}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  res = write(R"([[{"/b":3},{"/a/b/c":{"in":3},"/a/e":{"in":2}}]])");
  ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
  assertEqual(read(R"([["/b"]])"), R"([{"b":3}])");

  // Permute order of keys and objects within precondition
  std::map<std::string, std::string> baz{
      {"_id", "\"5a00203e4b660989b2ae5493\""},
      {"index", "0"},
      {"guid", "\"7a709cc2-1479-4079-a0a3-009cbe5674f4\""},
      {"isActive", "true"},
      {"balance", "\"$3,072.23\""},
      {"picture", "\"http://placehold.it/32x32\""},
      {"age", "21"},
      {"eyeColor", "\"green\""},
      {"name", R"({ "first": "Durham", "last": "Duke" })"},
      {"tags", R"(["anim","et","id","do","est",1.0,-1024,1024])"}};
  std::string const baz_text = to_json_object(baz);
  std::string const qux{R"(["3.14159265359",3.14159265359])"};
  std::string const foo_value{"\"bar\""};
  std::string const localObj = R"(
      {"foo" : )" + foo_value + R"(,
        "baz" : )" + baz_text + R"(,
        "qux" : )" + qux + R"(
      })";

  res = write(std::string("[[") + localObj + R"(,
      {
        "baz":{"old": )" +
              baz_text + R"(},
        "qux":)" +
              qux + R"(}]])");
  ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());

  writeAndCheck("[[" + localObj + "]]");
  writeAndCheck("[[" + localObj + R"(, {"foo":)" + foo_value +
                R"(,"baz":{"old":)" + baz_text + R"(},"qux":)" + qux + "}]]");
  writeAndCheck("[[" + localObj + R"(, {"baz":{"old":)" + baz_text +
                R"(},"foo":)" + foo_value + R"(,"qux":)" + qux + R"(}]])");
  writeAndCheck("[[" + localObj + R"(, {"baz":{"old":)" + baz_text +
                R"(},"qux":)" + qux + R"(,"foo":)" + foo_value + R"(}]])");
  writeAndCheck("[[" + localObj + R"(, {"qux":)" + qux + R"(,"baz":{"old":)" +
                baz_text + R"(},"foo":)" + foo_value + R"(}]])");

  std::vector<std::string> localKeys;
  std::transform(baz.begin(), baz.end(), std::back_inserter(localKeys),
                 [](auto kv) { return kv.first; });
  auto rd{std::random_device{}()};
  auto g{std::mt19937(rd)};
  for (int permutation_count{}; permutation_count < 5; ++permutation_count) {
    std::shuffle(localKeys.begin(), localKeys.end(), g);
    std::string const permuted = to_json_object(
        localKeys, [](std::string const& k) { return k; },
        [&baz](std::string const& k) { return baz[k]; });
    writeAndCheck("[[" + localObj + R"(, {"foo":)" + foo_value +
                  R"(,"baz":{"old":)" + permuted + R"(},"qux":)" + qux + "}]]");
    writeAndCheck("[[" + localObj + R"(, {"baz":{"old":)" + permuted +
                  R"(},"foo":)" + foo_value + R"(,"qux":)" + qux + R"(}]])");
    writeAndCheck("[[" + localObj + R"(, {"baz":{"old":)" + permuted +
                  R"(},"qux":)" + qux + R"(,"foo":)" + foo_value + R"(}]])");
    writeAndCheck("[[" + localObj + R"(, {"qux":)" + qux + R"(,"baz":{"old":)" +
                  permuted + R"(},"foo":)" + foo_value + R"(}]])");
  }

  // Permute order of keys and objects within arrays in preconditions
  {
    writeAndCheck(R"([[{"a":[{"b":12,"c":13}]}]])");
    writeAndCheck(R"([[{"a":[{"b":12,"c":13}]},{"a":[{"b":12,"c":13}]}]])");
    writeAndCheck(R"([[{"a":[{"b":12,"c":13}]},{"a":[{"c":13,"b":12}]}]])");

    std::map<std::string, std::string> localObj{{"b", "\"Hello world!\""},
                                                {"c", "3.14159265359"},
                                                {"d", "314159265359"},
                                                {"e", "-3"}};
    std::map<std::string, std::string> localObk{
        {"b", "1"}, {"c", "1.0"}, {"d", "100000000001"}, {"e", "-1"}};
    localKeys.resize(0);
    for (auto const& l : localObj) {
      localKeys.push_back(l.first);
    }
    std::string const localObj_text{to_json_object(localObj)};
    std::string const localObk_text{to_json_object(localObk)};
    writeAndCheck(R"([[ { "a" : [)" + localObj_text + "," + localObk_text +
                  R"(] } ]])");
    writeAndCheck(R"([[ { "a" : [)" + localObj_text + "," + localObk_text +
                  R"(] }, {"a" : [)" + localObj_text + "," + localObk_text +
                  R"(] }]])");
    auto rd{std::random_device{}()};
    auto g{std::mt19937(rd)};
    for (int m = 0; m < 7; ++m) {
      std::shuffle(localKeys.begin(), localKeys.end(), g);
      auto per1 = to_json_object(
          localKeys, [](std::string const& key) { return key; },
          [&localObj](std::string const& key) { return localObj.at(key); });
      auto per2 = to_json_object(
          localKeys, [](std::string const& key) { return key; },
          [&localObk](std::string const& key) { return localObk.at(key); });
      writeAndCheck(R"([[ { "a" : [)" + localObj_text + "," + localObk_text +
                    R"(] }, {"a" : [)" + per1 + "," + per2 + R"(] }]])");
      res = write(R"([[ { "a" : [)" + localObj_text + "," + localObk_text +
                  R"(] }, {"a" : [)" + per2 + "," + per1 + R"(] }]])");
      ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
    }

    res = write(R"([[{"a":12},{"a":{"intersectionEmpty":""}}]])");
    ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
    res = write(R"([[{"a":12},{"a":{"intersectionEmpty":[]}}]])");
    ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
    res = write(R"([[{"a":[12,"Pi",3.14159265359,true,false]},
                                {"a":{"intersectionEmpty":[]}}]])");
    ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
    res = write(R"([[{"a":[12,"Pi",3.14159265359,true,false]},
                                {"a":{"intersectionEmpty":[false,"Pi"]}}]])");
    ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
    res = write(R"([[{"a":[12,"Pi",3.14159265359,true,false]},
                                {"a":{"intersectionEmpty":["Pi",false]}}]])");
    ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
    res = write(R"([[{"a":[12,"Pi",3.14159265359,true,false]},
                                {"a":{"intersectionEmpty":[false,false,false]}}]])");
    ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
    res = write(R"([[{"a":[12,"Pi",3.14159265359,true,false]},
                                {"a":{"intersectionEmpty":["pi",3.1415926535]}}]])");
    ASSERT_EQ(consensus::apply_ret_t::APPLIED, res.front());
    res = write(R"([[{"a":[12,"Pi",3.14159265359,true,false]},
                                  {"a":{"instersectionEmpty":[]}}]])");
    ASSERT_EQ(consensus::apply_ret_t::PRECONDITION_FAILED, res.front());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test document/transaction assignment
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, document) {
  writeAndCheck(R"([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]])");
  assertEqual(read(R"([["a/e"],[ "d","a/b"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
  writeAndCheck(
      R"--([[{"a":{"_id":"576d1b7becb6374e24ed5a04","index":0,"guid":"60ffa50e-0211-4c60-a305-dcc8063ae2a5","isActive":true,"balance":"$1,050.96","picture":"http://placehold.it/32x32","age":30,"eyeColor":"green","name":{"first":"Maura","last":"Rogers"},"company":"GENESYNK","email":"maura.rogers@genesynk.net","phone":"+1(804)424-2766","address":"501RiverStreet,Wollochet,Vermont,6410","about":"Temporsintofficiaipsumidnullalaboreminimlaborisinlaborumincididuntexcepteurdolore.Sunteumagnadolaborumsunteaquisipsumaliquaaliquamagnaminim.Cupidatatadproidentullamconisietofficianisivelitculpaexcepteurqui.Suntautemollitconsecteturnulla.Commodoquisidmagnaestsitelitconsequatdoloreupariaturaliquaetid.","registered":"Friday,November28,20148:01AM","latitude":"-30.093679","longitude":"10.469577","tags":["laborum","proident","est","veniam","sunt"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"CarverDurham"},{"id":1,"name":"DanielleMalone"},{"id":2,"name":"ViolaBell"}],"greeting":"Hello,Maura!Youhave9unreadmessages.","favoriteFruit":"banana"}}],[{"!!@#$%^&*)":{"_id":"576d1b7bb2c1af32dd964c22","index":1,"guid":"e6bda5a9-54e3-48ea-afd7-54915fec48c2","isActive":false,"balance":"$2,631.75","picture":"http://placehold.it/32x32","age":40,"eyeColor":"blue","name":{"first":"Jolene","last":"Todd"},"company":"QUANTASIS","email":"jolene.todd@quantasis.us","phone":"+1(954)418-2311","address":"818ButlerStreet,Berwind,Colorado,2490","about":"Commodoesseveniamadestirureutaliquipduistempor.Auteeuametsuntessenisidolorfugiatcupidatatsintnulla.Sitanimincididuntelitculpasunt.","registered":"Thursday,June12,201412:08AM","latitude":"-7.101063","longitude":"4.105685","tags":["ea","est","sunt","proident","pariatur"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"SwansonMcpherson"},{"id":1,"name":"YoungTyson"},{"id":2,"name":"HinesSandoval"}],"greeting":"Hello,Jolene!Youhave5unreadmessages.","favoriteFruit":"strawberry"}}],[{"1234567890":{"_id":"576d1b7b79527b6201ed160c","index":2,"guid":"2d2d7a45-f931-4202-853d-563af252ca13","isActive":true,"balance":"$1,446.93","picture":"http://placehold.it/32x32","age":28,"eyeColor":"blue","name":{"first":"Pickett","last":"York"},"company":"ECSTASIA","email":"pickett.york@ecstasia.me","phone":"+1(901)571-3225","address":"556GrovePlace,Stouchsburg,Florida,9119","about":"Idnulladolorincididuntirurepariaturlaborumutmolliteavelitnonveniaminaliquip.Adametirureesseanimindoloreduisproidentdeserunteaconsecteturincididuntconsecteturminim.Ullamcoessedolorelitextemporexcepteurexcepteurlaboreipsumestquispariaturmagna.ExcepteurpariaturexcepteuradlaborissitquieiusmodmagnalaborisincididuntLoremLoremoccaecat.","registered":"Thursday,January28,20165:20PM","latitude":"-56.18036","longitude":"-39.088125","tags":["ad","velit","fugiat","deserunt","sint"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"BarryCleveland"},{"id":1,"name":"KiddWare"},{"id":2,"name":"LangBrooks"}],"greeting":"Hello,Pickett!Youhave10unreadmessages.","favoriteFruit":"strawberry"}}],[{"@":{"_id":"576d1b7bc674d071a2bccc05","index":3,"guid":"14b44274-45c2-4fd4-8c86-476a286cb7a2","isActive":true,"balance":"$1,861.79","picture":"http://placehold.it/32x32","age":27,"eyeColor":"brown","name":{"first":"Felecia","last":"Baird"},"company":"SYBIXTEX","email":"felecia.baird@sybixtex.name","phone":"+1(821)498-2971","address":"571HarrisonAvenue,Roulette,Missouri,9284","about":"Adesseofficianisiexercitationexcepteurametconsecteturessequialiquaquicupidatatincididunt.Nostrudullamcoutlaboreipsumduis.ConsequatsuntlaborumadLoremeaametveniamesseoccaecat.","registered":"Monday,December21,20156:50AM","latitude":"0.046813","longitude":"-13.86172","tags":["velit","qui","ut","aliquip","eiusmod"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"CeliaLucas"},{"id":1,"name":"HensonKline"},{"id":2,"name":"ElliottWalker"}],"greeting":"Hello,Felecia!Youhave9unreadmessages.","favoriteFruit":"apple"}}],[{"|}{[]αв¢∂єƒgαв¢∂єƒg":{"_id":"576d1b7be4096344db437417","index":4,"guid":"f789235d-b786-459f-9288-0d2f53058d02","isActive":false,"balance":"$2,011.07","picture":"http://placehold.it/32x32","age":28,"eyeColor":"brown","name":{"first":"Haney","last":"Burks"},"company":"SPACEWAX","email":"haney.burks@spacewax.info","phone":"+1(986)587-2735","address":"197OtsegoStreet,Chesterfield,Delaware,5551","about":"Quisirurenostrudcupidatatconsequatfugiatvoluptateproidentvoluptate.Duisnullaadipisicingofficiacillumsuntlaborisdeseruntirure.Laborumconsecteturelitreprehenderitestcillumlaboresintestnisiet.Suntdeseruntexercitationutauteduisaliquaametetquisvelitconsecteturirure.Auteipsumminimoccaecatincididuntaute.Irureenimcupidatatexercitationutad.Minimconsecteturadipisicingcommodoanim.","registered":"Friday,January16,20155:29AM","latitude":"86.036358","longitude":"-1.645066","tags":["occaecat","laboris","ipsum","culpa","est"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"SusannePacheco"},{"id":1,"name":"SpearsBerry"},{"id":2,"name":"VelazquezBoyle"}],"greeting":"Hello,Haney!Youhave10unreadmessages.","favoriteFruit":"apple"}}]])--");
  assertEqual(
      read(R"([["/!!@#$%^&*)/address"]])"),
      R"--([{"!!@#$%^&*)":{"address": "818ButlerStreet,Berwind,Colorado,2490"}}])--");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test arrays
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, arrays) {
  writeAndCheck(R"([[{"/":[]}]])");
  assertEqual(read(R"([["/"]])"), R"([[]])");
  writeAndCheck(R"([[{"/":[1,2,3]}]])");
  assertEqual(read(R"([["/"]])"), R"([[1,2,3]])");
  writeAndCheck(R"([[{"/a":[1,2,3]}]])");
  assertEqual(read(R"([["/"]])"), R"([{"a":[1,2,3]}])");
  writeAndCheck(R"([[{"1":["C","C++","Java","Python"]}]])");
  assertEqual(read(R"([["/1"]])"), R"([{"1":["C","C++","Java","Python"]}])");
  writeAndCheck(R"([[{"1":["C",2.0,"Java","Python"]}]])");
  assertEqual(read(R"([["/1"]])"), R"([{"1":["C",2.0,"Java","Python"]}])");
  writeAndCheck(R"([[{"1":["C",2.0,"Java",{"op":"set","new":12,"ttl":7}]}]])");
  assertEqual(read(R"([["/1"]])"),
              R"([{"1":["C",2,"Java",{"op":"set","new":12,"ttl":7}]}])");
  writeAndCheck(
      R"([[{"1":["C",2.0,"Java",{"op":"set","new":12,"ttl":7,"Array":[12,3]}]}]])");
  assertEqual(
      read(R"([["/1"]])"),
      R"([{"1":["C",2,"Java",{"op":"set","new":12,"ttl":7,"Array":[12,3]}]}])");
  writeAndCheck(R"([[{"2":[[],[],[],[],[[[[[]]]]]]}]])");
  assertEqual(read(R"([["/2"]])"), R"([{"2":[[],[],[],[],[[[[[]]]]]]}])");
  writeAndCheck(R"([[{"2":[[[[[[]]]]],[],[],[],[[]]]}]])");
  assertEqual(read(R"([["/2"]])"), R"([{"2":[[[[[[]]]]],[],[],[],[[]]]}])");
  writeAndCheck(
      R"([[{"2":[[[[[["Hello World"],"Hello World"],1],2.0],"C"],[1],[2],[3],[[1,2],3],4]}]])");
  assertEqual(
      read(R"([["/2"]])"),
      R"([{"2":[[[[[["Hello World"],"Hello World"],1],2.0],"C"],[1],[2],[3],[[1,2],3],4]}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple transaction
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, transaction) {
  writeAndCheck(R"([[{"a":{"b":{"c":[1,2,4]},"e":12},"d":false}],
                     [{"a":{"b":{"c":[1,2,3]}}}]])");
  assertEqual(read(R"([["a/e"],[ "d","a/b"]])"),
              R"([{"a":{}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "new" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_set_new) {
  writeAndCheck(R"([[{"a/z":{"op":"set","new":12}}]])");
  assertEqual(read(R"([["/a/z"]])"), R"([{"a":{"z":12}}])");
  writeAndCheck(R"([[{"a/y":{"op":"set","new":12, "ttl": 1}}]])");
  assertEqual(read(R"([["/a/y"]])"), R"([{"a":{"y":12}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{1100});
  assertEqual(read(R"([["/a/y"]])"), R"([{"a":{}}])");
  writeAndCheck(R"([[{"/a/y":{"op":"set","new":12, "ttl": 3}}]])");
  assertEqual(read(R"([["a/y"]])"), R"([{"a":{"y":12}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3100});
  assertEqual(read(R"([["/a/y"]])"), R"([{"a":{}}])");
  writeAndCheck(R"([[{"foo/bar":{"op":"set","new":{"baz":12}}}]])");
  assertEqual(read(R"([["/foo/bar/baz"]])"), R"([{"foo":{"bar":{"baz":12}}}])");
  assertEqual(read(R"([["/foo/bar"]])"), R"([{"foo":{"bar":{"baz":12}}}])");
  assertEqual(read(R"([["/foo"]])"), R"([{"foo":{"bar":{"baz":12}}}])");
  writeAndCheck(R"([[{"foo/bar":{"op":"set","new":{"baz":12},"ttl":3}}]])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3100});
  assertEqual(read(R"([["/foo"]])"), R"([{"foo":{}}])");
  assertEqual(read(R"([["/foo/bar"]])"), R"([{"foo":{}}])");
  assertEqual(read(R"([["/foo/bar/baz"]])"), R"([{"foo":{}}])");
  writeAndCheck(R"([[{"a/u":{"op":"set","new":25, "ttl": 3}}]])");
  assertEqual(read(R"([["/a/u"]])"), R"([{"a":{"u":25}}])");
  writeAndCheck(R"([[{"a/u":{"op":"set","new":26}}]])");
  assertEqual(read(R"([["/a/u"]])"), R"([{"a":{"u":26}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3000});
  assertEqual(read(R"([["/a/u"]])"), R"([{"a":{"u":26}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "push" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_push) {
  writeAndCheck(R"([[{"/a/b/c":[1,2,3]}]])");
  writeAndCheck(R"([[{"/a/b/c":{"op":"push","new":"max"}}]])");
  assertEqual(read(R"([["/a/b/c"]])"), R"([{"a":{"b":{"c":[1,2,3,"max"]}}}])");
  writeAndCheck(
      R"([[{"/a/euler":{"op":"push","new":2.71828182845904523536}}]])");
  assertEqual(read(R"([["/a/euler"]])"),
              R"([{"a":{"euler":[2.71828182845904523536]}}])");
  writeAndCheck(
      R"([[{"/a/euler":{"op":"set","new":2.71828182845904523536}}]])");
  assertEqual(read(R"([["/a/euler"]])"),
              R"([{"a":{"euler":2.71828182845904523536}}])");
  writeAndCheck(
      R"([[{"/a/euler":{"op":"push","new":2.71828182845904523536}}]])");
  assertEqual(read(R"([["/a/euler"]])"),
              R"([{"a":{"euler":[2.71828182845904523536]}}])");

  writeAndCheck(
      R"([[{"/version":{"op":"set", "new": {"c": ["hello"]}, "ttl":3}}]])");
  assertEqual(read(R"([["version"]])"), R"([{"version":{"c":["hello"]}}])");
  writeAndCheck(
      R"([[{"/version/c":{"op":"push", "new":"world"}}]])");  // int before
  assertEqual(read(R"([["version"]])"),
              R"([{"version":{"c":["hello","world"]}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3100});
  assertEqual(read(R"([["version"]])"), "[{}]");
  writeAndCheck(
      R"([[{"/version/c":{"op":"push", "new":"hello"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"([{"version":{"c":["hello"]}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "remove" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_remove) {
  writeAndCheck(R"([[{"/a/euler":2.71828182845904523536}]])");
  writeAndCheck(R"([[{"/a/euler":{"op":"delete"}}]])");
  assertEqual(read(R"([["/a/euler"]])"), R"([{"a":{}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "prepend" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_prepend) {
  writeAndCheck(R"([[{"/a/b/c":[1,2,3,"max"]}]])");
  writeAndCheck(R"([[{"/a/b/c":{"op":"prepend","new":3.141592653589793}}]])");
  assertEqual(read(R"([["/a/b/c"]])"),
              R"([{"a":{"b":{"c":[3.141592653589793,1,2,3,"max"]}}}])");
  writeAndCheck(
      R"([[{"/a/euler":{"op":"prepend","new":2.71828182845904523536}}]])");
  assertEqual(read(R"([["/a/euler"]])"),
              R"([{"a":{"euler":[2.71828182845904523536]}}])");
  writeAndCheck(
      R"([[{"/a/euler":{"op":"set","new":2.71828182845904523536}}]])");
  assertEqual(read(R"([["/a/euler"]])"),
              R"([{"a":{"euler":2.71828182845904523536}}])");
  writeAndCheck(
      R"([[{"/a/euler":{"op":"prepend","new":2.71828182845904523536}}]])");
  assertEqual(read(R"([["/a/euler"]])"),
              R"([{"a":{"euler":[2.71828182845904523536]}}])");
  writeAndCheck(R"([[{"/a/euler":{"op":"prepend","new":1.25}}]])");
  assertEqual(read(R"([["/a/euler"]])"),
              R"([{"a":{"euler":[1.25,2.71828182845904523536]}}])");

  writeAndCheck(
      R"([[{"/version":{"op":"set", "new": {"c": ["hello"]}, "ttl":3}}]])");
  assertEqual(read(R"([["version"]])"), R"([{"version":{"c":["hello"]}}])");
  writeAndCheck(
      R"([[{"/version/c":{"op":"prepend", "new":"world"}}]])");  // int before
  assertEqual(read(R"([["version"]])"),
              R"([{"version":{"c":["world","hello"]}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3100});
  assertEqual(read(R"([["version"]])"), "[{}]");
  writeAndCheck(
      R"([[{"/version/c":{"op":"prepend", "new":"hello"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"([{"version":{"c":["hello"]}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "shift" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_shift) {
  writeAndCheck(R"([[{"/a/f":{"op":"shift"}}]])");  // none before
  assertEqual(read(R"([["/a/f"]])"), R"([{"a":{"f":[]}}])");
  writeAndCheck(R"([[{"/a/e":{"op":"shift"}}]])");  // on empty array
  assertEqual(read(R"([["/a/f"]])"), R"([{"a":{"f":[]}}])");
  writeAndCheck(R"([[{"/a/b/c":["this-will-be-removed", 1,2,3,"max"]}]])");
  writeAndCheck(R"([[{"/a/b/c":{"op":"shift"}}]])");  // on existing array
  assertEqual(read(R"([["/a/b/c"]])"), R"([{"a":{"b":{"c":[1,2,3,"max"]}}}])");
  writeAndCheck(R"([[{"/a/b/d":{"op":"shift"}}]])");  // on existing scalar
  assertEqual(read(R"([["/a/b/d"]])"), R"([{"a":{"b":{"d":[]}}}])");

  writeAndCheck(
      R"([[{"/version":{"op":"set", "new": {"c": ["hello","world"]}, "ttl":3}}]])");
  assertEqual(read(R"([["version"]])"),
              R"([{"version":{"c":["hello","world"]}}])");
  writeAndCheck(R"([[{"/version/c":{"op":"shift"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"([{"version":{"c":["world"]}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3100});
  assertEqual(read(R"([["version"]])"), "[{}]");
  writeAndCheck(R"([[{"/version/c":{"op":"shift"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"([{"version":{"c":[]}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "pop" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_pop) {
  writeAndCheck(R"([[{"/a/f":{"op":"pop"}}]])");  // none before
  assertEqual(read(R"([["/a/f"]])"), R"( [{"a":{"f":[]}}])");
  writeAndCheck(R"([[{"/a/e":{"op":"pop"}}]])");  // on empty array
  assertEqual(read(R"([["/a/f"]])"), R"( [{"a":{"f":[]}}])");
  writeAndCheck(R"([[{"/a/b/c":[1,2,3,"max"]}]])");
  writeAndCheck(R"([[{"/a/b/c":{"op":"pop"}}]])");  // on existing array
  assertEqual(read(R"([["/a/b/c"]])"), R"( [{"a":{"b":{"c":[1,2,3]}}}])");
  writeAndCheck(R"([[{"a/b/d":1}]])");              // on existing scalar
  writeAndCheck(R"([[{"/a/b/d":{"op":"pop"}}]])");  // on existing scalar
  assertEqual(read(R"([["/a/b/d"]])"), R"( [{"a":{"b":{"d":[]}}}])");

  writeAndCheck(
      R"([[{"/version":{"op":"set", "new": {"c": ["hello","world"]}, "ttl":3}}]])");
  assertEqual(read(R"([["version"]])"),
              R"( [{"version":{"c":["hello","world"]}}])");
  writeAndCheck(R"([[{"/version/c":{"op":"pop"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"([{"version":{"c":["hello"]}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3100});
  assertEqual(read(R"([["version"]])"), R"([{}])");
  writeAndCheck(R"([[{"/version/c":{"op":"pop"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"( [{"version":{"c":[]}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "erase" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_erase) {
  writeAndCheck(R"([[{"/version":{"op":"delete"}}]])");

  writeAndCheck(R"([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]])");  // none before
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[0,1,2,3,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":3}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[0,1,2,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":3}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[0,1,2,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":0}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[1,2,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":1}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[2,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":2}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":4}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":5}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":9}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[6,7,8]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":7}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[6,8]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","val":6}}],
                     [{"a":{"op":"erase","val":8}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[]}])");

  writeAndCheck(R"([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]])");  // none before
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[0,1,2,3,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":3}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[0,1,2,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":0}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[1,2,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":0}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[2,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":2}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[2,4,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":4}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[2,4,6,7,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":2}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[2,4,7,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":2}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[2,4,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":0}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[4,9]}])");
  writeAndCheck(R"([[{"a":{"op":"erase","pos":1}}],
                     [{"a":{"op":"erase","pos":0}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[]}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "replace" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_replace) {
  writeAndCheck(R"([[{"/version":{"op":"delete"}}]])");  // clear
  writeAndCheck(R"([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[0,1,2,3,4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":3,"new":"three"}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[0,1,2,"three",4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":1,"new":[1]}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{"a":[0,[1],2,"three",4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":[1],"new":[1,2,3]}}]])");
  assertEqual(read(R"([["/a"]])"),
              R"([{"a":[0,[1,2,3],2,"three",4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":[1,2,3],"new":[1,2,3]}}]])");
  assertEqual(read(R"([["/a"]])"),
              R"([{"a":[0,[1,2,3],2,"three",4,5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":4,"new":[1,2,3]}}]])");
  assertEqual(read(R"([["/a"]])"),
              R"([{"a":[0,[1,2,3],2,"three",[1,2,3],5,6,7,8,9]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":9,"new":[1,2,3]}}]])");
  assertEqual(read(R"([["/a"]])"),
              R"([{"a":[0,[1,2,3],2,"three",[1,2,3],5,6,7,8,[1,2,3]]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":[1,2,3],"new":{"a":0}}}]])");
  assertEqual(read(R"([["/a"]])"),
              R"([{"a":[0,{"a":0},2,"three",{"a":0},5,6,7,8,{"a":0}]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":{"a":0},"new":"a"}}]])");
  assertEqual(read(R"([["/a"]])"),
              R"([{"a":[0,"a",2,"three","a",5,6,7,8,"a"]}])");
  writeAndCheck(R"([[{"a":{"op":"replace","val":"a","new":"/a"}}]])");
  assertEqual(read(R"([["/a"]])"),
              R"([{"a":[0,"/a",2,"three","/a",5,6,7,8,"/a"]}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "increment" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_increment) {
  writeAndCheck(R"([[{"/version":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"/version":{"op":"increment"}}]])");  // none before
  assertEqual(read(R"([["version"]])"), R"( [{"version":1}])");
  writeAndCheck(R"([[{"/version":{"op":"increment"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"( [{"version":2}])");
  writeAndCheck(
      R"([[{"/version":{"op":"set", "new": {"c":12}, "ttl":3}}]])");  // int
                                                                      // before
  assertEqual(read(R"([["version"]])"), R"( [{"version":{"c":12}}])");
  writeAndCheck(R"([[{"/version/c":{"op":"increment"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"( [{"version":{"c":13}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3100});
  assertEqual(read(R"([["version"]])"), R"( [{}])");
  writeAndCheck(R"([[{"/version/c":{"op":"increment"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"( [{"version":{"c":1}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "decrement" operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_decrement) {
  writeAndCheck(R"([[{"/version":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"/version":{"op":"decrement"}}]])");  // none before
  assertEqual(read(R"([["version"]])"), R"( [{"version":-1}])");
  writeAndCheck(R"([[{"/version":{"op":"decrement"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"( [{"version":-2}])");
  writeAndCheck(
      R"([[{"/version":{"op":"set", "new": {"c":12}, "ttl":3}}]])");  // int
                                                                      // before
  assertEqual(read(R"([["version"]])"), R"( [{"version":{"c":12}}])");
  writeAndCheck(R"([[{"/version/c":{"op":"decrement"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"( [{"version":{"c":11}}])");
  std::this_thread::sleep_for(std::chrono::milliseconds{3100});
  assertEqual(read(R"([["version"]])"), R"( [{}])");
  writeAndCheck(R"([[{"/version/c":{"op":"decrement"}}]])");  // int before
  assertEqual(read(R"([["version"]])"), R"( [{"version":{"c":-1}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "op" keyword in other places than as operator
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, op_in_strange_places) {
  writeAndCheck(R"([[{"/op":12}]])");
  assertEqual(read(R"([["/op"]])"), R"( [{"op":12}])");
  writeAndCheck(R"([[{"/op":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"/op/a/b/c":{"op":"set","new":{"op":13}}}]])");
  assertEqual(read(R"([["/op/a/b/c"]])"),
              R"( [{"op":{"a":{"b":{"c":{"op":13}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/c/op":{"op":"increment"}}]])");
  assertEqual(read(R"([["/op/a/b/c"]])"),
              R"( [{"op":{"a":{"b":{"c":{"op":14}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/c/op":{"op":"decrement"}}]])");
  assertEqual(read(R"([["/op/a/b/c"]])"),
              R"( [{"op":{"a":{"b":{"c":{"op":13}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/c/op":{"op":"pop"}}]])");
  assertEqual(read(R"([["/op/a/b/c"]])"),
              R"( [{"op":{"a":{"b":{"c":{"op":[]}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/c/op":{"op":"increment"}}]])");
  assertEqual(read(R"([["/op/a/b/c"]])"),
              R"( [{"op":{"a":{"b":{"c":{"op":1}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/c/op":{"op":"shift"}}]])");
  assertEqual(read(R"([["/op/a/b/c"]])"),
              R"( [{"op":{"a":{"b":{"c":{"op":[]}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/c/op":{"op":"decrement"}}]])");
  assertEqual(read(R"([["/op/a/b/c"]])"),
              R"( [{"op":{"a":{"b":{"c":{"op":-1}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/c/op":{"op":"push","new":-1}}]])");
  assertEqual(read(R"([["/op/a/b/c"]])"),
              R"( [{"op":{"a":{"b":{"c":{"op":[-1]}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/d":{"op":"set","new":{"ttl":14}}}]])");
  assertEqual(read(R"([["/op/a/b/d"]])"),
              R"( [{"op":{"a":{"b":{"d":{"ttl":14}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/d/ttl":{"op":"increment"}}]])");
  assertEqual(read(R"([["/op/a/b/d"]])"),
              R"( [{"op":{"a":{"b":{"d":{"ttl":15}}}}}])");
  writeAndCheck(R"([[{"/op/a/b/d/ttl":{"op":"decrement"}}]])");
  assertEqual(read(R"([["/op/a/b/d"]])"),
              R"( [{"op":{"a":{"b":{"d":{"ttl":14}}}}}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief op delete on top node
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, operators_on_root_node) {
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [{}])");
  writeAndCheck(R"([[{"/":{"op":"increment"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [1])");
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"/":{"op":"decrement"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [-1])");
  writeAndCheck(R"([[{"/":{"op":"push","new":"Hello"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [["Hello"]])");
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"/":{"op":"push","new":"Hello"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [["Hello"]])");
  writeAndCheck(R"([[{"/":{"op":"pop"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [[]])");
  writeAndCheck(R"([[{"/":{"op":"pop"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [[]])");
  writeAndCheck(R"([[{"/":{"op":"push","new":"Hello"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [["Hello"]])");
  writeAndCheck(R"([[{"/":{"op":"shift"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [[]])");
  writeAndCheck(R"([[{"/":{"op":"shift"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [[]])");
  writeAndCheck(R"([[{"/":{"op":"prepend","new":"Hello"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [["Hello"]])");
  writeAndCheck(R"([[{"/":{"op":"shift"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [[]])");
  writeAndCheck(R"([[{"/":{"op":"pop"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [[]])");
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [[]])");
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  assertEqual(read(R"([["/"]])"), R"( [[]])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test that order should not matter
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, order) {
  writeAndCheck(R"([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]])");
  assertEqual(read(R"([["a/e"],[ "d","a/b"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"d":false, "a":{"b":{"c":[1,2,3]},"e":12}}]])");
  assertEqual(read(R"([["a/e"],[ "d","a/b"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
  writeAndCheck(R"([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]])");
  assertEqual(read(R"([["a/e"],[ "d","a/b"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
  writeAndCheck(R"([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]])");
  assertEqual(read(R"([["a/e"],["a/b","d"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test nasty willful attempt to break
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, order_evil) {
  writeAndCheck(R"([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]])");
  assertEqual(read(R"([["a/e"],[ "d","a/b"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"d":false, "a":{"b":{"c":[1,2,3]},"e":12}}]])");
  assertEqual(read(R"([["a/e"],[ "d","a/b"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
  writeAndCheck(R"([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]])");
  assertEqual(read(R"([["a/e"],[ "d","a/b"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
  writeAndCheck(R"([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]])");
  assertEqual(read(R"([["a/e"],["a/b","d"]])"),
              R"([{"a":{"e":12}},{"a":{"b":{"c":[1,2,3]}},"d":false}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Test nasty willful attempt to break
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, slash_o_rama) {
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"//////////////////////a/////////////////////b//":
                    {"b///////c":4}}]])");
  assertEqual(read(R"([["/"]])"), R"( [{"a":{"b":{"b":{"c":4}}}}])");
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  writeAndCheck(R"([[{"////////////////////////": "Hi there!"}]])");
  assertEqual(read(R"([["/"]])"), R"(["Hi there!"])");
  writeAndCheck(R"([[{"/":{"op":"delete"}}]])");
  writeAndCheck(
      R"([[{"/////////////////\\/////a/////////////^&%^&$^&%$////////b\\\n//":
        {"b///////c":4}}]])");
  assertEqual(read(R"([["/"]])"),
              R"([{"\\":{"a":{"^&%^&$^&%$":{"b\\\n":{"b":{"c":4}}}}}}])");
}

TEST_F(StoreTestAPI, keys_beginning_with_same_string) {
  writeAndCheck(
      R"([[{"/bumms":{"op":"set","new":"fallera"}, "/bummsfallera": {"op":"set","new":"lalalala"}}]])");
  assertEqual(read(R"([["/bumms", "/bummsfallera"]])"),
              R"( [{"bumms":"fallera", "bummsfallera": "lalalala"}])");
}

TEST_F(StoreTestAPI, hidden_agency_write) {
  auto res = write(R"([[{".agency": {"op":"set","new":"fallera"}}]])");
  ASSERT_EQ(res.front(), consensus::apply_ret_t::FORBIDDEN);
}

TEST_F(StoreTestAPI, hidden_agency_write_slash) {
  auto res = write(R"([[{"/.agency": {"op":"set","new":"fallera"}}]])");
  ASSERT_EQ(res.front(), consensus::apply_ret_t::FORBIDDEN);
}

TEST_F(StoreTestAPI, hidden_agency_write_deep) {
  auto res = write(R"([[{"/.agency/hans": {"op":"set","new":"fallera"}}]])");
  ASSERT_EQ(res.front(), consensus::apply_ret_t::FORBIDDEN);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, huge_transaction_package) {
  writeAndCheck(R"([[{"a":{"op":"delete"}}]])");  // cleanup first
  std::stringstream ss;
  ss << "[";
  bool first{true};
  for (int i{}; i < 20000; ++i) {
    if (first)
      first = false;
    else
      ss << ",";
    ss << R"([{"a":{"op":"increment"}}, {}, "huge)" << i << R"("])";
  }
  ss << "]";
  writeAndCheck(ss.str());
  assertEqual(read(R"([["a"]])"), R"([{"a":20000}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package, inc/dec
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, transaction_with_inc_dec) {
  writeAndCheck(R"([[{"a":{"op":"delete"}}]])");  // cleanup first
  std::stringstream ss;
  int i{};
  ss << R"([[{"a":{"op":"increment"}}, {}, "inc)" << i << "\"]";
  ss << R"(,[{"a":{"op":"decrement"}}, {}, "dec)" << i << "\"]";
  for (++i; i < 100; ++i) {
    ss << R"(,[{"a":{"op":"increment"}}, {}, "inc)" << i << "\"]";
    ss << R"(,[{"a":{"op":"decrement"}}, {}, "dec)" << i << "\"]";
  }
  ss << "]";
  writeAndCheck(ss.str());
  assertEqual(read(R"([["a"]])"), R"( [{"a":0}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction, update of same key
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, transaction_update_same_key) {
  writeAndCheck(R"([[{"a":{"op":"delete"}}]])");  // cleanup first
  writeAndCheck(R"([[{"a": "foo"}],[{"a":"bar"}]])");
  assertEqual(read(R"([["a"]])"), R"( [{"a":"bar"}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction, insert and remove of same key
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, transaction_insert_remove_same_key) {
  writeAndCheck(R"([[{"a":{"op":"delete"}}]])");  // cleanup first
  writeAndCheck(R"([[{"a":"foo"}],[{"a":{"op":"delete"}}]])");
  assertEqual(read(R"([["/a"]])"), R"( [{}])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package, all different keys
////////////////////////////////////////////////////////////////////////////////

TEST_F(StoreTestAPI, transaction_different_keys) {
  std::stringstream ss;
  int i{};
  ss << R"([[{"a)" << i << R"(":{"op":"increment"}}, {}, "diff)" << i << "\"]";
  for (++i; i < 100; ++i) {
    ss << R"(,[{"a)" << i << R"(":{"op":"increment"}}, {}, "diff)" << i
       << "\"]";
  }
  ss << "]";
  writeAndCheck(ss.str());
  for (i = 0; i < 100; ++i) {
    assertEqual(read(R"([["a)" + std::to_string(i) + "\"]]"),
                R"([{"a)" + std::to_string(i) + R"(":1}])");
  }
}

}  // namespace store_test_api
}  // namespace tests
}  // namespace arangodb
