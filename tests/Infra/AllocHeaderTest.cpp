////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "infra/AllocHeader.h"
#include "infra/StaticResourceMonitor.h"

#include <cstddef>
#include <cstdint>

using namespace arangodb::infra;

namespace {

// Mirrors the layout posix_memalign produces for large alignments: the first
// aligned address leaving room for real_ptr plus the header below it.
auto alignedUserPtr(void* real, size_t alignment) -> void* {
  auto addr =
      reinterpret_cast<uintptr_t>(real) + sizeof(AllocHeader) + sizeof(void*);
  addr = (addr + alignment - 1) & ~(alignment - 1);
  return reinterpret_cast<void*>(addr);
}

constexpr size_t kAlignments[] = {32, 64, 128, 256, 4096};

}  // namespace

TEST(AllocHeaderTest, user_pointer_sits_directly_above_the_header) {
  alignas(alignof(std::max_align_t)) std::byte buffer[64];
  auto* header = reinterpret_cast<AllocHeader*>(buffer);

  EXPECT_EQ(static_cast<void*>(buffer + sizeof(AllocHeader)), userPtr(header));
  EXPECT_EQ(header, headerPtr(userPtr(header)));
}

TEST(AllocHeaderTest, normal_header_carries_monitor_and_requested_size) {
  alignas(alignof(std::max_align_t)) std::byte buffer[64];
  auto* header = reinterpret_cast<AllocHeader*>(buffer);
  StaticResourceMonitor monitor;

  initHeader(header, &monitor, 1234);

  EXPECT_EQ(&monitor, getMonitor(header));
  EXPECT_EQ(1234u, header->requested_size);
  EXPECT_FALSE(isAlignedAlloc(header));
}

// A normal allocation stores no real_ptr; the header is the base pointer.
TEST(AllocHeaderTest, normal_header_recovers_itself_as_the_real_pointer) {
  alignas(alignof(std::max_align_t)) std::byte buffer[64];
  auto* header = reinterpret_cast<AllocHeader*>(buffer);
  StaticResourceMonitor monitor;

  initHeader(header, &monitor, 1234);

  EXPECT_EQ(static_cast<void*>(header), getRealPtr(header));
  EXPECT_EQ(sizeof(AllocHeader), headerOverhead(header));
}

// malloc(0) returns a real block, so a zero size must round-trip.
TEST(AllocHeaderTest, zero_requested_size_round_trips) {
  alignas(alignof(std::max_align_t)) std::byte buffer[64];
  auto* header = reinterpret_cast<AllocHeader*>(buffer);
  StaticResourceMonitor monitor;

  initHeader(header, &monitor, 0);

  EXPECT_EQ(0u, header->requested_size);
  EXPECT_EQ(&monitor, getMonitor(header));
  EXPECT_FALSE(isAlignedAlloc(header));
}

TEST(AllocHeaderTest, aligned_header_recovers_the_real_pointer) {
  alignas(4096) std::byte buffer[16384];
  StaticResourceMonitor monitor;

  for (auto alignment : kAlignments) {
    void* user = alignedUserPtr(buffer, alignment);
    auto* header = headerPtr(user);

    initAlignedHeader(header, &monitor, buffer, 777);

    EXPECT_TRUE(isAlignedAlloc(header)) << "alignment " << alignment;
    EXPECT_EQ(static_cast<void*>(buffer), getRealPtr(header))
        << "alignment " << alignment;
    EXPECT_EQ(777u, header->requested_size) << "alignment " << alignment;
  }
}

// The tag shares the monitor word, so masking must restore it exactly.
TEST(AllocHeaderTest, aligned_header_recovers_the_untagged_monitor) {
  alignas(4096) std::byte buffer[16384];
  StaticResourceMonitor monitor;

  for (auto alignment : kAlignments) {
    auto* header = headerPtr(alignedUserPtr(buffer, alignment));

    initAlignedHeader(header, &monitor, buffer, 777);

    EXPECT_EQ(&monitor, getMonitor(header)) << "alignment " << alignment;
  }
}

TEST(AllocHeaderTest, aligned_user_pointer_honours_the_requested_alignment) {
  alignas(4096) std::byte buffer[16384];

  for (auto alignment : kAlignments) {
    auto addr = reinterpret_cast<uintptr_t>(alignedUserPtr(buffer, alignment));
    EXPECT_EQ(0u, addr % alignment) << "alignment " << alignment;
  }
}

// The scheme rests on the metadata fitting the existing alignment gap. If the
// gap were smaller, initAlignedHeader would write below the allocation.
TEST(AllocHeaderTest, alignment_gap_always_holds_real_pointer_plus_header) {
  alignas(4096) std::byte buffer[16384];
  StaticResourceMonitor monitor;

  for (auto alignment : kAlignments) {
    void* user = alignedUserPtr(buffer, alignment);
    auto* header = headerPtr(user);
    initAlignedHeader(header, &monitor, buffer, 777);

    auto overhead = headerOverhead(header);
    EXPECT_EQ(static_cast<size_t>(static_cast<std::byte*>(user) - buffer),
              overhead)
        << "alignment " << alignment;
    EXPECT_GE(overhead, sizeof(AllocHeader) + sizeof(void*))
        << "alignment " << alignment;
  }
}
