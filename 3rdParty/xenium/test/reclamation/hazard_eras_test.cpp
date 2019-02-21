#include <xenium/reclamation/hazard_eras.hpp>

#include <gtest/gtest.h>

namespace {

  struct my_static_hazard_eras_policy : xenium::reclamation::static_hazard_eras_policy<2>
  {
    // we are redifining this method in our own policy to enforce the
    // immediate reclamation of nodes.
    static constexpr size_t retired_nodes_threshold() { return 0; }
  };

  struct my_dynamic_hazard_eras_policy : xenium::reclamation::dynamic_hazard_eras_policy<2>
  {
    // we are redifining this method in our own policy to enforce the
    // immediate reclamation of nodes.
    static constexpr size_t retired_nodes_threshold() { return 0; }
  };

  template <typename Policy>
  struct HazardEras : ::testing::Test
  {
    using HE = xenium::reclamation::hazard_eras<Policy>;

    struct Foo : HE::template enable_concurrent_ptr<Foo, 2>
    {
      Foo** instance;
      Foo(Foo** instance) : instance(instance) {}
      virtual ~Foo() { if (instance) *instance = nullptr; }
    };

    struct Bar
    {
      int x;
      virtual ~Bar() {}
    };

    struct FooBar : Bar, Foo
    {
      FooBar(Foo** instance) : Foo(instance) {}
    };

    template <typename T>
    using concurrent_ptr = typename HE::template concurrent_ptr<T>;
    template <typename T> using marked_ptr = typename concurrent_ptr<T>::marked_ptr;

    Foo* foo = new Foo(&foo);
    marked_ptr<Foo> mp = marked_ptr<Foo>(foo, 3);

    void TearDown() override
    {
      // There might be some retired nodes remaining from a testcase that need to be reclaimed.
      // In order to do so we create a guard for a dummy object and mark it for reclamation.
      // This triggers reclamation of all the objects in the retired_list.
      advance_era();
      if (mp == nullptr)
        assert(foo == nullptr);
      else
        delete foo;
    }

    Foo* dummy2;
    void advance_era() {
      // In order to do advance the global era counter we create a dummy object and mark it for reclamation.
      Foo* dummy = new Foo(&dummy2);
      {
        typename concurrent_ptr<Foo>::guard_ptr gp(dummy);
        gp.reclaim();
      }
    }
  };

  using Policies = ::testing::Types<
    my_static_hazard_eras_policy,
    my_dynamic_hazard_eras_policy
  >;
  TYPED_TEST_CASE(HazardEras, Policies);

