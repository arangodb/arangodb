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
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "Basics/system-compiler.h"

#include "RestServer/Metrics.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iterator>
#include <random>
#include <thread>
#include <vector>

namespace {
constexpr size_t numThreads = 4;
constexpr uint64_t numOpsPerThread = 25 * 1000 * 1000;
}

TEST(MetricsTest, test_counter_concurrency) {
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

TEST(MetricsTest, test_histogram_concurrency_same) {
  lin_scale_t scale(1, 100, 4);
  Histogram h(scale, "histogram", "Histogram");

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

TEST(MetricsTest, test_histogram_concurrency_distributed) {
  lin_scale_t scale(1, 100, 4);
  Histogram h(scale, "histogram", "Histogram");

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
        h.count(static_cast<int>(value));
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

TEST(MetricsTest, test_histogram_simple) {
  lin_scale_t scale(1, 100, 4);
  Histogram h(scale, "histogram", "Histogram");

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


TEST(MetricsTest, test_counter) {
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

}

template<typename T> void gauge_test() {
  T zdo = T(.1);
  T zero = T(0.);
  T one = T(1.);
  Gauge g(zero, "gauge_1", "Gauge 1");

  using namespace std;
  using namespace std::chrono;

  random_device rnd_device;
  mt19937 mersenne_engine {rnd_device()};  // Generates random integers
  uniform_real_distribution<T> dist {T(1), T(100)};
  vector<T> vr(1000);
  auto gen = [&dist, &mersenne_engine](){
               return dist(mersenne_engine);
             };
  generate(vr.begin(), vr.end(), gen);

  size_t const p = 10;
  size_t const part = vr.size()/p;
  std::vector<std::future<void>> f;
  f.reserve(p);

  g = one;
  for (size_t i = 0; i < p; ++i) {
    f.push_back(
      async(launch::async,
            [&] {
              for (size_t j = 0; j < part; ++j) {
                g += vr.at(j);
                g -= vr.at(j);
              }}));
  }
  for (auto& i : f) {
    i.wait();
  }
  if constexpr (std::is_same<T,float>::value) {
    ASSERT_TRUE(std::abs(1.f - g.load()) < 1.e-3f);
  } else {
    ASSERT_TRUE(std::abs(1.f - g.load()) < 1.e-10f);
  }

  g = one;
  for (size_t i = 0; i < p; ++i) {
    f.push_back(
      async(launch::async,
            [&] {
              for (size_t j = 0; j < part; ++j) {
                g *= vr.at(j);
                g /= vr.at(j);
              }}));
  }
  for (auto& i : f) {
    i.wait();
  }
  if constexpr (std::is_same<T,float>::value) {
    ASSERT_TRUE(std::abs(1.f - g.load()) < 1.e-3f);
  } else {
    ASSERT_TRUE(std::abs(1.f - g.load()) < 1.e-10f);
  }

  if constexpr (std::is_same<T, float>::value) {
    g = zero;
    ASSERT_FLOAT_EQ(g.load(), zero);
    g += zdo;
    ASSERT_FLOAT_EQ(g.load(), zdo);
    g -= zdo;
    ASSERT_FLOAT_EQ(g.load(), zero);
    g += zdo;
    g *= g.load();
    ASSERT_FLOAT_EQ(g.load(), zdo * zdo);
    g /= g.load();
    ASSERT_FLOAT_EQ(g.load(), one);
    g -= g.load();
    ASSERT_FLOAT_EQ(g.load(), zero);
  } else if constexpr (std::is_same<T, double>::value) {
    g = zero;
    ASSERT_DOUBLE_EQ(g.load(), zero);
    g += zdo;
    ASSERT_DOUBLE_EQ(g.load(), zdo);
    g -= zdo;
    ASSERT_DOUBLE_EQ(g.load(), zero);
    g += zdo;
    g *= g.load();
    ASSERT_DOUBLE_EQ(g.load(), zdo * zdo);
    g /= g.load();
    ASSERT_DOUBLE_EQ(g.load(), one);
    g -= g.load();
    ASSERT_DOUBLE_EQ(g.load(), zero);
  } else {
    // should only be instantiating this class with double or float
    ASSERT_TRUE(false);
  }
}

TEST(MetricsTest, test_gauge_double) {
  gauge_test<double>();
}

TEST(MetricsTest, test_gauge_float) {
  gauge_test<float>();
}

TEST(MetricsTest, test_gauge_operations_uint64) {
  Gauge<uint64_t> g(0, "gauge", "Test gauge");

  ASSERT_EQ(0, g.load());

  // assignment
  g = 0;
  ASSERT_EQ(0, g.load());
  
  g = 123456;
  ASSERT_EQ(123456, g.load());

  g = 0;
  ASSERT_EQ(0, g.load());
  
  // prefix increment
  {
    ASSERT_EQ(1, (++g).load());
    ASSERT_EQ(1, g.load());
  }
  
  g = 42;
  ASSERT_EQ(42, g.load());
  
  // prefix decrement
  {
    ASSERT_EQ(41, (--g).load());
    ASSERT_EQ(41, g.load());
  }

  g = 95;
  ASSERT_EQ(95, g.load());

  // fetch_add
  {
    ASSERT_EQ(95, g.fetch_add(27));
    ASSERT_EQ(95 + 27, g.load());
  }

  g = 12345;
  ASSERT_EQ(12345, g.load());

  // fetch_sub
  {
    ASSERT_EQ(12345, g.fetch_sub(123));
    ASSERT_EQ(12345 - 123, g.load());
  }
  
  g = 9999;

  // operator+=
  {
    ASSERT_EQ(9999 + 49, (g += 49).load());
    ASSERT_EQ(9999 + 49, g.load());
  }
  
  g = 9999;

  // operator-=
  {
    ASSERT_EQ(9999 - 49, (g -= 49).load());
    ASSERT_EQ(9999 - 49, g.load());
  }
  
  g = 9999;

  // operator*=
  {
    ASSERT_EQ(9999 * 11, (g *= 11).load());
    ASSERT_EQ(9999 * 11, g.load());
  }
  
  g = 9999;
  
  // operator/=
  {
    ASSERT_EQ(9999 / 11, (g /= 11).load());
    ASSERT_EQ(9999 / 11, g.load());
  }  

  g = 0;
  {
    ASSERT_EQ(0, (g /= 10).load());
    ASSERT_EQ(0, g.load());
  }  
}

TEST(MetricsTest, test_gauge_operations_double) {
  Gauge<double> g(0.0, "gauge", "Test gauge");
  
  ASSERT_DOUBLE_EQ(0.0, g.load());

  // assignment
  g = 0.0;
  ASSERT_DOUBLE_EQ(0.0, g.load());
  
  g = 42.1;
  ASSERT_DOUBLE_EQ(42.1, g.load());
  
  g = 0.0;
  ASSERT_DOUBLE_EQ(0.0, g.load());

  // prefix increment
  {
    ASSERT_DOUBLE_EQ(1.0, (++g).load());
    ASSERT_DOUBLE_EQ(1.0, g.load());
  }
  
  g = 42.433;
  ASSERT_DOUBLE_EQ(42.433, g.load());
  
  // prefix decrement
  {
    ASSERT_DOUBLE_EQ(41.433, (--g).load());
    ASSERT_DOUBLE_EQ(41.433, g.load());
  }

  g = 95.91;
  ASSERT_DOUBLE_EQ(95.91, g.load());

  // fetch_add
  {
    ASSERT_DOUBLE_EQ(95.91, g.fetch_add(27.33));
    ASSERT_DOUBLE_EQ(95.91 + 27.33, g.load());
  }

  g = 12345.55;
  ASSERT_DOUBLE_EQ(12345.55, g.load());

  // fetch_sub
  {
    ASSERT_DOUBLE_EQ(12345.55, g.fetch_sub(123.33));
    ASSERT_DOUBLE_EQ(12345.55 - 123.33, g.load());
  }
  
  g = 9999.913;

  // operator+=
  {
    ASSERT_DOUBLE_EQ(9999.913 + 49.1, (g += 49.1).load());
    ASSERT_DOUBLE_EQ(9999.913 + 49.1, g.load());
  }
  
  g = 9999.0001;

  // operator-=
  {
    ASSERT_DOUBLE_EQ(9999.0001 - 49.1132, (g -= 49.1132).load());
    ASSERT_DOUBLE_EQ(9999.0001 - 49.1132, g.load());
  }
  
  g = 9999.0041;

  // operator*=
  {
    ASSERT_DOUBLE_EQ(9999.0041 * 11.44, (g *= 11.44).load());
    ASSERT_DOUBLE_EQ(9999.0041 * 11.44, g.load());
  }
  
  g = 9999.002;
  
  // operator/=
  {
    ASSERT_DOUBLE_EQ(9999.002 / 11.5, (g /= 11.5).load());
    ASSERT_DOUBLE_EQ(9999.002 / 11.5, g.load());
  }  

  g = 0.0;
  {
    ASSERT_DOUBLE_EQ(0.0, (g /= 10.0).load());
    ASSERT_DOUBLE_EQ(0.0, g.load());
  }  
}

template<typename Scale> void histogram_test(Scale const& scale) {

  using T = typename Scale::value_type;
  bool constexpr linear = (Scale::scale_type == ScaleType::Linear);

  int buckets = static_cast<int>(scale.n());
  T mx = scale.high(), mn = scale.low(), d;
  ADB_IGNORE_UNUSED T base = static_cast<T>(0.);
  T span = mx - mn;
  ADB_IGNORE_UNUSED T step = span / static_cast<T>(buckets);
  T mmin = (std::is_floating_point<T>::value) ? span / T(1.e6) : T(1), one(1), ten(10);

  if constexpr (!linear) {
    base = scale.base();
  }
  Histogram h(scale, "hist_test", "Hist test");

  //lower bucket bounds
  for (int i = 0; i < buckets; ++i) {
    if constexpr (linear) {
      d = mn + step*i + mmin;
    } else {
      d = mn + (mx-mn) * static_cast<T>(pow(base, i-buckets)) + mmin;
    }
    h.count(d);
//    ASSERT_DOUBLE_EQ(h.load(i), 1);
  }

  //upper bucket bounds
  for (int i = 0; i < buckets; ++i) {
    if constexpr (linear) {
      d = mn + step*(i+1) - mmin;
    } else {
      d = mn + (mx-mn) * static_cast<T>(pow(base, i-buckets+1)) - mmin;
    }
    h.count(d);
//    ASSERT_DOUBLE_EQ(h.load(i), 2);
  }

  //below lower limit
  h.count(mn - one);
  h.count(mn - ten);
//  ASSERT_DOUBLE_EQ(h.load(0), 5);

  // above upper limit
  h.count(mx + one);
  h.count(mx + ten);
//  ASSERT_DOUBLE_EQ(h.load(buckets-1), 5);

  // dump
  std::string s;
  h.toPrometheus(s, "", "");
}


TEST(MetricsTest, test_double_histogram) {
  histogram_test(lin_scale_t( 1.,  2.,  9));
  histogram_test(lin_scale_t(-1.,  1., 10));
  histogram_test(lin_scale_t(-2., -1.,  8));
}
TEST(MetricsTest, test_float_histogram) {
  histogram_test(lin_scale_t( 1.f,  2.f,  9));
  histogram_test(lin_scale_t(-1.f,  1.f, 10));
  histogram_test(lin_scale_t(-2.f, -1.f,  8));
}

TEST(MetricsTest, test_short_histogram) {
  histogram_test(lin_scale_t<short>(-17, 349, 6));
  histogram_test(lin_scale_t<short>( 20,  40, 7));
  histogram_test(lin_scale_t<short>(-63, -11, 8));
}
TEST(MetricsTest, test_int_histogram) {
  histogram_test(lin_scale_t<int>(-17, 349, 6));
  histogram_test(lin_scale_t<int>( 20,  40, 7));
  histogram_test(lin_scale_t<int>(-63, -11, 8));
}

TEST(MetricsTest, test_double_log_10_histogram) {
  histogram_test(log_scale_t(10., 0.,  2000.,  5));
}
TEST(MetricsTest, test_float_log_10_histogram) {
  histogram_test(log_scale_t(10.f, 0.f,  2000.f,  5));
}
TEST(MetricsTest, test_double_log_2_histogram) {
  histogram_test(log_scale_t(2., 0.,  2000.,  10));
}
TEST(MetricsTest, test_float_log_2_histogram) {
  histogram_test(log_scale_t(2.f, 0.f,  2000.f,  10));
}
TEST(MetricsTest, test_double_log_e_histogram) {
  histogram_test(log_scale_t(std::exp(1.), 0.,  2000.,  10));
}
TEST(MetricsTest, test_float_log_e_histogram) {
  histogram_test(log_scale_t(std::exp(1.f), 0.f,  2000.f,  10));
}
TEST(MetricsTest, test_double_log_bin_histogram) {
  histogram_test(log_scale_t(2., 0.,  128.,  8));
}
TEST(MetricsTest, test_float_log_bin_histogram) {
  histogram_test(log_scale_t(2.f, 0.f,  128.f,  8));
}
TEST(MetricsTest, test_double_log_offset_histogram) {
  histogram_test(log_scale_t(2., 0.,  128.,  8));
}
TEST(MetricsTest, test_float_log__histogram) {
  histogram_test(log_scale_t(2.f, 0.f,  128.f,  8));
}
TEST(MetricsTest, test_int64_log_bin_histogram) {
  histogram_test(log_scale_t<int64_t>(2, 50,  8000,  10));
}
TEST(MetricsTest, test_uint64_log_bin_histogram) {
  histogram_test(log_scale_t<uint64_t>(2, 50, 8000, 10));
}
