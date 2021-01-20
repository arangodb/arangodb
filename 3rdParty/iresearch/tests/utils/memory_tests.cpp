////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/memory_pool.hpp"
#include "utils/memory.hpp"
#include "utils/timer_utils.hpp"

#include <fstream>
#include <list>
#include <set>
#include <map>

namespace {

struct unique_base {
  using ptr = std::unique_ptr<unique_base>;

  static ptr make() { return irs::memory::make_unique<unique_base>(); }
};

struct unique : unique_base { };

struct shared_base {
  using ptr = std::shared_ptr<shared_base>;

  static ptr make() { return irs::memory::make_shared<shared_base>(); }
};

struct shared : shared_base { };

class memory_pool_allocator_test: public test_base {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).

    test_base::SetUp();
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).

    test_base::TearDown();
  }
};

} // LOCAL

TEST(memory_test, allocate_unique_array_no_construct) {
  std::allocator<char> alloc;
  ASSERT_EQ(nullptr, irs::memory::allocate_unique<char[]>(alloc, 0, irs::memory::allocate_only));
  ASSERT_NE(nullptr, irs::memory::allocate_unique<char[]>(alloc, 256, irs::memory::allocate_only));
}

TEST(memory_test, allocate_unique_array) {
  std::allocator<char> alloc;
  ASSERT_EQ(nullptr, irs::memory::allocate_unique<char[]>(alloc, 0));
  ASSERT_NE(nullptr, irs::memory::allocate_unique<char[]>(alloc, 256));
}

TEST(memory_pool_test, init) {
  // default ctor
  {
    irs::memory::memory_pool<> pool;

    ASSERT_EQ(size_t(irs::memory::freelist::MIN_SIZE), pool.slot_size());
    ASSERT_EQ(0, pool.capacity());
    ASSERT_TRUE(pool.empty());
    ASSERT_EQ(32, pool.next_size());

    // allocate 1 node
    ASSERT_NE(nullptr, pool.allocate());
    ASSERT_EQ(32, pool.capacity());
    ASSERT_FALSE(pool.empty());
    ASSERT_EQ(64, pool.next_size()); // log2_grow by default
  }

  // initial size is too small
  {
    irs::memory::memory_pool<> pool(3, 0);

    ASSERT_EQ(size_t(irs::memory::freelist::MIN_SIZE), pool.slot_size());
    ASSERT_EQ(0, pool.capacity());
    ASSERT_TRUE(pool.empty());
    ASSERT_EQ(2, pool.next_size());

    // allocate 1 node
    ASSERT_NE(nullptr, pool.allocate());
    ASSERT_EQ(2, pool.capacity());
    ASSERT_FALSE(pool.empty());
    ASSERT_EQ(4, pool.next_size()); // log2_grow by default
  }
}

