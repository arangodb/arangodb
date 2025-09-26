#include "gtest/gtest.h"

#include "Aql/AqlValue.h"
#include "Basics/Exceptions.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/SupervisedBuffer.h"
#include <velocypack/Builder.h>
#include <velocypack/Buffer.h>
#include <string>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::velocypack;

TEST(SupervisedBufferTest, AccountsMemoryLargeAndSmallValuesNormalBuffer) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor{global};

  {
    ResourceUsageScope usageScope(monitor);
    AqlValue largeValue;
    {
      Builder builder;
      builder.openArray();
      builder.add(Value(std::string(1024, 'a')));
      builder.close();

      ASSERT_EQ(monitor.current(), 0);
      ASSERT_GE(builder.size(), 1024);
      largeValue = AqlValue{builder.slice(), builder.size()};
      monitor.increaseMemoryUsage(largeValue.memoryUsage());
      ASSERT_EQ(monitor.current(), largeValue.memoryUsage());
    }
    ASSERT_EQ(monitor.current(), largeValue.memoryUsage());

    monitor.decreaseMemoryUsage(largeValue.memoryUsage());
    largeValue.destroy();
    ASSERT_EQ(monitor.current(), 0);
  }

  {
    ResourceUsageScope usageScope(monitor);
    AqlValue smallValue;
    {
      Builder builder;
      builder.openArray();
      builder.add(Value(1));  // inline in AqlValue
      builder.close();

      ASSERT_EQ(monitor.current(), 0);
      smallValue = AqlValue{builder.slice(), builder.size()};
      monitor.increaseMemoryUsage(smallValue.memoryUsage());  // very likely 0
      ASSERT_EQ(monitor.current(), smallValue.memoryUsage());
    }
    ASSERT_EQ(monitor.current(), 0);

    monitor.decreaseMemoryUsage(smallValue.memoryUsage());
    smallValue.destroy();
    ASSERT_EQ(monitor.current(), 0);
  }
}

TEST(SupervisedBufferTest, AccountsMemoryLargeAndSmallValuesSupervisedBuffer) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor{global};

  {
    ResourceUsageScope usageScope(monitor);
    AqlValue largeValue;
    {
      ASSERT_EQ(monitor.current(), 0);  // scope starts empty
      SupervisedBuffer supervisedBuffer(monitor);
      Builder builder(supervisedBuffer);
      builder.openArray();
      builder.add(Value(std::string(1024, 'a')));
      builder.close();

      ASSERT_GE(builder.size(), 1024);
      ASSERT_GE(monitor.current(), builder.size());  // capacity >= size
      largeValue = AqlValue{builder.slice(), builder.size()};
      monitor.increaseMemoryUsage(largeValue.memoryUsage());
      ASSERT_GE(monitor.current(), builder.size() + largeValue.memoryUsage());
      ASSERT_EQ(monitor.current(),
                supervisedBuffer.capacity() + largeValue.memoryUsage());
    }
    // now only AqlValue is accounted
    ASSERT_EQ(monitor.current(), largeValue.memoryUsage());

    monitor.decreaseMemoryUsage(largeValue.memoryUsage());
    largeValue.destroy();
    ASSERT_EQ(monitor.current(), 0);
  }

  {
    ResourceUsageScope usageScope(monitor);
    AqlValue smallValue;
    {
      SupervisedBuffer supervisedBuffer(monitor);
      Builder builder(supervisedBuffer);

      std::size_t before = monitor.current();
      builder.openArray();
      builder.add(Value(1));  // inline in AqlValue
      builder.close();

      ASSERT_EQ(monitor.current(), before);

      smallValue = AqlValue{builder.slice(), builder.size()};
      monitor.increaseMemoryUsage(smallValue.memoryUsage());  // likely 0
      ASSERT_EQ(monitor.current(), before + smallValue.memoryUsage());
    }
    ASSERT_EQ(monitor.current(), smallValue.memoryUsage());

    monitor.decreaseMemoryUsage(smallValue.memoryUsage());
    smallValue.destroy();
    ASSERT_EQ(monitor.current(), 0);
  }
}

