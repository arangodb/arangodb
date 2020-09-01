////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Cluster maintenance
///
/// @file
///
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "RestServer/Metrics.h"

#include <atomic>
#include <thread>
#include <vector>

namespace {
constexpr size_t numThreads = 4;
constexpr uint64_t numOpsPerThread = 25 * 1000 * 1000;
}

class MetricsTest : public ::testing::Test {
protected:
  MetricsTest () {}
};

TEST_F(MetricsTest, test_counter_concurrency) {
  Counter c(0, "counter", "Counter");

  ASSERT_EQ(c.load(),  0);

  std::atomic<bool> go = false;

  std::vector<std::thread> threads;
  threads.reserve(::numThreads);
  for (size_t i = 0; i < ::numThreads; ++i) {
    threads.emplace_back([&]() {
      while (!go.load()) {
        // wait until all threads are created, so they can 
        // start at the approximate same time
      }
      for (uint64_t i = 0; i < ::numOpsPerThread; ++i) {
        ++c;
      }
    });
  }

  go.store(true);
  
  for (auto& thread : threads) {
    thread.join();
  }
  
  ASSERT_EQ(c.load(), ::numThreads * ::numOpsPerThread);
}

TEST_F(MetricsTest, test_histogram_concurrency_same) {
  Histogram h(4, 1, 100, "histogram", "Histogram");

  ASSERT_EQ(h.load(0), 0);
  ASSERT_EQ(h.load(1), 0);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
  
  std::atomic<bool> go = false;

  std::vector<std::thread> threads;
  threads.reserve(::numThreads);
  for (size_t i = 0; i < ::numThreads; ++i) {
    threads.emplace_back([&]() {
      while (!go.load()) {
        // wait until all threads are created, so they can 
        // start at the approximate same time
      }
      for (uint64_t i = 0; i < ::numOpsPerThread; ++i) {
        h.count(1);
      }
    });
  }
  
  go.store(true);
  
  for (auto& thread : threads) {
    thread.join();
  }
  
  ASSERT_EQ(h.load(0), ::numThreads * ::numOpsPerThread);
  ASSERT_EQ(h.load(1), 0);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
}

TEST_F(MetricsTest, test_histogram_concurrency_distributed) {
  Histogram h(4, 1, 100, "histogram", "Histogram");

  ASSERT_EQ(h.load(0), 0);
  ASSERT_EQ(h.load(1), 0);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
  
  std::atomic<bool> go = false;

  std::vector<std::thread> threads;
  threads.reserve(::numThreads);
  for (size_t i = 0; i < ::numThreads; ++i) {
    threads.emplace_back([&](uint64_t value) {
      while (!go.load()) {
        // wait until all threads are created, so they can 
        // start at the approximate same time
      }
      for (uint64_t i = 0; i < ::numOpsPerThread; ++i) {
        h.count(value);
      }
    }, i * 30);
  }
  
  go.store(true);
  
  for (auto& thread : threads) {
    thread.join();
  }
  
  ASSERT_EQ(h.load(0), ::numOpsPerThread);
  ASSERT_EQ(h.load(1), (::numThreads > 1 ? 1 : 0) * ::numOpsPerThread);
  ASSERT_EQ(h.load(2), (::numThreads > 2 ? 1 : 0) * ::numOpsPerThread);
  ASSERT_EQ(h.load(3), (::numThreads > 3 ? (::numThreads - 3) : 0) * ::numOpsPerThread);
}

TEST_F(MetricsTest, test_histogram_simple) {
  Histogram h(4, 1, 100, "histogram", "Histogram");

  ASSERT_EQ(h.load(0), 0);
  ASSERT_EQ(h.load(1), 0);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);

  h.count(0);
  ASSERT_EQ(h.load(0), 1);
  ASSERT_EQ(h.load(1), 0);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
  
  h.count(0);
  ASSERT_EQ(h.load(0), 2);
  ASSERT_EQ(h.load(1), 0);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
  
  h.count(1);
  ASSERT_EQ(h.load(0), 3);
  ASSERT_EQ(h.load(1), 0);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
  
  h.count(1);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 0);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
  
  h.count(30);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 1);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
  
  h.count(30);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 2);
  ASSERT_EQ(h.load(2), 0);
  ASSERT_EQ(h.load(3), 0);
  
  h.count(60);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 2);
  ASSERT_EQ(h.load(2), 1);
  ASSERT_EQ(h.load(3), 0);
  
  h.count(60);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 2);
  ASSERT_EQ(h.load(2), 2);
  ASSERT_EQ(h.load(3), 0);
  
  h.count(90);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 2);
  ASSERT_EQ(h.load(2), 2);
  ASSERT_EQ(h.load(3), 1);
  
  h.count(90);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 2);
  ASSERT_EQ(h.load(2), 2);
  ASSERT_EQ(h.load(3), 2);
  
  h.count(10000);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 2);
  ASSERT_EQ(h.load(2), 2);
  ASSERT_EQ(h.load(3), 3);
  
  h.count(10000000);
  ASSERT_EQ(h.load(0), 4);
  ASSERT_EQ(h.load(1), 2);
  ASSERT_EQ(h.load(2), 2);
  ASSERT_EQ(h.load(3), 4);
}