  TYPED_TEST(HazardEras, mark_returns_the_same_mark_as_the_original_marked_ptr)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
    EXPECT_EQ(this->mp.mark(), gp.mark());
  }

  TYPED_TEST(HazardEras, get_returns_the_same_pointer_as_the_original_marked_ptr)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
  }

  TYPED_TEST(HazardEras, acquire_guard_acquires_pointer)
  {
    using concurrent_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>;
    concurrent_ptr foo_ptr(this->mp);
    typename concurrent_ptr::guard_ptr gp = xenium::acquire_guard(foo_ptr);
    EXPECT_EQ(this->mp, gp);
  }

  TYPED_TEST(HazardEras, additional_acquire_call_do_not_lead_to_overallocation_of_HEs)
  {
    using concurrent_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>;
    concurrent_ptr foo_ptr(this->mp);
    typename concurrent_ptr::guard_ptr gp;
    gp.acquire(foo_ptr);
    gp.acquire(foo_ptr);
    gp.acquire(foo_ptr);
  }

  TYPED_TEST(HazardEras, acquire_if_equal_returns_true_and_acquires_pointer_when_values_are_equal)
  {
    using concurrent_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>;
    concurrent_ptr foo_ptr(this->mp);
    typename concurrent_ptr::guard_ptr gp;
    EXPECT_TRUE(gp.acquire_if_equal(foo_ptr, this->mp));
    EXPECT_EQ(this->mp, gp);
  }

  TYPED_TEST(HazardEras, acquire_if_equal_returns_false_and_resets_guard_when_values_are_not_equal)
  {
    using concurrent_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>;
    using Foo = typename TestFixture::Foo;
    concurrent_ptr foo_ptr(this->mp);
    typename concurrent_ptr::guard_ptr gp;
    Foo* other = new Foo(&other);
    std::unique_ptr<Foo> other_ptr(other);
    EXPECT_FALSE(gp.acquire_if_equal(foo_ptr, other));
    EXPECT_EQ(nullptr, gp.get());
  }
  TYPED_TEST(HazardEras, static_policy_throws_bad_hazard_era_alloc_when_HE_pool_is_exceeded)
  {
    if (std::is_same<TypeParam , my_dynamic_hazard_eras_policy>::value)
      return;

    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp1{this->mp};
    TestFixture::advance_era();
    guard_ptr gp2{this->mp};
    TestFixture::advance_era();
    EXPECT_THROW(
      guard_ptr gp3{this->mp},
      xenium::reclamation::bad_hazard_era_alloc
    );
  }

  TYPED_TEST(HazardEras, reset_releases_ownership_and_sets_pointer_to_null)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
    gp.reset();
    EXPECT_EQ(nullptr, gp.get());
  }

  TYPED_TEST(HazardEras, reclaim_releases_ownership_and_deletes_object_because_no_HE_protects_it)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
    gp.reclaim();
    this->mp = nullptr;
    EXPECT_EQ(nullptr, this->foo);
    EXPECT_EQ(nullptr, gp.get());
  }

  TYPED_TEST(HazardEras, object_cannot_be_reclaimed_as_long_as_another_guard_protects_it)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
    guard_ptr gp2(this->mp);
    gp.reclaim();
    this->mp = nullptr;
    EXPECT_NE(nullptr, this->foo);
  }

  TYPED_TEST(HazardEras, copy_constructor_leads_to_shared_ownership_preventing_the_object_from_beeing_reclaimed)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
    guard_ptr gp2(gp);
    gp.reclaim();
    this->mp = nullptr;
    EXPECT_NE(nullptr, this->foo);
  }

  TYPED_TEST(HazardEras, move_constructor_moves_ownership_and_resets_source_object)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
    guard_ptr gp2(std::move(gp));
    EXPECT_EQ(nullptr, gp.get());
    gp2.reclaim();
    this->mp = nullptr;
    EXPECT_EQ(nullptr, gp2.get());
    EXPECT_EQ(nullptr, this->foo);
  }

  TYPED_TEST(HazardEras, copy_assignment_leads_to_shared_ownership_preventing_the_object_from_beeing_reclaimed)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
    guard_ptr gp2{};
    gp2 = gp;
    gp.reclaim();
    this->mp = nullptr;
    EXPECT_NE(nullptr, this->foo);
  }

  TYPED_TEST(HazardEras, move_assignment_moves_ownership_and_resets_source_object)
  {
    using guard_ptr = typename TestFixture::template concurrent_ptr<typename TestFixture::Foo>::guard_ptr;
    guard_ptr gp(this->mp);
    guard_ptr gp2{};
    gp2 = std::move(gp);
    gp2.reclaim();
    this->mp = nullptr;
    EXPECT_EQ(nullptr, gp.get());
    EXPECT_EQ(nullptr, this->foo);
  }

  TYPED_TEST(HazardEras, dynamic_policy_can_protect_more_than_K_objects)
  {
    if (std::is_same<TypeParam , my_static_hazard_eras_policy>::value)
      return;

    const size_t count = 100;

    using Foo = typename TestFixture::Foo;
    using guard_ptr = typename TestFixture::template concurrent_ptr<Foo>::guard_ptr;

    std::vector<Foo*> foos(count);
    std::vector<guard_ptr> guards(100);
    std::vector<guard_ptr> guards2(100);

    for (size_t i = 0; i < count; ++i)
    {
      foos[i] = new Foo(&foos[i]);
      guards[i] = guard_ptr(foos[i]);
      guards2[i] = guard_ptr(foos[i]);
    }

    for (size_t i = 0; i < count; ++i)
      guards[i].reclaim();

    for (size_t i = 0; i < count; ++i)
      ASSERT_NE(nullptr, foos[i]);

    for (size_t i = 0; i < count; ++i)
      guards2[i].reset();

    // reclaim another dummy node to enforce a rescan and reclamation of the retired nodes.
    Foo* dummy = new Foo(&dummy);
    guard_ptr{dummy}.reclaim();

    for (size_t i = 0; i < count; ++i)
      EXPECT_EQ(nullptr, foos[i]);
  }
}