TEST(SupervisedBufferTest,
     ManuallyIncreaseAccountsMemoryLargeAndSmallValuesSupervisedBuffer) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor{global};

  {
    ResourceUsageScope usageScope(monitor);
    AqlValue largeValue;
    std::size_t sizeBeforeLocal = 0;
    std::size_t valueSizeLocal = 0;
    {
      SupervisedBuffer supervisedBuffer(monitor);
      Builder builder(supervisedBuffer);
      builder.openArray();
      builder.add(Value(std::string(2048, 'a')));
      builder.close();

      ASSERT_GE(monitor.current(), builder.size());  // capacity accounted

      std::size_t monitorBefore = monitor.current();
      std::size_t sizeBefore = builder.size();
      usageScope.increase(sizeBefore);  // manual buffer usage accounting
      ASSERT_EQ(monitor.current(), monitorBefore + sizeBefore);

      sizeBeforeLocal = sizeBefore;
      largeValue = AqlValue{builder.slice(), builder.size()};
      valueSizeLocal = largeValue.memoryUsage();
      monitor.increaseMemoryUsage(valueSizeLocal);  // add value usage
      ASSERT_GE(monitor.current(), sizeBefore + valueSizeLocal);
    }
    ASSERT_GE(monitor.current(), sizeBeforeLocal + valueSizeLocal);
    monitor.decreaseMemoryUsage(valueSizeLocal);
    largeValue.destroy();
    usageScope.decrease(sizeBeforeLocal);  // balance manual accounting
    ASSERT_EQ(monitor.current(), 0);
  }

  {
    ResourceUsageScope usageScope(monitor);
    AqlValue smallValue;
    std::size_t sizeBeforeLocal = 0;
    std::size_t valueSizeLocal = 0;
    {
      SupervisedBuffer supervisedBuffer(monitor);
      Builder builder(supervisedBuffer);
      builder.openArray();
      builder.add(Value(42));  // inline in AqlValue
      builder.close();

      ASSERT_GE(monitor.current(), builder.size());  // capacity accounted

      std::size_t monitorBefore = monitor.current();
      std::size_t sizeBefore = builder.size();
      usageScope.increase(sizeBefore);
      ASSERT_EQ(monitor.current(), monitorBefore + sizeBefore);

      sizeBeforeLocal = sizeBefore;
      smallValue = AqlValue{builder.slice(), builder.size()};
      valueSizeLocal = smallValue.memoryUsage();  // likely 0
      monitor.increaseMemoryUsage(valueSizeLocal);
      ASSERT_GE(monitor.current(), sizeBefore + valueSizeLocal);
    }
    ASSERT_GE(monitor.current(), sizeBeforeLocal + valueSizeLocal);
    monitor.decreaseMemoryUsage(valueSizeLocal);
    smallValue.destroy();
    usageScope.decrease(sizeBeforeLocal);
    ASSERT_EQ(monitor.current(), 0);
  }
}

TEST(SupervisedBufferTest,
     ManuallyIncreaseAccountsMemoryLargeAndSmallValuesNormalBuffer) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor{global};

  {
    ResourceUsageScope usageScope(monitor);
    AqlValue largeValue;
    std::size_t sizeBeforeLocal = 0;
    std::size_t valueSizeLocal = 0;
    {
      Builder builder;
      builder.openArray();
      builder.add(Value(std::string(2048, 'a')));
      builder.close();
      ASSERT_EQ(monitor.current(), 0);

      std::size_t monitorBefore = monitor.current();
      std::size_t sizeBefore = builder.size();
      usageScope.increase(sizeBefore);
      ASSERT_EQ(monitor.current(), monitorBefore + sizeBefore);

      sizeBeforeLocal = sizeBefore;
      largeValue = AqlValue{builder.slice(), builder.size()};
      valueSizeLocal = largeValue.memoryUsage();
      monitor.increaseMemoryUsage(valueSizeLocal);
      ASSERT_EQ(monitor.current(), sizeBefore + valueSizeLocal);
    }
    ASSERT_EQ(monitor.current(), sizeBeforeLocal + valueSizeLocal);
    monitor.decreaseMemoryUsage(valueSizeLocal);
    largeValue.destroy();
    usageScope.decrease(sizeBeforeLocal);
    ASSERT_EQ(monitor.current(), 0);
  }

  {
    ResourceUsageScope usageScope(monitor);
    AqlValue smallValue;
    std::size_t sizeBeforeLocal = 0;
    std::size_t valueSizeLocal = 0;
    {
      Builder builder;
      builder.openArray();
      builder.add(Value(42));  // inline in AqlValue
      builder.close();
      ASSERT_EQ(monitor.current(), 0);

      std::size_t preMonitor = monitor.current();
      std::size_t sizeBefore = builder.size();
      usageScope.increase(sizeBefore);
      ASSERT_EQ(monitor.current(), preMonitor + sizeBefore);

      sizeBeforeLocal = sizeBefore;
      smallValue = AqlValue{builder.slice(), builder.size()};
      valueSizeLocal = smallValue.memoryUsage();  // likely 0
      monitor.increaseMemoryUsage(valueSizeLocal);
      ASSERT_EQ(monitor.current(), sizeBefore + valueSizeLocal);
    }
    ASSERT_EQ(monitor.current(), sizeBeforeLocal + valueSizeLocal);
    monitor.decreaseMemoryUsage(valueSizeLocal);
    smallValue.destroy();
    usageScope.decrease(sizeBeforeLocal);
    ASSERT_EQ(monitor.current(), 0);
  }
}

