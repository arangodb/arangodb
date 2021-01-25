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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/CpuUsageSnapshot.h"

using namespace arangodb;

TEST(CpuUsageSnapshotTest, testEmpty) {
  CpuUsageSnapshot s;

  ASSERT_FALSE(s.valid());
  ASSERT_EQ(0, s.total());
  ASSERT_EQ(0.0, s.userPercent());
  ASSERT_EQ(0.0, s.systemPercent());
  ASSERT_EQ(0.0, s.idlePercent());
  ASSERT_EQ(0.0, s.iowaitPercent());
}

TEST(CpuUsageSnapshotTest, testFromString) {
  { 
    std::string input("");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    ASSERT_FALSE(s.valid());
    ASSERT_EQ(0, s.total());
  }
  
  { 
    std::string input("quetzalcoatl");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    ASSERT_FALSE(s.valid());
    ASSERT_EQ(0, s.total());
  }
  
  { 
    std::string input("1");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    ASSERT_FALSE(s.valid());
    ASSERT_EQ(0, s.total());
  }
  
  { 
    std::string input("1 2 3445");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    ASSERT_FALSE(s.valid());
    ASSERT_EQ(0, s.total());
  }
  
  { 
    std::string input("19999999999999999999999999999999999999999999999999999999999999999999999999999999999999");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    ASSERT_FALSE(s.valid());
    ASSERT_EQ(0, s.total());
  }
  
  { 
    std::string input("1234 48868 939949 439995 2030223 02232");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    ASSERT_FALSE(s.valid());
    ASSERT_EQ(0, s.total());
  }

  { 
    std::string input("1 2 3 4 5 6 7 8 9 10");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    uint64_t total = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10;
    ASSERT_TRUE(s.valid());
    ASSERT_EQ(total, s.total());
    ASSERT_EQ(1, s.user);
    ASSERT_EQ(2, s.nice);
    ASSERT_EQ(3, s.system);
    ASSERT_EQ(4, s.idle);
    ASSERT_EQ(5, s.iowait);
    ASSERT_EQ(6, s.irq);
    ASSERT_EQ(7, s.softirq);
    ASSERT_EQ(8, s.steal);
    ASSERT_EQ(9, s.guest);
    ASSERT_EQ(10, s.guestnice);
    ASSERT_DOUBLE_EQ(100. * (1.+ 2.)  / double(total), s.userPercent());
    ASSERT_DOUBLE_EQ(100. * 3. / double(total), s.systemPercent());
    ASSERT_DOUBLE_EQ(100. * 4. / double(total), s.idlePercent());
    ASSERT_DOUBLE_EQ(100. * 5. / double(total), s.iowaitPercent());
  }
  
  { 
    std::string input("1 0 0 0 0 0 0 0 0 0");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    uint64_t total = 1;
    ASSERT_TRUE(s.valid());
    ASSERT_EQ(total, s.total());
    ASSERT_EQ(1, s.user);
    ASSERT_EQ(0, s.nice);
    ASSERT_EQ(0, s.system);
    ASSERT_EQ(0, s.idle);
    ASSERT_EQ(0, s.iowait);
    ASSERT_EQ(0, s.irq);
    ASSERT_EQ(0, s.softirq);
    ASSERT_EQ(0, s.steal);
    ASSERT_EQ(0, s.guest);
    ASSERT_EQ(0, s.guestnice);
    ASSERT_DOUBLE_EQ(100. * 1. / double(total), s.userPercent());
    ASSERT_DOUBLE_EQ(100. * 0. / double(total), s.systemPercent());
    ASSERT_DOUBLE_EQ(100. * 0. / double(total), s.idlePercent());
    ASSERT_DOUBLE_EQ(100. * 0. / double(total), s.iowaitPercent());
  }

  { 
    std::string input("578816 390 54632 4019475 2523 0 275 0 0 0");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    uint64_t total = 578816 + 390 + 54632 + 4019475 + 2523 + 0 + 275 + 0 + 0 + 0;
    ASSERT_TRUE(s.valid());
    ASSERT_EQ(total, s.total());
    ASSERT_EQ(578816, s.user);
    ASSERT_EQ(390, s.nice);
    ASSERT_EQ(54632, s.system);
    ASSERT_EQ(4019475, s.idle);
    ASSERT_EQ(2523, s.iowait);
    ASSERT_EQ(0, s.irq);
    ASSERT_EQ(275, s.softirq);
    ASSERT_EQ(0, s.steal);
    ASSERT_EQ(0, s.guest);
    ASSERT_EQ(0, s.guestnice);
    ASSERT_DOUBLE_EQ(100. * (578816. + 390.)  / double(total), s.userPercent());
    ASSERT_DOUBLE_EQ(100. * 54632. / double(total), s.systemPercent());
    ASSERT_DOUBLE_EQ(100. * 4019475. / double(total), s.idlePercent());
    ASSERT_DOUBLE_EQ(100. * 2523. / double(total), s.iowaitPercent());
  }

  { 
    std::string input("304866003 5720038 69726754 4732078787 130352063 0 7621266 0 0 0");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    uint64_t total = 304866003 + 5720038 + 69726754 + 4732078787 + 130352063 + 0 + 7621266 + 0 + 0 + 0;
    ASSERT_TRUE(s.valid());
    ASSERT_EQ(total, s.total());
    ASSERT_EQ(304866003, s.user);
    ASSERT_EQ(5720038, s.nice);
    ASSERT_EQ(69726754, s.system);
    ASSERT_EQ(4732078787, s.idle);
    ASSERT_EQ(130352063, s.iowait);
    ASSERT_EQ(0, s.irq);
    ASSERT_EQ(7621266, s.softirq);
    ASSERT_EQ(0, s.steal);
    ASSERT_EQ(0, s.guest);
    ASSERT_EQ(0, s.guestnice);
    ASSERT_DOUBLE_EQ(100. * (304866003. + 5720038.)  / double(total), s.userPercent());
    ASSERT_DOUBLE_EQ(100. * 69726754. / double(total), s.systemPercent());
    ASSERT_DOUBLE_EQ(100. * 4732078787. / double(total), s.idlePercent());
    ASSERT_DOUBLE_EQ(100. * 130352063. / double(total), s.iowaitPercent());
  }

  { 
    std::string input("624582 562 63837 5793524 3165 0 361 0 0 0\ncpu0 51303 38 7370 749474 378 0 216 0 0 0");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

    uint64_t total = 624582 + 562 + 63837 + 5793524 + 3165 + 0 + 361 + 0 + 0 + 0;
    ASSERT_TRUE(s.valid());
    ASSERT_EQ(total, s.total());
    ASSERT_EQ(624582, s.user);
    ASSERT_EQ(562, s.nice);
    ASSERT_EQ(63837, s.system);
    ASSERT_EQ(5793524, s.idle);
    ASSERT_EQ(3165, s.iowait);
    ASSERT_EQ(0, s.irq);
    ASSERT_EQ(361, s.softirq);
    ASSERT_EQ(0, s.steal);
    ASSERT_EQ(0, s.guest);
    ASSERT_EQ(0, s.guestnice);
    ASSERT_DOUBLE_EQ(100. * (624582. + 562.)  / double(total), s.userPercent());
    ASSERT_DOUBLE_EQ(100. * 63837. / double(total), s.systemPercent());
    ASSERT_DOUBLE_EQ(100. * 5793524. / double(total), s.idlePercent());
    ASSERT_DOUBLE_EQ(100. * 3165. / double(total), s.iowaitPercent());
  }
}

