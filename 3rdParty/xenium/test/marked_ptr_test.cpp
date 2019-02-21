#include <xenium/reclamation/detail/marked_ptr.hpp>

#include <gtest/gtest.h>

using namespace xenium::reclamation;

namespace {
 
struct Foo {
  int x;
  static constexpr int number_of_mark_bits = 0;
};

TEST(marked_ptr, get_returns_correct_pointer)
{
  Foo f;
  detail::marked_ptr<Foo, 2> p(&f, 3);
  EXPECT_EQ(&f, p.get());
  EXPECT_EQ(3, p.mark());
}

TEST(marked_ptr, deref_works_correctly)
{
  Foo f;
  detail::marked_ptr<Foo, 2> p(&f, 3);
  p->x = 42;
  EXPECT_EQ(42, f.x);

  (*p).x = 43;
  EXPECT_EQ(43, f.x);
}

TEST(marked_ptr, reset_sets_ptr_to_null)
{
  Foo f;
  detail::marked_ptr<Foo, 2> p(&f, 3);
  p.reset();
  EXPECT_EQ(nullptr, p.get());
}

}