TEST_F(MetricsTest, test_counter) {

  Counter c(0, "counter_1", "Counter 1");

  ASSERT_EQ(c.load(),  0);
  c++;
  ASSERT_EQ(c.load(),  1);
  c += 9;
  ASSERT_EQ(c.load(), 10);
  c =  0;
  ASSERT_EQ(c.load(),  0);

  c.count();
  ASSERT_EQ(c.load(),  1);
  c.count(  9);
  ASSERT_EQ(c.load(), 10);
  c.store(0);
  ASSERT_EQ(c.load(),  0);

  std::string s;
  c.toPrometheus(s);
  LOG_DEVEL << s;

}

template<typename T> void gauge_test() {

  T zdo = .1, zero = 0., one = 1.;
  Gauge g(zero, "gauge_1", "Gauge 1");

  ASSERT_DOUBLE_EQ(g.load(),  zero);
  g += zdo;
  ASSERT_DOUBLE_EQ(g.load(),  zdo);
  g -= zdo;
  ASSERT_DOUBLE_EQ(g.load(),  zero);
  g += zdo;
  g *= g.load();
  ASSERT_DOUBLE_EQ(g.load(),  zdo*zdo);
  g /= g.load();
  ASSERT_DOUBLE_EQ(g.load(),  one);
  g -= g.load();
  ASSERT_DOUBLE_EQ(g.load(),  zero);

  std::string s;
  g.toPrometheus(s);
  LOG_DEVEL << s;

}

TEST_F(MetricsTest, long_double) {
  gauge_test<long double>();
}

TEST_F(MetricsTest, test_gauge_double) {
  gauge_test<double>();
}

TEST_F(MetricsTest, test_gauge_float) {
  gauge_test<float>();
}

template<typename T> void histogram_test(
  size_t const& buckets, T const& mn, T const& mx) {

  T span = mx - mn,  step = span/(T)buckets,
    mmin = (std::is_floating_point<T>::value) ? span / T(1.e6) : T(1),
    one = T(1), ten = T(10), thousand(10);
  Histogram h(buckets, mn, mx, "hist_test", "Hist test");

  //lower bucket bounds
  for (size_t i = 0; i < buckets; ++i) {
    auto d = mn + step*i + mmin;
    h.count(d);
    ASSERT_DOUBLE_EQ(h.load(i), 1);
  }

  //upper bucket bounds
  for (size_t i = 0; i < buckets; ++i) {
    auto d = mn + step*(i+1) - mmin;
    h.count(d);
    ASSERT_DOUBLE_EQ(h.load(i), 2);
  }

  //below lower limit
  h.count(mn - one);
  h.count(mn - ten);
  h.count(mn - thousand);
  ASSERT_DOUBLE_EQ(h.load(0), 5);

  // above upper limit
  h.count(mx + one);
  h.count(mx + ten);
  h.count(mx + thousand);
  ASSERT_DOUBLE_EQ(h.load(buckets-1), 5);

  // dump
  std::string s;
  h.toPrometheus(s);
  LOG_DEVEL << s;

}


TEST_F(MetricsTest, test_float_histogram) {
  histogram_test<long double>( 9,  1.,  2.);
  histogram_test<long double>(10, -1.,  1.);
  histogram_test<long double>( 8, -2., -1.);
}
TEST_F(MetricsTest, test_double_histogram) {
  histogram_test<long double>(12,  1.,  2.);
  histogram_test<long double>(11, -1.,  1.);
  histogram_test<long double>(13, -2., -1.);
}
TEST_F(MetricsTest, test_long_double_histogram) {
  histogram_test<long double>(12,  1.,  2.);
  histogram_test<long double>(17, -1.,  1.);
  histogram_test<long double>( 7, -2., -1.);
}
TEST_F(MetricsTest, test_short_histogram) {
  histogram_test<short>(6, -17, 349);
  histogram_test<short>(7,  20,  40);
  histogram_test<short>(8, -63, -11);
}
TEST_F(MetricsTest, test_int_histogram) {
  histogram_test<short>(6, -17, 349);
  histogram_test<short>(7,  20,  40);
  histogram_test<short>(8, -63, -11);
}
TEST_F(MetricsTest, test_long_histogram) {
  histogram_test<short>(6, -17, 349);
  histogram_test<short>(7,  20,  40);
  histogram_test<short>(8, -63, -11);
}