TEST(CpuUsageSnapshotTest, testClear) {
  std::string input("1 2 3 4 5 6 7 8 9 10");
  CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());

  ASSERT_TRUE(s.valid());
  s.clear();
  
  ASSERT_FALSE(s.valid());
  ASSERT_EQ(0, s.total());
  ASSERT_EQ(0.0, s.userPercent());
  ASSERT_EQ(0.0, s.systemPercent());
  ASSERT_EQ(0.0, s.idlePercent());
  ASSERT_EQ(0.0, s.iowaitPercent());
}

TEST(CpuUsageSnapshotTest, testSubtract) {
  {
    // subtract snapshot with the same data
    std::string input("1 2 3 4 5 6 7 8 9 10");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input.data(), input.size());
    ASSERT_TRUE(s.valid());
    
    CpuUsageSnapshot o = CpuUsageSnapshot::fromString(input.data(), input.size());
    ASSERT_TRUE(o.valid());

    s.subtract(o);
    ASSERT_FALSE(s.valid());
    ASSERT_EQ(0, s.total());
    ASSERT_EQ(0, s.user);
    ASSERT_EQ(0, s.nice);
    ASSERT_EQ(0, s.system);
    ASSERT_EQ(0, s.idle);
    ASSERT_EQ(0, s.iowait);
    ASSERT_EQ(0, s.irq);
    ASSERT_EQ(0, s.softirq);
    ASSERT_EQ(0, s.steal);
    ASSERT_EQ(0, s.guest);
    ASSERT_EQ(0, s.guestnice);
  }
  
  {
    std::string input1("624582 562 63837 5793524 3165 0 361 0 0 0");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input1.data(), input1.size());
    ASSERT_TRUE(s.valid());
    
    std::string input2("578816 390 54632 4019475 2523 0 275 0 0 0");
    CpuUsageSnapshot o = CpuUsageSnapshot::fromString(input2.data(), input2.size());
    ASSERT_TRUE(o.valid());

    s.subtract(o);
    ASSERT_TRUE(s.valid());
    ASSERT_EQ(624582 + 562 + 63837 + 5793524 + 3165 + 0 + 361 + 0 + 0 + 0 - 578816 - 390 - 54632 - 4019475 - 2523 - 0 - 275 - 0 -0 - 0, s.total());
    ASSERT_EQ(624582 - 578816, s.user);
    ASSERT_EQ(562 - 390, s.nice);
    ASSERT_EQ(63837 - 54632, s.system);
    ASSERT_EQ(5793524 - 4019475, s.idle);
    ASSERT_EQ(3165 - 2523, s.iowait);
    ASSERT_EQ(0, s.irq);
    ASSERT_EQ(361 - 275, s.softirq);
    ASSERT_EQ(0, s.steal);
    ASSERT_EQ(0, s.guest);
    ASSERT_EQ(0, s.guestnice);
  }
  
  {
    // underflow
    std::string input1("578816 390 54632 4019475 2523 0 275 0 0 0");
    CpuUsageSnapshot s = CpuUsageSnapshot::fromString(input1.data(), input1.size());
    ASSERT_TRUE(s.valid());
    
    std::string input2("624582 562 63837 5793524 3165 0 361 0 0 0");
    CpuUsageSnapshot o = CpuUsageSnapshot::fromString(input2.data(), input2.size());
    ASSERT_TRUE(o.valid());

    s.subtract(o);
    ASSERT_FALSE(s.valid());
    ASSERT_EQ(0, s.total());
    ASSERT_EQ(0, s.total());
    ASSERT_EQ(0, s.user);
    ASSERT_EQ(0, s.nice);
    ASSERT_EQ(0, s.system);
    ASSERT_EQ(0, s.idle);
    ASSERT_EQ(0, s.iowait);
    ASSERT_EQ(0, s.irq);
    ASSERT_EQ(0, s.softirq);
    ASSERT_EQ(0, s.steal);
    ASSERT_EQ(0, s.guest);
    ASSERT_EQ(0, s.guestnice);
    ASSERT_EQ(0, s.user);
  }
}