TEST(SupervisedBufferTest, ReuseSupervisedBufferAccountsMemory) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor{global};

  AqlValue firstValue;
  AqlValue secondValue;

  {
    SupervisedBuffer supervisedBuffer(monitor);
    Builder builder(supervisedBuffer);
    builder.openArray();
    builder.add(Value(std::string(1024, 'a')));
    builder.close();
    firstValue = AqlValue{builder.slice(), builder.size()};

    monitor.increaseMemoryUsage(firstValue.memoryUsage());
    ASSERT_GE(monitor.current(), firstValue.memoryUsage());
  }
  ASSERT_EQ(monitor.current(), firstValue.memoryUsage());

  {
    SupervisedBuffer supervisedBuffer(monitor);
    Builder builder(supervisedBuffer);
    builder.openArray();
    builder.add(Value(std::string(2048, 'b')));
    builder.close();
    secondValue = AqlValue{builder.slice(), builder.size()};

    monitor.increaseMemoryUsage(secondValue.memoryUsage());
    ASSERT_GE(monitor.current(),
              firstValue.memoryUsage() + secondValue.memoryUsage());
  }
  ASSERT_EQ(monitor.current(),
            firstValue.memoryUsage() + secondValue.memoryUsage());

  // drop manual accounting for both values before destroying them
  monitor.decreaseMemoryUsage(firstValue.memoryUsage());
  monitor.decreaseMemoryUsage(secondValue.memoryUsage());
  firstValue.destroy();
  secondValue.destroy();
  ASSERT_EQ(monitor.current(), 0);
}

TEST(SupervisedBufferTest, SupervisedBuilderGrowthAndRecycle) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor{global};

  {
    ResourceUsageScope usageScope(monitor);
    SupervisedBuffer supervisedBuffer(monitor);
    Builder builder(supervisedBuffer);

    builder.openArray();
    builder.add(Value(1));
    builder.add(Value(2));
    builder.add(Value(3));
    builder.close();

    std::size_t memory1 = monitor.current();
    ASSERT_GE(memory1, builder.size());

    builder.clear();

    builder.openArray();
    for (int i = 0; i < 200; ++i) {
      builder.add(Value(std::string(1024, 'a')));
    }
    builder.close();
    std::size_t memory2 = monitor.current();
    ASSERT_GT(memory2, memory1);
    ASSERT_GE(memory2, builder.size());

    builder.clear();
    std::size_t memory3 = monitor.current();
    ASSERT_EQ(memory3, memory2);
  }
  ASSERT_EQ(monitor.current(), 0);
}

TEST(SupervisedBufferTest, DetailedBufferResizeAndRecycle) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor{global};

  {
    ResourceUsageScope usageScope(monitor);
    SupervisedBuffer supervisedBuffer(monitor);
    Builder builder(supervisedBuffer);

    builder.openArray();
    for (int i = 0; i < 5; ++i) {
      builder.add(Value(i));  // tiny
    }
    builder.close();
    std::size_t memory1 = monitor.current();
    ASSERT_GE(memory1, builder.size());

    builder.clear();

    builder.openArray();
    for (int i = 0; i < 100; ++i) {
      builder.add(Value(std::string(256, 'a')));
    }
    builder.close();
    std::size_t memory2 = monitor.current();
    ASSERT_GE(memory2, builder.size());
    ASSERT_GT(memory2, memory1);
    ASSERT_GE(monitor.current(), builder.size());

    builder.clear();

    builder.openArray();
    builder.close();
    std::size_t memory3 = monitor.current();
    ASSERT_GE(memory3, builder.size());
    ASSERT_EQ(memory3, memory2);  // capacity kept after clear
  }
  ASSERT_EQ(monitor.current(), 0);
}

