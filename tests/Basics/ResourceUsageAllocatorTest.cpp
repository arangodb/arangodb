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
/// @author Jan Steemann
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"

#include <string>
#include <vector>

using namespace arangodb;

TEST(ResourceUsageAllocatorTest, testEmpty) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  ResourceUsageAllocator<int, ResourceMonitor> alloc(monitor);
  ASSERT_EQ(0, monitor.current());
}

TEST(ResourceUsageAllocatorTest, testStringAppend) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  ResourceUsageAllocator<std::string, ResourceMonitor> alloc(monitor);

  std::string test(alloc);
  ASSERT_EQ(0, monitor.current());

  // nothing will be tracked here
  for (size_t i = 0; i < 32768; ++i) {
    test.append("foobar");
  }
  ASSERT_EQ(0, monitor.current());
}

TEST(ResourceUsageAllocatorTest, testStringPushBack) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  ResourceUsageAllocator<std::string, ResourceMonitor> alloc(monitor);

  std::string test(alloc);
  ASSERT_EQ(0, monitor.current());

  // nothing will be tracked here
  for (size_t i = 0; i < 65537; ++i) {
    test.push_back('x');
  }
  ASSERT_EQ(0, monitor.current());
}

TEST(ResourceUsageAllocatorTest, testMonitoredStringPushBack) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  using MonitoredString =
      std::basic_string<char, std::char_traits<char>,
                        ResourceUsageAllocator<char, ResourceMonitor>>;

  ResourceUsageAllocator<MonitoredString, ResourceMonitor> alloc(monitor);

  MonitoredString test(alloc);
  ASSERT_EQ(0, monitor.current());

  for (size_t i = 0; i < 32769; ++i) {
    test.push_back('x');
  }
  // we must have seen _some_ allocation(s)
  ASSERT_GT(monitor.current(), 0);
  // we don't know how much memory was used exactly (depends on
  // the internal growth strategy for std::string, which we don't
  // want to replicate here)
  ASSERT_LE(32768, monitor.current());
  ASSERT_LE(monitor.current(), 65536);
}

TEST(ResourceUsageAllocatorTest, testMonitoredStringResize) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  using MonitoredString =
      std::basic_string<char, std::char_traits<char>,
                        ResourceUsageAllocator<char, ResourceMonitor>>;

  ResourceUsageAllocator<MonitoredString, ResourceMonitor> alloc(monitor);

  MonitoredString test(alloc);
  ASSERT_EQ(0, monitor.current());

  test.resize(128'000);

  // we must have seen _some_ allocation, at least 128'000 plus one
  // byte for the null terminator
  ASSERT_GE(monitor.current(), 128'001);

  // clear strink
  test.clear();
  test.shrink_to_fit();
  ASSERT_GE(monitor.current(), 0);

  test.resize(256'000);

  ASSERT_GE(monitor.current(), 256'001);
}

TEST(ResourceUsageAllocatorTest, testMonitoredStringVectorReserve) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  using MonitoredString =
      std::basic_string<char, std::char_traits<char>,
                        ResourceUsageAllocator<char, ResourceMonitor>>;
  using MonitoredStringVector =
      std::vector<MonitoredString,
                  ResourceUsageAllocator<MonitoredString, ResourceMonitor>>;

  ResourceUsageAllocator<MonitoredStringVector, ResourceMonitor> alloc(monitor);

  MonitoredStringVector test(alloc);
  ASSERT_EQ(0, monitor.current());

  test.reserve(32768);
  ASSERT_GE(monitor.current(), 32768 * sizeof(MonitoredString));

  test.reserve(35000);
  ASSERT_GE(monitor.current(), 35000 * sizeof(MonitoredString));
}

TEST(ResourceUsageAllocatorTest, testMonitoredStringVectorGrowth) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  using MonitoredString =
      std::basic_string<char, std::char_traits<char>,
                        ResourceUsageAllocator<char, ResourceMonitor>>;
  using MonitoredStringVector =
      std::vector<MonitoredString,
                  ResourceUsageAllocator<MonitoredString, ResourceMonitor>>;

  ResourceUsageAllocator<MonitoredStringVector, ResourceMonitor> alloc(monitor);

  MonitoredStringVector test(alloc);
  ASSERT_EQ(0, monitor.current());

  // allocation sizes for this pattern are likely
  // 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384
  for (size_t i = 0; i < 16383; ++i) {
    test.emplace_back();
  }
  ASSERT_GE(monitor.current(), 16384 * sizeof(MonitoredString));
}

TEST(ResourceUsageAllocatorTest,
     testMonitoredStringVectorWithMonitoredStringPayloads) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  using MonitoredString =
      std::basic_string<char, std::char_traits<char>,
                        ResourceUsageAllocator<char, ResourceMonitor>>;
  using MonitoredStringVector =
      std::vector<MonitoredString,
                  ResourceUsageAllocator<MonitoredString, ResourceMonitor>>;

  ResourceUsageAllocator<MonitoredStringVector, ResourceMonitor> alloc(monitor);

  MonitoredStringVector test(alloc);
  ASSERT_EQ(0, monitor.current());

  constexpr std::string_view payload(
      "ein-mops-kam-in-die-küche-und-stahl-dem-koch-ein-ei");
  {
    // create one MonitoredString to insert 8000 times later
    MonitoredString p(payload, alloc);
    ASSERT_EQ(monitor.current(), payload.size() + 1);

    // insert 8000 monitored strings
    for (size_t i = 0; i < 8000; ++i) {
      test.emplace_back(p);
    }
    // 8192 because the vector will employ a times-2 growth strategy
    ASSERT_GE(monitor.current(), 8192 * sizeof(MonitoredString) +
                                     (8000 + 1) * (payload.size() + 1));
  }

  test = MonitoredStringVector(alloc);

  ASSERT_EQ(monitor.current(), 0);

  {
    // insert 8000 std::strings. these will be converted to MonitoredStrings
    for (size_t i = 0; i < 8000; ++i) {
      test.emplace_back(std::string(payload));
    }
    // 8192 because the vector will employ a times-2 growth strategy
    ASSERT_GE(monitor.current(),
              8192 * sizeof(MonitoredString) + 8000 * (payload.size() + 1));
  }
}
