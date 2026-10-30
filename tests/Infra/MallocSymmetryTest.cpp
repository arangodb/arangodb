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
///
/// free() assumes every pointer it receives carries one of our headers. An entry
/// point that allocates without going through our malloc, but is freed through
/// our free, makes free() read a foreign allocator's metadata as a monitor
/// pointer. These tests assert no such entry point exists.
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "infra/AllocHeader.h"
#include "infra/ResourceMonitorRef.h"
#include "infra/ResourceMonitorScope.h"
#include "infra/StaticResourceMonitor.h"

#define JEMALLOC_NO_DEMANGLE
#include <jemalloc/jemalloc.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <optional>
#include <unistd.h>

using namespace arangodb::infra;

namespace {

// arenas.lookup reports EINVAL for an address in none of jemalloc's extents, so
// it answers "did this come out of our malloc?" without trusting the bytes in
// front of the pointer. je_malloc_usable_size cannot: it is undefined, not
// false, on a foreign pointer.
auto jemallocOwns(void* block) -> bool {
  unsigned arena = 0;
  size_t len = sizeof(arena);
  return je_mallctl("arenas.lookup", &arena, &len, &block, sizeof(block)) == 0;
}

// At -O3 clang deletes a malloc/free pair whose pointer never escapes, leaving
// the test measuring nothing.
template<typename T>
void mustEscape(T& pointer) {
  asm volatile("" : : "r,m"(pointer) : "memory");
}

}  // namespace

class MallocSymmetryTest : public ::testing::Test {
 protected:
  // A monitor we own, so the header's attribution can be checked too.
  void SetUp() override { _scope.emplace(ResourceMonitorRef{_monitor}); }

  void TearDown() override { _scope.reset(); }

  // Recovers the block the way free() does, then asserts jemalloc owns it.
  void expectOurs(void* user, size_t requested, char const* entryPoint) {
    ASSERT_NE(nullptr, user) << entryPoint << " returned nullptr";
    auto* header = headerPtr(user);
    EXPECT_TRUE(jemallocOwns(getRealPtr(header)))
        << entryPoint << " did not route through our malloc";
    EXPECT_EQ(&_monitor, getMonitor(header))
        << entryPoint << " misattributed the allocation";
    EXPECT_EQ(requested, header->requested_size)
        << entryPoint << " recorded the wrong requested size";
  }

  // Declared before _scope so the monitor outlives the guard that installs it.
  StaticResourceMonitor _monitor;
  std::optional<ResourceMonitorScope> _scope;
};

TEST_F(MallocSymmetryTest, malloc_is_tracked) {
  void* p = malloc(100);
  expectOurs(p, 100, "malloc");
  free(p);
}

TEST_F(MallocSymmetryTest, calloc_is_tracked) {
  void* p = calloc(10, 8);
  expectOurs(p, 80, "calloc");
  free(p);
}

TEST_F(MallocSymmetryTest, realloc_is_tracked) {
  void* p = malloc(100);
  p = realloc(p, 200);
  expectOurs(p, 200, "realloc");
  free(p);
}

TEST_F(MallocSymmetryTest, realloc_of_nullptr_is_tracked) {
  void* p = realloc(nullptr, 100);
  expectOurs(p, 100, "realloc(nullptr)");
  free(p);
}

TEST_F(MallocSymmetryTest, posix_memalign_is_tracked) {
  void* p = nullptr;
  ASSERT_EQ(0, posix_memalign(&p, 64, 100));
  expectOurs(p, 100, "posix_memalign");
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(p) % 64);
  free(p);
}

TEST_F(MallocSymmetryTest, aligned_alloc_is_tracked) {
  void* p = aligned_alloc(64, 128);
  expectOurs(p, 128, "aligned_alloc");
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(p) % 64);
  free(p);
}

// libgomp calls memalign and releases with free, so this mismatch is live.
TEST_F(MallocSymmetryTest, memalign_is_tracked) {
  void* p = memalign(64, 100);
  expectOurs(p, 100, "memalign");
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(p) % 64);
  free(p);
}

TEST_F(MallocSymmetryTest, valloc_is_tracked) {
  auto pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  void* p = valloc(100);
  expectOurs(p, 100, "valloc");
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(p) % pageSize);
  free(p);
}

TEST_F(MallocSymmetryTest, pvalloc_is_tracked) {
  auto pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  void* p = pvalloc(100);
  // pvalloc rounds the request up to a whole number of pages.
  expectOurs(p, pageSize, "pvalloc");
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(p) % pageSize);
  free(p);
}

TEST_F(MallocSymmetryTest, reallocarray_is_tracked) {
  void* p = malloc(100);
  p = reallocarray(p, 10, 8);
  expectOurs(p, 80, "reallocarray");
  free(p);
}

TEST_F(MallocSymmetryTest, operator_new_is_tracked) {
  auto* p = new char[100];
  expectOurs(p, 100, "operator new[]");
  delete[] p;
}

// Not allocators, but they hand back memory the caller must free(). Pinned here
// so a glibc upgrade cannot quietly move them onto a non-interposed path.
TEST_F(MallocSymmetryTest, strdup_is_tracked) {
  char const* source = "0123456789";
  char* p = strdup(source);
  expectOurs(p, strlen(source) + 1, "strdup");
  free(p);
}

TEST_F(MallocSymmetryTest, asprintf_is_tracked) {
  char* p = nullptr;
  ASSERT_LT(0, asprintf(&p, "%s", "0123456789"));
  ASSERT_NE(nullptr, p);
  EXPECT_TRUE(jemallocOwns(getRealPtr(headerPtr(p))))
      << "asprintf did not route through our malloc";
  free(p);
}

TEST_F(MallocSymmetryTest, getcwd_is_tracked) {
  char* p = getcwd(nullptr, 0);
  ASSERT_NE(nullptr, p);
  EXPECT_TRUE(jemallocOwns(getRealPtr(headerPtr(p))))
      << "getcwd(nullptr, 0) did not route through our malloc";
  free(p);
}

// No gtest call may sit between the reads: gtest allocates, and those
// allocations land on the same monitor.
TEST_F(MallocSymmetryTest, accounting_is_symmetric) {
  auto before = _monitor.actualBytes();
  void* p = malloc(100);
  mustEscape(p);
  auto allocated = _monitor.actualBytes();
  free(p);
  auto after = _monitor.actualBytes();

  EXPECT_GT(allocated, before);
  EXPECT_EQ(before, after);
}
