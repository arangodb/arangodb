
#include <iostream>
#include "formats/columnstore2.hpp"
#include "resource_manager.hpp"
#include <gtest/gtest.h>

using irs::columnstore2::column;

namespace {

    class address_table_tests : public ::testing::Test {
        public:
            address_table_tests() { }

            void SetUp() override {
            }

            void TearDown() override {}

        public:
            irs::ManagedTypedAllocator<uint64_t> alloc;
    };
}

TEST_F(address_table_tests, smoke_test) {

    column::address_table addr_table(alloc);

    ASSERT_EQ(0, addr_table.size());

    //  push
    ASSERT_TRUE(addr_table.push_back(1));
    ASSERT_EQ(1, addr_table.size());

    //  pop
    ASSERT_TRUE(addr_table.pop_back());
    ASSERT_EQ(0, addr_table.size());

    //  full
    for (uint64_t i = 0; i < column::kBlockSize; i++) {
        ASSERT_TRUE(addr_table.push_back(i));
    }
    ASSERT_EQ(addr_table.size(), column::kBlockSize);
    ASSERT_TRUE(addr_table.full());

    //  reset
    addr_table.reset();
    ASSERT_TRUE(addr_table.empty());
}

//  pop when empty
TEST_F(address_table_tests, pop_when_empty) {

    column::address_table addr_table(alloc);
    ASSERT_TRUE(addr_table.empty());
    ASSERT_FALSE(addr_table.pop_back());
}

//  push when full
TEST_F(address_table_tests, push_when_full) {

    column::address_table addr_table(alloc);

    ASSERT_TRUE(addr_table.empty());

    for (uint64_t i = 0; i < column::kBlockSize; i++) {
        ASSERT_TRUE(addr_table.push_back(i));
    }

    ASSERT_EQ(addr_table.size(), column::kBlockSize);
    ASSERT_FALSE(addr_table.push_back(2));

    ASSERT_TRUE(addr_table.pop_back());
    ASSERT_TRUE(addr_table.push_back(2));
}

//  push and verify data
TEST_F(address_table_tests, verify_data) {

    column::address_table addr_table(alloc);

    for (uint64_t i = 0; i < 200; i++) {
        ASSERT_TRUE(addr_table.push_back(i));
    }

    ASSERT_EQ(addr_table.size(), 200);

    auto curr = addr_table.current();
    auto begin = addr_table.begin();

    uint64_t data { 199 };
    while (curr != begin) {
        --curr;
        ASSERT_EQ(*curr, data--);
    }
}

//  write using current ptr
TEST_F(address_table_tests, write_using_current_ptr) {

    column::address_table addr_table(alloc);

    uint64_t elemCount = 60;
    //  add data via push_back.
    auto data = 0;
    for (uint64_t i = 0; i < elemCount; i++) {
        ASSERT_TRUE(addr_table.push_back(data++));
    }

    //  add data via current() ptr.
    //  column::flush_block() uses address_table
    //  in this way.
    uint64_t addr_table_size = 40900;
    ASSERT_TRUE(addr_table.grow_size(addr_table.size() + addr_table_size));

    auto curr = addr_table.current();
    auto end = curr + addr_table_size;

    while (curr != end) {
        *curr++ = data++;
    }

    //  size is updated only when adding data
    //  via push_back().
    ASSERT_EQ(addr_table.size(), 60);

    auto begin = addr_table.begin();
    for (uint64_t i = 0; i < 40960; i++) {
        ASSERT_EQ(*begin++, i);
    }
}

//  size arithmetic
TEST_F(address_table_tests, begin_current_end) {

    column::address_table addr_table(alloc);

    ASSERT_EQ(addr_table.size(), 0);
    ASSERT_TRUE(addr_table.push_back(45));
    ASSERT_EQ(addr_table.size(), 1);

    ASSERT_EQ(addr_table.current(), addr_table.begin() + addr_table.size());
}

//  max size allowed
TEST_F(address_table_tests, max_size_allowed) {

    column::address_table addr_table(alloc);

    //  end() always points to begin() + kBlockSize
    //  coz address_table pre-allocates memory to
    //  accommodate kBlockSize elements.
    ASSERT_EQ(addr_table.size(), 0);

    ASSERT_EQ(addr_table.current(), addr_table.begin() + 0);

    ASSERT_TRUE(addr_table.grow_size(column::kBlockSize));

    for (uint64_t i = 0; i < column::kBlockSize; i++) {
        ASSERT_FALSE(addr_table.full());
        ASSERT_TRUE(addr_table.push_back(i));
    }
    ASSERT_TRUE(addr_table.full());
    ASSERT_EQ(addr_table.current(), addr_table.begin() + column::kBlockSize);
}

//  full
TEST_F(address_table_tests, empty_full_and_reset) {

    column::address_table addr_table(alloc);

    ASSERT_FALSE(addr_table.full());
    for (uint64_t i = 0; i < column::kBlockSize; i++) {
        ASSERT_FALSE(addr_table.full());
        ASSERT_TRUE(addr_table.push_back(i));
    }
    ASSERT_TRUE(addr_table.full());

    addr_table.pop_back();
    ASSERT_FALSE(addr_table.full());

    addr_table.reset();
    ASSERT_FALSE(addr_table.full());
    ASSERT_EQ(addr_table.current(), addr_table.begin());
    ASSERT_EQ(addr_table.size(), 0);
}
