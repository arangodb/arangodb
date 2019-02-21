#include <xenium/reclamation/detail/marked_ptr.hpp>
#include <xenium/reclamation/detail/concurrent_ptr.hpp>

#include <gtest/gtest.h>

namespace {
 
struct Foo {
  int x;
  static constexpr int number_of_mark_bits = 2;
};

template <class T, class MarkedPtr>
struct my_guard_ptr
{
  my_guard_ptr(const MarkedPtr& p = MarkedPtr()) : p(p) {}
  MarkedPtr p;

  T* get() const { return p.get(); }
  uintptr_t mark() const { return p.mark(); }
  operator MarkedPtr() const noexcept { return p; }
};

template <typename T>
using concurrent_ptr = xenium::reclamation::detail::concurrent_ptr<T, T::number_of_mark_bits, my_guard_ptr>;

TEST(concurrent_ptr, get_returns_pointer_that_was_passed_to_constructor)
{
  Foo f;
  concurrent_ptr<Foo>::marked_ptr mp(&f);
  concurrent_ptr<Foo> p(mp);

  EXPECT_EQ(&f, p.load().get());
}

TEST(concurrent_ptr, get_returns_pointer_that_was_passed_to_store)
{
  Foo f;
  concurrent_ptr<Foo>::marked_ptr mp(&f);
  concurrent_ptr<Foo> p;
  p.store(mp);

  EXPECT_EQ(&f, p.load().get());
}

TEST(concurrent_ptr, compare_exchange_weak_sets_value_and_returns_true_when_expected_value_matches)
{
  Foo f1, f2;
  concurrent_ptr<Foo>::marked_ptr mp1(&f1);
  concurrent_ptr<Foo>::marked_ptr mp2(&f2);
  concurrent_ptr<Foo> p(mp1);
  bool success = p.compare_exchange_weak(mp1, mp2);

  EXPECT_TRUE(success);
  EXPECT_EQ(&f2, p.load().get());
}

TEST(concurrent_ptr, compare_exchange_weak_value_remains_unchanged_and_returns_false_when_expected_value_does_not_match)
{
  Foo f1, f2;
  concurrent_ptr<Foo>::marked_ptr mp1(&f1);
  concurrent_ptr<Foo>::marked_ptr mp2(&f2);
  concurrent_ptr<Foo> p(mp1);
  bool success = p.compare_exchange_weak(mp2, mp2);

  EXPECT_FALSE(success);
  EXPECT_EQ(&f1, p.load().get());
}

TEST(concurrent_ptr, compare_exchange_weak_with_guard_ptr)
{
  Foo f1, f2;
  concurrent_ptr<Foo>::marked_ptr mp(&f1);
  concurrent_ptr<Foo>::guard_ptr gp(&f2);
  concurrent_ptr<Foo> p(mp);
  p.compare_exchange_weak(mp, gp);

  EXPECT_EQ(&f2, p.load().get());
}
}
