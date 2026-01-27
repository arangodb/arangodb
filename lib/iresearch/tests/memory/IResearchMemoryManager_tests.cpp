////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Koushal Kawade
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <memory>
#include "tests_shared.hpp"
#include "utils/managed_allocator.hpp"
#include "resource_manager.hpp"

using namespace irs;

struct LargeData {
    char data[1024];
};

class IResearchMemoryLimitTest : public ::testing::Test {

    //  IResearchMemoryManager's state persists between tests
    //  since it's a static singleton instance. We must clear
    //  the used memory at the end of each test so that the next
    //  test gets a fresh start.
    void clearUsedMemory() {

        auto memoryMgr = IResearchMemoryManager::GetInstance();
        memoryMgr->Decrease(memoryMgr->getCurrentUsage());
    }

    virtual void TearDown() override {
        clearUsedMemory();
    }
};

TEST_F(IResearchMemoryLimitTest, memory_manager_smoke_test) {
    auto memoryMgr = IResearchMemoryManager::GetInstance();

    auto elemSize = sizeof(uint64_t);
    auto maxElems = 2;
    memoryMgr->SetMemoryLimit(maxElems * elemSize);

    //  Add 10 elems.
    for (int i = 0; i < maxElems; i++) {
        ASSERT_NO_THROW(memoryMgr->Increase(elemSize));
    }

    //  Try adding 11th element. Should throw.
    ASSERT_THROW(memoryMgr->Increase(elemSize), std::bad_alloc);

    //  Remove 1 element and try again, shouldn't throw.
    memoryMgr->Decrease(elemSize);
    ASSERT_NO_THROW(memoryMgr->Increase(elemSize));

    //  Limit reached. Should start throwing again.
    ASSERT_THROW(memoryMgr->Increase(elemSize), std::bad_alloc);

    //  Increase the limit and add 1 more element, shouldn't throw.
    memoryMgr->SetMemoryLimit((maxElems + 1) * elemSize);
    ASSERT_NO_THROW(memoryMgr->Increase(elemSize));

    //  Adding any more elements should throw again
    ASSERT_THROW(memoryMgr->Increase(elemSize), std::bad_alloc);
}

TEST_F(IResearchMemoryLimitTest, no_limit_set_allows_infinite_allocations) {

    //  The expectation is to allow potentially infinite allocations
    //  even if we're testing with just 1MB.

    auto memoryMgr = IResearchMemoryManager::GetInstance();
    memoryMgr->SetMemoryLimit(0);

    //  allocate vector
    ManagedVector<LargeData> vec;

    for (size_t i = 0; i < 1024; i++) {
        ASSERT_NO_THROW(vec.push_back(LargeData()));
    }
}

TEST_F(IResearchMemoryLimitTest, zero_limit_allow_infinite_allocations) {

    auto memoryMgr = IResearchMemoryManager::GetInstance();
    memoryMgr->SetMemoryLimit(0);

    //  allocate vector
    ManagedVector<LargeData> vec;

    for (size_t i = 0; i < 1024; i++) {
        ASSERT_NO_THROW(vec.push_back(LargeData()));
    }
}

TEST_F(IResearchMemoryLimitTest, managed_allocator_smoke_test) {

    auto memoryMgr = IResearchMemoryManager::GetInstance();
    memoryMgr->SetMemoryLimit(3);

    using type = char;
    std::vector<type, ManagedTypedAllocator<type>> arr(*memoryMgr);

    arr.push_back('a');
    arr.push_back('b');
    ASSERT_THROW(arr.push_back('c'), std::bad_alloc);
}

TEST_F(IResearchMemoryLimitTest, memory_manager_managed_vector_test) {

    //  set limit
    size_t maxElements { 4 };
    auto memoryMgr = IResearchMemoryManager::GetInstance();
    memoryMgr->SetMemoryLimit(maxElements * sizeof(uint64_t));

    auto addElemsToVec = [](size_t count) {
        ManagedVector<uint64_t> vec;
        for (size_t i = 0; i < count; i++) {
            vec.push_back(i);
        }
    };

    ASSERT_THROW(addElemsToVec(maxElements + 1), std::bad_alloc);

    //  Increase memory limit.
    memoryMgr->SetMemoryLimit(maxElements * 3 * sizeof(uint64_t));
    ASSERT_NO_THROW(addElemsToVec(maxElements + 1));
}
