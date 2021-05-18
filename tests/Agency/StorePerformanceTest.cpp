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
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Agency/Store.h"
#include "Mocks/Servers.h"
#include "fakeit.hpp"

#include <velocypack/Builder.h>
#include <velocypack/Compare.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <vector>
#include <string>
#include <string_view>
#include <functional>

namespace arangodb {
namespace tests {
namespace store_performance_test {

const std::array<size_t, 4> repetition_times {100, 1500, 5000, 20000};

struct OperationMeasurement 
{
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;
  using DurationIterator = std::vector<Duration>::iterator;
  using InformValue = std::function<void(Duration)>;
  using StatExtractor = std::function<void(DurationIterator,DurationIterator,InformValue)>;
  using Observation = Clock::time_point;
  using Observations = std::vector<Observation>;
  
  OperationMeasurement(size_t expected_count = 0u) {
    observations_.reserve(expected_count + 2);
    ++*this;
  }

  ~OperationMeasurement() {
    stop();
  }

  void stop() {
    if (!stopped_) {
      if (observations_.size() < 2) {
        // we must be measuring a single long operation
        ++*this;
      }
      stopped_ = true;
    }
  }

  OperationMeasurement(OperationMeasurement const&) = delete;
  OperationMeasurement(OperationMeasurement&) = delete;
  OperationMeasurement(OperationMeasurement&&) = delete;

  OperationMeasurement& operator++() {
    observations_.push_back(Clock::now());
    return *this;
  }

  void report() 
  {
    stop();
    static std::array<std::pair<std::string_view, StatExtractor>, 5> duration_stats = {{
      {"max", [](DurationIterator begin, DurationIterator end, InformValue inform) { inform (*std::max_element(begin,end)); }},
      {"min", [](DurationIterator begin, DurationIterator end, InformValue inform) { inform (*std::min_element(begin,end)); }},
      {"avg", [](DurationIterator begin, DurationIterator end, InformValue inform) { 
        if (begin != end) { 
          inform (std::accumulate(begin, end, OperationMeasurement::Clock::duration::zero()) / std::distance(begin, end));
        }
      }},
      {"med", [](DurationIterator begin, DurationIterator end, InformValue inform) { 
        if (begin != end) { 
          std::vector<Duration> copy {begin, end};
          std::sort(copy.begin(), copy.end());
          inform (copy[copy.size() / 2]);
        }
      }},
      {"max10", [](DurationIterator begin, DurationIterator end, InformValue inform) { 
        if (begin != end) { 
          std::vector<Duration> copy {begin, end};
          std::sort(copy.begin(), copy.end(), [](Duration const& a, Duration const& b){ return b < a; });
          copy.resize(std::min(10ul, copy.size()));
          std::for_each(copy.begin(), copy.end(), inform);
        }
      }},
    }};

    std::vector<OperationMeasurement::Clock::duration> durations;
    auto begin{observations_.begin()};
    auto from {*begin};
    std::transform(++begin, observations_.end(), std::back_inserter(durations), [&from](auto const point){ 
      auto d {point - from};
      from = point;
      return d;
    });

    auto inform {[](Duration d){ std::cout << std::setw(10) << d.count() << "ns ";}};
    for (auto const& stat: duration_stats) {
      std::cout << std::setw(5) << stat.first << ": ";
      stat.second(durations.begin(), durations.end(), inform);
      std::cout << std::endl;
    }
  }

private:
  Observations observations_;
  bool stopped_{};
};

class StorePerformanceTest : public ::testing::Test {
 public:
  StorePerformanceTest() : _server(), _store(_server.server(), nullptr) {}

 protected:
  arangodb::tests::mocks::MockCoordinator _server;
  arangodb::consensus::Store _store;

  std::shared_ptr<VPackBuilder> read(std::string const& json) {
    return read(VPackParser::fromJson(json));
  }

  std::shared_ptr<VPackBuilder> read(consensus::query_t const& q) {
    try {
      auto result{std::make_shared<VPackBuilder>()};
      _store.read(q, result);
      return result;
    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) +
                               " while trying to read " + q->toJson());
    }
  }

  std::vector<consensus::apply_ret_t> write(consensus::query_t const& query) {
    try {
      return _store.applyTransactions(query);
    } catch (std::exception& err) {
      throw std::runtime_error(std::string(err.what()) + " while parsing " + query->toJson());
    }
  }

