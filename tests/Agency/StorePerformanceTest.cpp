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
/// @author Ignacio Rodriguez
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Agency/Store.h"
#include "Mocks/Servers.h"
#include "fakeit.hpp"

#include <velocypack/Builder.h>
#include <velocypack/Compare.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace tests {
namespace store_performance_test {

class StorePerformanceTest : public ::testing::Test {
 public:
  StorePerformanceTest() : _server(), _store(_server.server(), nullptr) {}

 protected:
  arangodb::tests::mocks::MockCoordinator _server;
  arangodb::consensus::Store _store;

  std::shared_ptr<VPackBuilder> read(std::string const& json) {
    try {
      consensus::query_t q{VPackParser::fromJson(json)};
      auto result{std::make_shared<VPackBuilder>()};
      _store.read(q, result);
      return result;
    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) +
                               " while trying to read " + json);
    }
  }

  auto write(std::string const& json) {
    try {
      auto q{VPackParser::fromJson(json)};
      return _store.applyTransactions(q);
    } catch (std::exception& err) {
      throw std::runtime_error(std::string(err.what()) + " while parsing " + json);
    }
  }

  template <typename TOstream, typename TValueContainer>
  static TOstream& insert_value_array(TOstream& out, TValueContainer const& src) {
    out << "[";
    if (!src.empty()) {
      std::copy(src.begin(), src.end() - 1,
                std::ostream_iterator<std::string>(out, ", "));
      out << src.back();
    }
    out << "]";
    return out;
  }

  std::vector<consensus::apply_ret_t> write(std::vector<std::vector<std::string>> const& operations) {
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

  std::vector<consensus::apply_ret_t> transactAndCheck(std::string const& json) {
    try {
      auto q{VPackParser::fromJson(json)};
      auto results{_store.applyTransactions(q)};
      return results;

    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) +
                               ", transact failed processing " + json);
    }
  }

  void writeAndCheck(std::string const& json) {
    try {
      auto r{write(json)};
      auto applied_all = std::all_of(r.begin(), r.end(), [](auto const& result) {
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
      if (!velocypack::NormalizedCompare::equals(result->slice(), expected->slice())) {
        throw std::runtime_error(
            result->toJson() + " should have been equal to " + expected->toJson());
      }
    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) + " comparing to " + expected_result);
    }
  }

  template <typename TSource>
  static std::string to_json_object(
      TSource const& src,
      std::function<std::string(typename TSource::const_reference)> extractName,
      std::function<std::string(typename TSource::const_reference)> extractValue) {
    bool is_first{true};
    return "{" +
           std::accumulate(src.begin(), src.end(), std::string(),
                           [&is_first, extractName,
                            extractValue](std::string partial, auto const& element) {
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

  template <typename TSource>
  static std::string to_json_object(TSource const& src) {
    return to_json_object(
        src, [](auto const& element) { return element.first; },
        [](auto const& element) { return element.second; });
  }
};

// hammer a single key deep in the hierarchy 100000 times or so.
//  We test long paths.
//  Performance relative to depth.

TEST_F(StorePerformanceTest, single_deep_key_writes) {
  std::map<std::string, std::string> obj{};
  for (int i{}; i < 100000; ++i) {
    obj["a/b/c/d/e/f/g/h"] = std::to_string(i);
    write(std::vector<std::vector<std::string>>{{to_json_object(obj)}});
  }
}

TEST_F(StorePerformanceTest, single_deep_key_writes_and_reads) {
  std::map<std::string, std::string> obj{};
  auto const key {"a/b/c/d/e/f/g/h"};
  auto const json_read_key {std::string("[[\"") + key + "\"]]"};
  for (int i{}; i < 100000; ++i) {
    obj[key] = std::to_string(i);
    write(std::vector<std::vector<std::string>>{{to_json_object(obj)}});
    auto result {read(json_read_key)};
  }
}

TEST_F(StorePerformanceTest, single_deep_key_reads) {
  std::map<std::string, std::string> obj{};
  auto const key {"a/b/c/d/e/f/g/h"};
  write(std::vector<std::vector<std::string>>{{to_json_object(obj)}});
  auto const json_read_key {std::string("[[\"") + key + "\"]]"};
  for (int i{}; i < 100000; ++i) {
    obj[key] = std::to_string(i);
    auto result {read(json_read_key)};
  }
}

}  // namespace store_performance_test
}  // namespace tests
}  // namespace arangodb
