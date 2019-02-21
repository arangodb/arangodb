#include <xenium/reclamation/lock_free_ref_count.hpp>

#include <gtest/gtest.h>
#include <thread>

namespace {

struct Foo : xenium::reclamation::lock_free_ref_count<>::enable_concurrent_ptr<Foo, 2>
{
  Foo** instance;
  Foo(Foo** instance) : instance(instance) {}
  virtual ~Foo() { *instance = nullptr; }
};

template <typename T> using concurrent_ptr = xenium::reclamation::lock_free_ref_count<>::concurrent_ptr<T>;
template <typename T> using marked_ptr = typename concurrent_ptr<T>::marked_ptr;
template <typename T> using guard_ptr = typename concurrent_ptr<T>::guard_ptr;

struct LockFreeRefCount : testing::Test
{
  Foo* foo = new Foo(&foo);
  marked_ptr<Foo> mp = marked_ptr<Foo>(foo, 3);

  void TearDown() override { delete foo; }
};

TEST_F(LockFreeRefCount, inital_ref_count_value_is_one)
{
  guard_ptr<Foo> g;
  marked_ptr<Foo> m(g);
  EXPECT_EQ(1, foo->refs());
}

TEST_F(LockFreeRefCount, mark_returns_the_same_mark_as_the_original_marked_ptr)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  EXPECT_EQ(mp.mark(), gp.mark());
}

TEST_F(LockFreeRefCount, get_returns_the_same_pointer_as_the_original_marked_ptr)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  EXPECT_EQ(mp.get(), gp.get());
}

TEST_F(LockFreeRefCount, reset_releases_ownership)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  gp.reset();
  EXPECT_EQ(nullptr, gp.get());
}

TEST_F(LockFreeRefCount, reclaim_releases_ownership_and_deletes_object_if_ref_count_drops_to_zero)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  gp.reclaim();
  EXPECT_EQ(nullptr, foo);
  EXPECT_EQ(nullptr, gp.get());
}

TEST_F(LockFreeRefCount, guard_increments_ref_count)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  EXPECT_EQ(2, mp->refs());
}

TEST_F(LockFreeRefCount, copy_constructor_increments_ref_count)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2(gp);
  EXPECT_EQ(3, mp->refs());
}

TEST_F(LockFreeRefCount, move_constructor_does_not_increment_ref_count_and_resets_source)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2(std::move(gp));
  EXPECT_EQ(2, mp->refs());
  EXPECT_EQ(nullptr, gp.get());
}

TEST_F(LockFreeRefCount, copy_assignment_increments_ref_count)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2{};
  gp2 = gp;
  EXPECT_EQ(3, mp->refs());
}

TEST_F(LockFreeRefCount, move_assignment_does_not_increment_ref_count_and_resets_source)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2{};
  gp2 = std::move(gp);
  EXPECT_EQ(2, mp->refs());
  EXPECT_EQ(nullptr, gp.get());
}

TEST_F(LockFreeRefCount, guard_destructor_decrements_ref_count)
{
  {
    concurrent_ptr<Foo>::guard_ptr gp(mp);
  }
  EXPECT_EQ(1, foo->refs());
}

TEST_F(LockFreeRefCount, parallel_allocation_and_deallocation_of_nodes)
{
  struct Dummy : xenium::reclamation::lock_free_ref_count<>::enable_concurrent_ptr<Dummy> {};

  std::vector<std::thread> threads;
  for (int i = 0; i < 16; ++i)
  {
    threads.push_back(std::thread([]
    {
      const int MaxIterations = 10000;
      for (int j = 0; j < MaxIterations; ++j)
      {
        auto o = new Dummy;
        guard_ptr<Dummy> g(new Dummy);
        delete o;
        g.reclaim();
      }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}
}