  std::vector<consensus::apply_ret_t> write(std::string const& json) {
    auto q{VPackParser::fromJson(json)};
    return write(q);
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

  void writeAndCheck(std::string const& query) {
    try {
      auto r{write(query)};
      auto applied_all = std::all_of(r.begin(), r.end(), [](auto const& result) {
        return result == consensus::apply_ret_t::APPLIED;
      });
      ASSERT_TRUE(applied_all);
      if (!applied_all) {
        throw std::runtime_error("Not all applied");
      }
    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) + " processing " + query);
    }
  }

  void writeAndCheck(consensus::query_t const& query) {
    try {
      auto r{write(query)};
      auto applied_all = std::all_of(r.begin(), r.end(), [](auto const& result) {
        return result == consensus::apply_ret_t::APPLIED;
      });
      ASSERT_TRUE(applied_all);
      if (!applied_all) {
        throw std::runtime_error("Not all applied");
      }
    } catch (std::exception& ex) {
      throw std::runtime_error(std::string(ex.what()) + " processing " + query->toJson());
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
  std::vector<consensus::query_t> write_queries;
  std::generate_n(std::back_inserter(write_queries), repetition_times[3], [](){
    static size_t i{};
    return VPackParser::fromJson("[[{\"a/b/c/d/e/f/g/h\":" + std::to_string(i++) + "}]]");
  });
  OperationMeasurement op{repetition_times[3]};
  for (auto const& q: write_queries) {
    _store.applyTransactions(q);
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, single_deep_key_writes_and_reads) {
  std::vector<consensus::query_t> write_queries;
  std::generate_n(std::back_inserter(write_queries), repetition_times[3], [](){
    static size_t i{};
    return VPackParser::fromJson("[[{\"a/b/c/d/e/f/g/h\":" + std::to_string(i++) + "}]]");
  });
  auto const read_query {VPackParser::fromJson("[[\"a/b/c/d/e/f/g/h\"]]")};
  OperationMeasurement op{repetition_times[3]};
  for (auto const& q: write_queries) {
    _store.applyTransactions(q);
    read(read_query);
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, single_deep_key_reads) {
  write(R"([[{"a/b/c/d/e/f/g/h": 42}]])");
  auto const read_query {VPackParser::fromJson("[[\"a/b/c/d/e/f/g/h\"]]")};
  OperationMeasurement op{repetition_times[3]};
  for (size_t i{}; i < repetition_times[3]; ++i) {
    auto result {read(read_query)};
    ++op;
  }
  op.report();
}

// write lots of different keys in different places
//  Random-access performance

static std::string rand_path(const size_t len = 10u) {
  static auto const alphabet {"abcdefghijklmnopqrstuvwxyz0123456789"};
  std::string result;
  std::generate_n(std::back_inserter(result), 1 + std::rand() % (len-1), [](){
    return alphabet[std::rand() % (sizeof(alphabet)-1)];
  });
  return result;
}

#include <iostream>

TEST_F(StorePerformanceTest, scattered_keys_w) {
  std::vector<consensus::query_t> write_queries;
  std::generate_n(std::back_inserter(write_queries), repetition_times[3], [](){
    auto const key {rand_path()};
    auto const json_object {"[[{\"" + key + "\": " + std::to_string(rand()) + "}]]"};
    return VPackParser::fromJson(json_object);
  });
  OperationMeasurement op{repetition_times[3]};
  for (auto& q: write_queries) {
    auto result {write(q)};
    ASSERT_EQ(result.front(), consensus::apply_ret_t::APPLIED);
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, scattered_keys_wr) {
  std::vector<std::string> keys;
  std::generate_n(std::back_inserter(keys), repetition_times[1], [](){ return rand_path(); });
  for (auto const& key: keys) {
    writeAndCheck("[[{\"" + key + "\":1}]]");
  }
  std::vector<consensus::query_t> read_queries;
  std::generate_n(std::back_inserter(read_queries), repetition_times[1], [&keys](){ 
    return VPackParser::fromJson("[[\"" + keys[std::rand() % keys.size()] + "\"]]");
  });
  OperationMeasurement op{repetition_times[1]};
  for (auto const& rq: read_queries) {
    auto result {read(rq)};
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, scattered_keys_wwr) {
  std::vector<std::string> keys;
  std::generate_n(std::back_inserter(keys), repetition_times[1], [](){ return rand_path(20); });
  for (auto const& key: keys) {
    auto const json_object {"{\"" + key + "\": 1}"};
    auto result {write(std::vector<std::vector<std::string>>{{json_object}})};
    ASSERT_EQ(result.front(), consensus::apply_ret_t::APPLIED);
  }
  for (auto const& key: keys) {
    auto const json_object {"[[{\"" + key + "\": {\"op\": \"increment\"}}]]"};
    auto result {write(json_object)};
    if (result.front() != consensus::apply_ret_t::APPLIED) {
      throw std::runtime_error(json_object + " could not be applied: " + std::to_string(result.front()));
    }
  }
  std::vector<consensus::query_t> read_queries;
  std::generate_n(std::back_inserter(read_queries), repetition_times[1], [&keys](){ 
    return VPackParser::fromJson("[[\"" + keys[std::rand() % keys.size()] + "\"]]");
  });
  OperationMeasurement op{repetition_times[1]};
  for (auto const& rq: read_queries) {
    auto result {read(rq)};
    ++op;
  }
  op.report();
}

// do a lot of small transactions
// Performance in situations that should smartly use caches and primary memory.
TEST_F(StorePerformanceTest, small_tx_r) {
  writeAndCheck("[[{\"a\": 1}]]");
  writeAndCheck("[[{\"b/b/c\": 2}]]");
  writeAndCheck("[[{\"d\": 3}]]");
  auto rq {VPackParser::fromJson("[[\"a\"], [\"b/b/c\"], [\"d\"]]")};
  OperationMeasurement op{repetition_times[3]};
  for (size_t i{}; i < repetition_times[3]; ++i) {
    auto const result {read(rq)};
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, small_tx_w) {
  std::vector<consensus::query_t> write_queries;
  for (size_t i{}; i < repetition_times[3]; ++i) {
    auto const n {std::to_string(i)};
    write_queries.push_back(VPackParser::fromJson( "[[{\"a\": " + 
      n +"}],[{\"a/b/c\": " + 
      n +"}],[{\"d\": " + 
      n +"}]]"));
  }
  OperationMeasurement op{repetition_times[3]};
  for (auto const& wq: write_queries){
    writeAndCheck(wq);
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, small_tx_rw) {
  for (size_t i{}; i < repetition_times[3]; ++i) {
    writeAndCheck("[[{\"a\": " + std::to_string(i) +"}]]");
    writeAndCheck("[[{\"b/b/c\": " + std::to_string(i) +"}]]");
    writeAndCheck("[[{\"d\": " + std::to_string(i) +"}]]");
  }
  auto const rq { VPackParser::fromJson("[[\"a\"], [\"a/b/c\"], [\"d\"]]") };
  OperationMeasurement op{repetition_times[3]};
  for (size_t i{}; i < repetition_times[3]; ++i) {
    auto const result {read(rq)};
    ++op;
  }
  op.report();
}

// do fewer, but larger transactions
// Situations where primary memory and caches normally punish performance.

TEST_F(StorePerformanceTest, bigger_tx_r) {
  {
    std::stringstream ss;
    ss << "[[{\"k0\": 0}]";
    for (size_t i{1}; i < repetition_times[0]; ++i) {
      ss << ",[{\"k" << i << "\": " << i << "}]";
    }
    ss << "]";
    writeAndCheck(ss.str());
  }
  std::stringstream ss;
  ss << "[[";
  ss << "\"k0\"";
  for (size_t i{1}; i < repetition_times[0]; ++i) {
    ss << ",\"k" << i << "\"";
  }
  ss << "]]";
  auto const key_list {VPackParser::fromJson(ss.str())};

  OperationMeasurement op{repetition_times[0]};
  for (size_t i{}; i < repetition_times[0]; ++i) {
    auto const result {read(key_list)};
    // ASSERT_EQ(result->size(), result->size());
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, bigger_tx_w) {
  std::stringstream ss;
  ss << "[[{\"k0\": 0}]";
  for (size_t i{1}; i < repetition_times[0]; ++i) {
    ss << ",[{\"k" << i << "\": " << i << "}]";
  }
  ss << "]";
  auto const write_string {VPackParser::fromJson(ss.str())};
  OperationMeasurement op{repetition_times[0]};
  for (size_t i{}; i < repetition_times[0]; ++i) {
    write(write_string);
    ++op;
  }
  op.report();
}

// test array operations specifically

TEST_F(StorePerformanceTest, array_push) {
  writeAndCheck(R"([[{"/a/b/c":[]}]])");
  std::vector<consensus::query_t> write_queries;
  size_t i {};
  std::generate_n(std::back_inserter(write_queries), repetition_times[1], [&i](){
    return VPackParser::fromJson(R"([[{"/a/b/c":{"op":"push","new":)" + std::to_string(i++) + "}}]]");
  });
  OperationMeasurement op{repetition_times[1]};
  for (auto const& wq: write_queries) {
    writeAndCheck(wq);
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, array_pop) {
  std::stringstream ss;
  ss << R"([[{"/a/b/c":[0)";
  for (size_t i{1}; i < repetition_times[1]; ++i) {
    ss << "," << i;
  }
  ss << "]}]]";
  writeAndCheck(ss.str());
  auto const rq {VPackParser::fromJson(R"([[{"/a/b/c":{"op":"pop"}}]])")};
  OperationMeasurement op{repetition_times[1]};
  for (size_t i{}; i < repetition_times[1]; ++i) {
    writeAndCheck(rq);
    ++op;
  }
  op.report();
}

// test object operations specifically

// test operations which need to change a lot in the tree
// add in ascending order
TEST_F(StorePerformanceTest, tree_add_ascending) {
  std::vector<consensus::query_t> write_queries;
  auto const width {log10(repetition_times[3])};
  for (size_t i{}; i < repetition_times[3]; ++i) {
    auto k {std::to_string(i)};
    k = std::string(width - k.size(), '0') + k;
    write_queries.push_back(VPackParser::fromJson("[[{\"k"+k+"\": 42}]]"));
  }
  OperationMeasurement op{repetition_times[3]};
  for (auto const& wq: write_queries) {
    write(wq);
    ++op;
  }
  op.report();
}

// add in descending order
TEST_F(StorePerformanceTest, tree_add_descending) {
  std::vector<consensus::query_t> write_queries;
  auto const width {log10(repetition_times[3])};
  for (auto i{repetition_times[3]}; i > 0; --i) {
    auto k {std::to_string(i)};
    k = std::string(width - k.size(), '0') + k;
    write_queries.push_back(VPackParser::fromJson("[[{\"k"+k+"\": 42}]]"));
  }
  OperationMeasurement op{repetition_times[3]};
  for (auto const& wq: write_queries) {
    write(wq);
    ++op;
  }
  op.report();


}

// add ascending, remove root, then re-add
TEST_F(StorePerformanceTest, tree_add_remove_readd) {
  std::vector<consensus::query_t> write_queries;
  auto const width {log10(repetition_times[3])};
  for (size_t i{}; i < repetition_times[3]; ++i) {
    auto k {std::to_string(i)};
    k = std::string(width - k.size(), '0') + k;
    write_queries.push_back(VPackParser::fromJson("[[{\"k"+k+"\": 42}]]"));
  }
  OperationMeasurement op{repetition_times[3]};
  for (auto const& wq: write_queries) {
    write(wq);
    ++op;
  }
  write("[[{\"/t\": 0}]]");
  for (auto const& wq: write_queries) {
    write(wq);
    ++op;
  }
  op.report();
}

// test for contention:
// multiple threads manipulate values
TEST_F(StorePerformanceTest, multiple_threads_all_separate_keys) {
  std::vector<std::thread> threads;
  OperationMeasurement op{repetition_times[3]};
  std::generate_n(std::back_inserter(threads), repetition_times[3], [this](){return std::thread([this](){
    auto const key {rand_path()};
    auto const json_object {"{\"" + key + "\": " + std::to_string(rand()) + "}"};
    auto result {write(std::vector<std::vector<std::string>>{{json_object}})};
    ASSERT_EQ(result.front(), consensus::apply_ret_t::APPLIED);
  });});
  for (auto& thread: threads) {
    thread.join();
    ++op;
  }
  op.report();
}

TEST_F(StorePerformanceTest, multiple_threads_high_concurrence) {
  std::vector<std::thread> threads;
  OperationMeasurement op{repetition_times[3]};
  std::generate_n(std::back_inserter(threads), repetition_times[3], [this](){return std::thread([this](){
    std::string const key {"k" + std::to_string(rand() %3)};
    auto const json_object {"{\"" + key + "\": " + std::to_string(rand()) + "}"};
    auto result {write(std::vector<std::vector<std::string>>{{json_object}})};
    ASSERT_EQ(result.front(), consensus::apply_ret_t::APPLIED);
  });});
  for (auto& thread: threads) {
    thread.join();
    ++op;
  }
  op.report();
}

}  // namespace store_performance_test
}  // namespace tests
}  // namespace arangodb