TEST(SupervisedBufferTest, AccountCapacityGrowAndClear) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor(global);
  ASSERT_EQ(monitor.current(), 0);

  {
    SupervisedBuffer supervisedBuffer(monitor);
    Builder builder(supervisedBuffer);

    // add amount > local inline capacity
    const int bigN = 200;
    builder.openArray();
    for (int i = 0; i < bigN; ++i) {
      builder.add(Value("abcde"));
    }
    builder.close();

    // current tracks capacity, which must be >= payload size
    ASSERT_GE(monitor.current(), builder.size());

    std::size_t snapshot = monitor.current();

    // recycle but don't deallocate
    builder.clear();

    // usage must remain at the snapshot
    ASSERT_EQ(monitor.current(), snapshot);

    // append half of the previous amount
    const int halfN = bigN / 2;
    builder.openArray();
    for (int i = 0; i < halfN; ++i) {
      builder.add(Value("abcde"));
    }
    builder.close();

    // usage must still be equal to the snapshot, because capacity was
    // maintained
    ASSERT_EQ(monitor.current(), snapshot);
  }

  ASSERT_EQ(monitor.current(), 0);
}

TEST(SupervisedBufferTest, EnforceMemoryLimitOnGrowth) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor(global);
  monitor.memoryLimit(1024);

  {
    SupervisedBuffer supervisedBuffer(monitor);
    Builder builder(supervisedBuffer);
    builder.openArray();
    bool threw = false;
    try {
      for (int i = 0; i < 4096; ++i) {
        builder.add(Value("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));  // 32 bytes
      }
      builder.close();  // only close if we didn't hit the limit
    } catch (arangodb::basics::Exception const& ex) {
      threw = true;
      EXPECT_EQ(TRI_ERROR_RESOURCE_LIMIT, ex.code());
    }
    EXPECT_TRUE(threw);
  }
  ASSERT_EQ(monitor.current(), 0);
}

TEST(SupervisedBufferTest, MultipleGrowsAndRecycle) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor(global);
  ASSERT_EQ(monitor.current(), 0);

  {
    SupervisedBuffer supervisedBuffer(monitor);
    Builder builder(supervisedBuffer);
    builder.openArray();

    // no growth expected yet
    for (int i = 0; i < 8; ++i) {
      builder.add(Value("ab"));
      ASSERT_GE(monitor.current(), builder.bufferRef().size());
    }

    // add more items until the buffer grows twice
    int growths = 0;
    for (int i = 0; i < 512 && growths < 2; ++i) {
      std::size_t capBefore = supervisedBuffer.capacity();
      std::size_t monitorBefore = monitor.current();

      builder.add(Value("abcdefghijklmnopqrstuvwxyz"));

      std::size_t capAfter = supervisedBuffer.capacity();
      std::size_t monitorAfter = monitor.current();

      ASSERT_GE(monitorAfter, builder.bufferRef().size());

      if (capAfter > capBefore) {
        std::size_t capDiff = capAfter - capBefore;
        ASSERT_EQ(monitorAfter, monitorBefore + capDiff);
        ++growths;
      }
    }
    ASSERT_GE(growths, 2);
    builder.close();
    ASSERT_GE(monitor.current(), builder.size());

    // recycle the buffer, memory doesn't decrease
    auto maintainedMonitorCurrent = monitor.current();
    builder.clear();
    ASSERT_EQ(monitor.current(), maintainedMonitorCurrent);

    // capacity must remain the same
    builder.openArray();
    for (int i = 0; i < 16; ++i) {
      builder.add(Value("abcde"));
      ASSERT_GE(monitor.current(), builder.bufferRef().size());
      ASSERT_EQ(monitor.current(), maintainedMonitorCurrent);
    }
    builder.close();
    ASSERT_EQ(monitor.current(), maintainedMonitorCurrent);
  }

  ASSERT_EQ(monitor.current(), 0);
}