TEST(memory_pool_test, allocate_deallocate) {
  irs::memory::memory_pool<> pool(64);
  ASSERT_EQ(64, pool.slot_size());
  ASSERT_EQ(0, pool.capacity());
  ASSERT_TRUE(pool.empty());
  ASSERT_EQ(32, pool.next_size());

  // allocate 1 node
  ASSERT_NE(nullptr, pool.allocate());
  ASSERT_EQ(32, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_EQ(64, pool.next_size()); // log2_grow by default

  // allocate continious block of 31 nodes
  ASSERT_NE(nullptr, pool.allocate(31));
  ASSERT_EQ(32, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_EQ(64, pool.next_size()); // log2_grow by default

  // allocate 1 node (should cause pool growth)
  ASSERT_NE(nullptr, pool.allocate());
  ASSERT_EQ(32+64, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_EQ(128, pool.next_size()); // log2_grow by default

  // allocate 65 nodes (should cause pool growth)
  for (size_t i = 0; i < 65; ++i) {
    ASSERT_NE(nullptr, pool.allocate());
  }
  ASSERT_EQ(32+64+128, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_EQ(256, pool.next_size()); // log2_grow by default

  // allocate 126 nodes (should not cause pool growth)
  for (size_t i = 0; i < 126; ++i) {
    ASSERT_NE(nullptr, pool.allocate());
  }
  ASSERT_EQ(32+64+128, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_EQ(256, pool.next_size()); // log2_grow by default

  // allocate 1 node (should cause pool growth)
  ASSERT_NE(nullptr, pool.allocate());
  ASSERT_EQ(32+64+128+256, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_EQ(512, pool.next_size()); // log2_grow by default

  // allocate continious block of 256 slots (should cause pool growth)
  ASSERT_NE(nullptr, pool.allocate(256));
  ASSERT_EQ(32+64+128+256+512, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_EQ(1024, pool.next_size()); // log2_grow by default

  // allocate 255+256 nodes (should not cause pool growth)
  for (size_t i = 0; i < (255+256); ++i) {
    ASSERT_NE(nullptr, pool.allocate());
  }
  ASSERT_EQ(32+64+128+256+512, pool.capacity());
  ASSERT_FALSE(pool.empty());
  ASSERT_EQ(1024, pool.next_size()); // log2_grow by default
}

TEST_F(memory_pool_allocator_test, allocate_deallocate) {
  size_t ctor_calls = 0;
  size_t dtor_calls = 0;

  struct checker {
    checker(size_t& ctor_calls, size_t& dtor_calls)
      : dtor_calls_(&dtor_calls) {
      ++ctor_calls;
    }

    ~checker() {
      ++(*dtor_calls_);
    }

    size_t* dtor_calls_;
  }; // test

  irs::memory::memory_pool<> pool(0);
  irs::memory::memory_pool_allocator<checker, decltype(pool)> alloc(pool);

  auto* p = alloc.allocate(1);
  ASSERT_EQ(0, ctor_calls);
  ASSERT_EQ(0, dtor_calls);
  alloc.construct(p, ctor_calls, dtor_calls);
  ASSERT_EQ(1, ctor_calls);
  ASSERT_EQ(0, dtor_calls);
  alloc.destroy(p);
  ASSERT_EQ(1, ctor_calls);
  ASSERT_EQ(1, dtor_calls);
  alloc.deallocate(p, 1);
}

TEST_F(memory_pool_allocator_test, profile_std_map) {
  struct test_data {
    test_data(size_t i)
      : a(i), b(i), c(i), d(i) {
    }

    bool operator<(const test_data& rhs) const {
      return a < rhs.a;
    }

    size_t a;
    size_t b;
    size_t c;
    size_t d;
  };

  auto comparer = [](const test_data& lhs, const test_data& rhs) {
    return lhs.a == rhs.a;
  };

  const size_t size = 10000;
  iresearch::timer_utils::init_stats(true);

  // default allocator
  {
    std::map<size_t, test_data, std::less<test_data>> data;
    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("std::allocator");
      data.emplace(i, i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }
  }

#if !(defined(_MSC_VER) && defined(IRESEARCH_DEBUG))
  // MSVC in debug mode sets _ITERATOR_DEBUG_LEVEL equal to 2 (by default) which
  // causes modification of the containers/iterators layout. Internally, containers
  // allocate auxillary "Proxy" objects which are have to be created with the help
  // of the provided allocator using "rebind".
  // In that case allocator should handle creation of more than one types of objects
  // with the different layout what is impossible for simple segregated storage allocator.
  // That's why we have to use memory_pool_multi_size_allocator instead of
  // memory_poolallocator to deal with that MSVC feature.

  // pool allocator
  {
    irs::memory::memory_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(0);

    typedef irs::memory::memory_pool_allocator<
      std::pair<const size_t, test_data>,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::map<size_t, test_data, std::less<size_t>, alloc_t> data(
      std::less<size_t>(), alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator");
      data.emplace(i, i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(i);
      if (i % 2) {
        ASSERT_EQ(data.end(), it);
      } else {
        ASSERT_NE(data.end(), it);
        ASSERT_EQ(i, it->second.a);
      }
    }
  }
#endif

  // mutli-size pool allocator
  {
    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool;

    typedef irs::memory::memory_pool_multi_size_allocator<
      std::pair<size_t const, test_data>,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::map<size_t, test_data, std::less<size_t>, alloc_t> data(
      std::less<size_t>{}, alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(multi-size)");
      data.emplace(i, i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(i);
      if (i % 2) {
        ASSERT_EQ(data.end(), it);
      } else {
        ASSERT_NE(data.end(), it);
        ASSERT_EQ(i, it->second.a);
      }
    }
  }

  // mutli-size pool allocator (custom initial size)
  {
    const size_t initial_size = 128;

    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(initial_size);

    typedef irs::memory::memory_pool_multi_size_allocator<
      std::pair<size_t const, test_data>,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::map<size_t, test_data, std::less<size_t>, alloc_t> data(
      std::less<size_t>(), alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(multi-size,initial_size==128)");
      data.emplace(i, i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(i);
      if (i % 2) {
        ASSERT_EQ(data.end(), it);
      } else {
        ASSERT_NE(data.end(), it);
        ASSERT_EQ(i, it->second.a);
      }
    }
  }

  auto path = test_dir();

  path /= "profile_memory_pool_allocator.log";

  std::ofstream out(path.native());

  irs::timer_utils::flush_stats(out);
  out.close();
  std::cout << "Path to timing log: " << path.utf8_absolute() << std::endl;
}

TEST_F(memory_pool_allocator_test, profile_std_multimap) {
  struct test_data {
    test_data(size_t i)
      : a(i), b(i), c(i), d(i) {
    }

    bool operator<(const test_data& rhs) const {
      return a < rhs.a;
    }

    size_t a;
    size_t b;
    size_t c;
    size_t d;
  };

  auto comparer = [](const test_data& lhs, const test_data& rhs) {
    return lhs.a == rhs.a;
  };

  const size_t size = 10000;
  iresearch::timer_utils::init_stats(true);

  // default allocator
  {
    std::multimap<size_t, test_data, std::less<test_data>> data;
    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("std::allocator");
      data.emplace(i, i);
      data.emplace(i, i);
      data.erase(--data.end());
    }
  }

#if !(defined(_MSC_VER) && defined(IRESEARCH_DEBUG))
  // MSVC in debug mode sets _ITERATOR_DEBUG_LEVEL equal to 2 (by default) which
  // causes modification of the containers/iterators layout. Internally, containers
  // allocate auxillary "Proxy" objects which are have to be created with the help
  // of the provided allocator using "rebind".
  // In that case allocator should handle creation of more than one types of objects
  // with the different layout what is impossible for simple segregated storage allocator.
  // That's why we have to use memory_pool_multi_size_allocator instead of
  // memory_pool_allocator to deal with that MSVC feature.

  // pool allocator (default ctor)
  {
    irs::memory::memory_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(0);

    typedef irs::memory::memory_pool_allocator<
      std::pair<size_t const, test_data>,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;
    
    alloc_t alloc(pool);
    std::multimap<size_t, test_data, std::less<size_t>, alloc_t> data(
      std::less<size_t>(), alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator");
      data.emplace(i, i);
      data.emplace(i, i);
      data.erase(--data.end());
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(i);
      ASSERT_NE(data.end(), it);
      ASSERT_EQ(i, it->second.a);
    }
  }
#endif

#if !(defined(_MSC_VER) && defined(IRESEARCH_DEBUG))
  // MSVC in debug mode sets _ITERATOR_DEBUG_LEVEL equal to 2 (by default) which
  // causes modification of the containers/iterators layout. Internally, containers
  // allocate auxillary "Proxy" objects which are have to be created with the help
  // of the provided allocator using "rebind".
  // In that case allocator should handle creation of more than one types of objects
  // with the different layout what is impossible for simple segregated storage allocator.
  // That's why we have to use memory_pool_multi_size_allocator instead of
  // memory_pool_allocator to deal with that MSVC feature.

  // pool allocator (specify size)
  {
    const size_t initial_size = 128;

    irs::memory::memory_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(0, initial_size);

    typedef irs::memory::memory_pool_allocator<
      std::pair<size_t const, test_data>,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::multimap<size_t, test_data, std::less<size_t>, alloc_t> data(
      std::less<size_t>(), alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(initial_size==128)");
      data.emplace(i, i);
      data.emplace(i, i);
      data.erase(--data.end());
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(i);
      ASSERT_NE(data.end(), it);
      ASSERT_EQ(i, it->second.a);
    }
  }
#endif

  // mutli-size pool allocator
  {
    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool;

    typedef irs::memory::memory_pool_multi_size_allocator<
      std::pair<size_t const, test_data>,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::multimap<size_t, test_data, std::less<size_t>, alloc_t> data(
      std::less<size_t>(), alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(multi-size)");
      data.emplace(i, i);
      data.emplace(i, i);
      data.erase(--data.end());
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(i);
      ASSERT_NE(data.end(), it);
      ASSERT_EQ(i, it->second.a);
    }
  }

  // mutli-size pool allocator (custom initial size)
  {
    const size_t initial_size =128;

    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(initial_size);

    typedef irs::memory::memory_pool_multi_size_allocator<
      std::pair<size_t const, test_data>,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::multimap<size_t, test_data, std::less<size_t>, alloc_t> data(
       std::less<size_t>(), alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(multi-size,initial_size==128)");
      data.emplace(i, i);
      data.emplace(i, i);
      data.erase(--data.end());
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(i);
      ASSERT_NE(data.end(), it);
      ASSERT_EQ(i, it->second.a);
    }
  }

  auto path = test_dir();

  path /= "profile_memory_pool_allocator.log";

  std::ofstream out(path.native());

  irs::timer_utils::flush_stats(out);
  out.close();
  std::cout << "Path to timing log: " << path.utf8_absolute() << std::endl;
}

TEST_F(memory_pool_allocator_test, profile_std_list) {
  struct test_data {
    test_data(size_t i)
      : a(i), b(i), c(i), d(i) {
    }

    size_t a;
    size_t b;
    size_t c;
    size_t d;
  };

  const size_t size = 10000;
  iresearch::timer_utils::init_stats(true);

  // default allocator
  {
    std::list<test_data> data;
    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("std::allocator");
      data.emplace_back(i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }
  }

#if !(defined(_MSC_VER) && defined(IRESEARCH_DEBUG))
  // MSVC in debug mode sets _ITERATOR_DEBUG_LEVEL equal to 2 (by default) which
  // causes modification of the containers/iterators layout. Internally, containers
  // allocate auxillary "Proxy" objects which are have to be created with the help
  // of the provided allocator using "rebind".
  // In that case allocator should handle creation of more than one types of objects
  // with the different layout what is impossible for simple segregated storage allocator.
  // That's why we have to use memory_pool_multi_size_allocator instead of
  // memory_pool_allocator to deal with that MSVC feature.

  // pool allocator (default size)
  {
    irs::memory::memory_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(0);

    typedef irs::memory::memory_pool_allocator<
      test_data,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::list<test_data, alloc_t> data(alloc);

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator");
      data.emplace_back(i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    size_t i = 0;
    for (auto& item : data) {
      ASSERT_EQ(i, item.a);
      i += 2;
    }
  }
#endif

  // mutli-size pool allocator
  {
    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool;

    typedef irs::memory::memory_pool_multi_size_allocator<
      test_data,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::list<test_data, alloc_t> data(alloc);

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(multi-size)");
      data.emplace_back(i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    size_t i = 0;
    for (auto& item : data) {
      ASSERT_EQ(i, item.a);
      i += 2;
    }
  }

  // mutli-size pool allocator (custom initial size)
  {
    const size_t initial_size = 128;

    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(initial_size);

    typedef irs::memory::memory_pool_multi_size_allocator<
      test_data,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::list<test_data, alloc_t> data(alloc);

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(multi-size,initial_size==128)");
      data.emplace_back(i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    size_t i = 0;
    for (auto& item : data) {
      ASSERT_EQ(i, item.a);
      i += 2;
    }
  }

  auto path = test_dir();

  path /= "profile_memory_pool_allocator.log";

  std::ofstream out(path.native());

  irs::timer_utils::flush_stats(out);
  out.close();
  std::cout << "Path to timing log: " << path.utf8_absolute() << std::endl;
}

TEST_F(memory_pool_allocator_test, profile_std_set) {
  struct test_data {
    test_data(size_t i)
      : a(i), b(i), c(i), d(i) {
    }

    bool operator<(const test_data& rhs) const {
      return a < rhs.a;
    }

    size_t a;
    size_t b;
    size_t c;
    size_t d;
  };

  auto comparer = [](const test_data& lhs, const test_data& rhs) {
    return lhs.a == rhs.a;
  };

  const size_t size = 10000;
  iresearch::timer_utils::init_stats(true);

  // default allocator
  {
    std::set<test_data, std::less<test_data>> data;
    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("std::allocator");
      data.emplace(i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }
  }

#if !(defined(_MSC_VER) && defined(IRESEARCH_DEBUG))
  // MSVC in debug mode sets _ITERATOR_DEBUG_LEVEL equal to 2 (by default) which
  // causes modification of the containers/iterators layout. Internally, containers
  // allocate auxillary "Proxy" objects which are have to be created with the help
  // of the provided allocator using "rebind".
  // In that case allocator should handle creation of more than one types of objects
  // with the different layout what is impossible for simple segregated storage allocator.
  // That's why we have to use memory_pool_multi_size_allocator instead of
  // memory_pool_allocator to deal with that MSVC feature.

  // pool allocator (default ctor)
  {
    irs::memory::memory_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(0);

    typedef irs::memory::memory_pool_allocator<
      test_data,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::set<test_data, std::less<test_data>, alloc_t> data(
      std::less<test_data>(), alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator");
      data.emplace(i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(test_data(i));
      if (i % 2) {
        ASSERT_EQ(data.end(), it);
      } else {
        ASSERT_NE(data.end(), it);
        ASSERT_EQ(i, it->a);
      }
    }
  }
#endif

  // mutli-size pool allocator
  {
    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool;

    typedef irs::memory::memory_pool_multi_size_allocator<
      test_data,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::set<test_data, std::less<test_data>, alloc_t> data(
      std::less<test_data>(), alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(multi-size)");
      data.emplace(i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(test_data(i));
      if (i % 2) {
        ASSERT_EQ(data.end(), it);
      } else {
        ASSERT_NE(data.end(), it);
        ASSERT_EQ(i, it->a);
      }
    }
  }

  // mutli-size pool allocator (custom initial size)
  {
    const size_t initial_size = 128;

    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(initial_size);

    typedef irs::memory::memory_pool_multi_size_allocator<
      test_data,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc_t;

    alloc_t alloc(pool);
    std::set<test_data, std::less<test_data>, alloc_t> data(
      std::less<test_data>{}, alloc
    );

    for (size_t i = 0; i < size; ++i) {
      SCOPED_TIMER("irs::allocator(multi-size,initial_size==128)");
      data.emplace(i);
      if (i % 2) {
        data.erase(--data.end());
      }
    }

    // check data
    for (size_t i = 0; i < size; ++i) {
      const auto it = data.find(test_data(i));
      if (i % 2) {
        ASSERT_EQ(data.end(), it);
      } else {
        ASSERT_NE(data.end(), it);
        ASSERT_EQ(i, it->a);
      }
    }
  }

  auto path = test_dir();

  path /= "profile_memory_pool_allocator.log";

  std::ofstream out(path.native());

  irs::timer_utils::flush_stats(out);
  out.close();
  std::cout << "Path to timing log: " << path.utf8_absolute() << std::endl;
}

TEST_F(memory_pool_allocator_test, allocate_unique) {
  size_t ctor_calls = 0;
  size_t dtor_calls = 0;

  struct checker {
    checker(size_t& ctor_calls, size_t& dtor_calls)
      : dtor_calls_(&dtor_calls) {
      ++ctor_calls;
    }

    ~checker() {
      ++(*dtor_calls_);
    }

    size_t* dtor_calls_;
  }; // test

  irs::memory::memory_pool<
    irs::memory::identity_grow,
    irs::memory::malloc_free_allocator
  > pool(0);

  irs::memory::memory_pool_allocator<
    checker,
    decltype(pool),
    irs::memory::single_allocator_tag
  > alloc(pool);

  {
    auto ptr = irs::memory::allocate_unique<checker>(alloc, ctor_calls, dtor_calls);
    ASSERT_EQ(1, ctor_calls);
    ASSERT_EQ(0, dtor_calls);
  }
  ASSERT_EQ(1, ctor_calls);
  ASSERT_EQ(1, dtor_calls);
}

TEST_F(memory_pool_allocator_test, allocate_shared) {
  size_t ctor_calls = 0;
  size_t dtor_calls = 0;

  struct checker {
    checker(size_t& ctor_calls, size_t& dtor_calls)
      : dtor_calls_(&dtor_calls) {
      ++ctor_calls;
    }

    ~checker() {
      ++(*dtor_calls_);
    }

    size_t* dtor_calls_;
  }; // test

#if !(defined(_MSC_VER) && defined(IRESEARCH_DEBUG))
  // MSVC in debug mode sets _ITERATOR_DEBUG_LEVEL equal to 2 (by default) which
  // causes modification of the containers/iterators layout. Internally, containers
  // allocate auxillary "Proxy" objects which are have to be created with the help
  // of the provided allocator using "rebind".
  // In that case allocator should handle creation of more than one types of objects
  // with the different layout what is impossible for simple segregated storage allocator.
  // That's why we have to use memory_pool_multi_size_allocator instead of
  // memory_pool_allocator to deal with that MSVC feature.

  // pool allocator
  {
    ctor_calls = 0;
    dtor_calls = 0;

    irs::memory::memory_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool(0);

    irs::memory::memory_pool_allocator<
      checker,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc(pool);

    {
      auto ptr = std::allocate_shared<checker>(alloc, ctor_calls, dtor_calls);
      ASSERT_EQ(1, ctor_calls);
      ASSERT_EQ(0, dtor_calls);
    }
    ASSERT_EQ(1, ctor_calls);
    ASSERT_EQ(1, dtor_calls);
  }
#endif

  // multi-size allocator
  {
    ctor_calls = 0;
    dtor_calls = 0;

    irs::memory::memory_multi_size_pool<
      irs::memory::identity_grow,
      irs::memory::malloc_free_allocator
    > pool;

    irs::memory::memory_pool_multi_size_allocator<
      checker,
      decltype(pool),
      irs::memory::single_allocator_tag
    > alloc(pool);

    {
      auto ptr = std::allocate_shared<checker>(alloc, ctor_calls, dtor_calls);
      ASSERT_EQ(1, ctor_calls);
      ASSERT_EQ(0, dtor_calls);
    }
    ASSERT_EQ(1, ctor_calls);
    ASSERT_EQ(1, dtor_calls);
  }
}