TEST(SupervisedBufferTest, StealWithMemoryAccounting) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor{global};

  {
    // scope that will own the bytes after the steal
    ResourceUsageScope owningScope(monitor);

    SupervisedBuffer supervisedBuffer(monitor);
    Builder builder(supervisedBuffer);

    // the local inline capacity is 192 bytes
    std::size_t localCap = supervisedBuffer.capacity();

    // force growth beyond local capacity
    builder.openArray();
    for (int i = 0; i < 64; ++i) {
      builder.add(Value(std::string(256, 'a')));
    }
    builder.close();

    // buffer grew, so it allocated
    ASSERT_GT(supervisedBuffer.capacity(), localCap);

    // total and buffer capacity immediately before stealing
    std::size_t beforeTotal = monitor.current();
    std::size_t capBefore = supervisedBuffer.capacity();
    ASSERT_EQ(beforeTotal, capBefore);

    uint8_t* stolen = supervisedBuffer.stealWithMemoryAccounting(owningScope);
    // get the builder's ptr back to start
    builder.clear();

    // bytes still accounted so the memory accounted in the
    // monitor is the same
    ASSERT_EQ(monitor.current(), beforeTotal + localCap);

    // buffer returned to its local inline capacity after steal
    ASSERT_EQ(supervisedBuffer.capacity(), localCap);

    // owning scope now accounts for the bytes formerly in the buffer
    {
      std::size_t t0 = monitor.current();
      owningScope.decrease(capBefore);
      std::size_t t1 = monitor.current();
      ASSERT_EQ(t0 - t1, capBefore)
          << "owningScope must own the former buffer capacity";
      owningScope.increase(capBefore);
      std::size_t t2 = monitor.current();
      ASSERT_EQ(t2, t0);
    }

    // capacity should remain local
    builder.openArray();
    builder.add(Value("abcde"));
    builder.close();
    ASSERT_EQ(supervisedBuffer.capacity(), localCap);
    ASSERT_EQ(monitor.current(), beforeTotal + localCap);

    // frees the stolen memory
    velocypack_free(stolen);
  }

  ASSERT_EQ(monitor.current(), 0);
}

TEST(SupervisedBufferTest, StealWithLocalValueTracked) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor(global);

  ResourceUsageScope owningScope(monitor);
  SupervisedBuffer supervisedBuffer(monitor);

  // grow beyond inline storage
  Builder builder(supervisedBuffer);
  builder.openArray();
  for (int i = 0; i < 64; ++i) {
    builder.add(Value(std::string(256, 'a')));
  }
  builder.close();

  // expected tracked usage before steal
  std::size_t trackedBefore = monitor.current();
  ASSERT_EQ(trackedBefore, supervisedBuffer.capacity());

  // steal and reset builder
  uint8_t* ptr = supervisedBuffer.stealWithMemoryAccounting(owningScope);
  builder.clear();

  // After steal, monitor should now account for the inline value + bytes
  // allocated that are now tracked by owning scope
  ASSERT_EQ(monitor.current(),
            owningScope.tracked() + supervisedBuffer.capacity())
      << "buffer should account its inline (192) bytes + allocated bytes after "
         "steal";

  // monitor should remain unchanged
  builder.openArray();
  builder.add(Value(1));
  builder.add(Value(2));
  builder.close();
  ASSERT_EQ(monitor.current(),
            owningScope.tracked() + supervisedBuffer.capacity());

  velocypack_free(ptr);
  owningScope.revert();
  ASSERT_EQ(monitor.current(), supervisedBuffer.capacity());
}

TEST(SupervisedBufferTest, GrowStealAndGrowAgainAccounting) {
  auto& global = GlobalResourceMonitor::instance();
  ResourceMonitor monitor(global);

  ResourceUsageScope owningScope(monitor);
  SupervisedBuffer supervisedBuffer(monitor);
  auto capLocal = supervisedBuffer.capacity();
  ASSERT_EQ(owningScope.tracked(), 0);
  ASSERT_EQ(monitor.current(), capLocal);

  // grow once
  Builder builder(supervisedBuffer);
  builder.openArray();
  for (int i = 0; i < 64; ++i) {
    builder.add(Value(std::string(256, 'b')));
  }
  builder.close();

  // capacity and monitor usage
  std::size_t capBefore = supervisedBuffer.capacity();
  std::size_t monitorBefore = monitor.current();
  ASSERT_EQ(monitorBefore, capBefore);
  ASSERT_EQ(owningScope.tracked(), 0);

  // steal the dynamic allocation
  uint8_t* ptr = supervisedBuffer.stealWithMemoryAccounting(owningScope);
  ASSERT_EQ(capLocal, supervisedBuffer.capacity());
  ASSERT_EQ(monitor.current(), capBefore + capLocal);
  ASSERT_EQ(owningScope.tracked(), capBefore);
  builder.clear();

  // grow again to trigger allocation on heap
  builder.openArray();
  for (int i = 0; i < 32; ++i) {
    builder.add(Value(std::string(300, 'c')));
  }
  builder.close();

  // Now capacity > 192 again; monitor should reflect the full dynamic capacity
  ASSERT_GT(supervisedBuffer.capacity(), capLocal);
  ASSERT_EQ(monitor.current(),
            supervisedBuffer.capacity() + owningScope.tracked());

  velocypack_free(ptr);
  owningScope.revert();
  ASSERT_EQ(monitor.current(), supervisedBuffer.capacity());
}
