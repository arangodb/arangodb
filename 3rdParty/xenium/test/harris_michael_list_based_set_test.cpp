#include <xenium/reclamation/lock_free_ref_count.hpp>
#include <xenium/reclamation/hazard_pointer.hpp>
#include <xenium/reclamation/hazard_eras.hpp>
#include <xenium/reclamation/epoch_based.hpp>
#include <xenium/reclamation/new_epoch_based.hpp>
#include <xenium/reclamation/quiescent_state_based.hpp>
#include <xenium/reclamation/debra.hpp>
#include <xenium/reclamation/generic_epoch_based.hpp>
#include <xenium/reclamation/stamp_it.hpp>
#include <xenium/harris_michael_list_based_set.hpp>

#include <gtest/gtest.h>

#include <vector>
#include <thread>

namespace {

template <typename Reclaimer>
struct HarrisMichaelListBasedSet : testing::Test {};

using Reclaimers = ::testing::Types<
    xenium::reclamation::lock_free_ref_count<>,
    xenium::reclamation::hazard_pointer<xenium::reclamation::static_hazard_pointer_policy<3>>,
    xenium::reclamation::hazard_eras<xenium::reclamation::static_hazard_eras_policy<3>>,
    xenium::reclamation::epoch_based<10>,
    xenium::reclamation::new_epoch_based<10>,
    xenium::reclamation::quiescent_state_based,
    xenium::reclamation::debra<20>,
    xenium::reclamation::stamp_it,
    xenium::reclamation::epoch_based2<>,
    xenium::reclamation::new_epoch_based2<>,
    xenium::reclamation::debra2<>
  >;
TYPED_TEST_CASE(HarrisMichaelListBasedSet, Reclaimers);

TYPED_TEST(HarrisMichaelListBasedSet, emplace_same_element_twice_fails_second_time)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  EXPECT_TRUE(list.emplace(42));
  EXPECT_FALSE(list.emplace(42));
}

TYPED_TEST(HarrisMichaelListBasedSet, emplace_or_get_inserts_new_element_and_returns_iterator_to_it)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  auto result = list.emplace_or_get(42);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(list.begin(), result.first);
  EXPECT_EQ(42, *result.first);
}

TYPED_TEST(HarrisMichaelListBasedSet, emplace_or_get_does_not_insert_anything_and_returns_iterator_to_existing_element)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(42);
  auto result = list.emplace_or_get(42);
  EXPECT_FALSE(result.second);
  EXPECT_EQ(list.begin(), result.first);
  EXPECT_EQ(42, *result.first);
}

TYPED_TEST(HarrisMichaelListBasedSet, contains_returns_false_for_non_existing_element)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(42);
  EXPECT_FALSE(list.contains(43));
}

TYPED_TEST(HarrisMichaelListBasedSet, constains_returns_true_for_existing_element)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(42);
  EXPECT_TRUE(list.contains(42));
}

TYPED_TEST(HarrisMichaelListBasedSet, find_returns_end_iterator_for_non_existing_element)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(43);
  EXPECT_EQ(list.end(), list.find(42));
}

TYPED_TEST(HarrisMichaelListBasedSet, find_returns_matching_iterator_for_existing_element)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(42);
  auto it = list.find(42);
  EXPECT_EQ(list.begin(), it);
  EXPECT_EQ(42, *it);
  EXPECT_EQ(list.end(), ++it);
}

TYPED_TEST(HarrisMichaelListBasedSet, comparer_policy_defines_order_of_entries)
{
  using my_list = xenium::harris_michael_list_based_set<int,
    xenium::policy::reclaimer<TypeParam>, xenium::policy::compare<std::greater<int>>>;
  my_list list;
  list.emplace(42);
  list.emplace(43);
  auto it = list.begin();
  EXPECT_EQ(43, *it);
  ++it;
  EXPECT_EQ(42, *it);
}

TYPED_TEST(HarrisMichaelListBasedSet, erase_existing_element_succeeds)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(42);
  EXPECT_TRUE(list.erase(42));
}

TYPED_TEST(HarrisMichaelListBasedSet, erase_nonexisting_element_fails)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  EXPECT_FALSE(list.erase(42));
}

TYPED_TEST(HarrisMichaelListBasedSet, erase_existing_element_twice_fails_the_seond_time)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(42);
  EXPECT_TRUE(list.erase(42));
  EXPECT_FALSE(list.erase(42));
}

TYPED_TEST(HarrisMichaelListBasedSet, erase_via_iterator_removes_entry_and_returns_iterator_to_successor)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(41);
  list.emplace(42);
  list.emplace(43);

  auto it = list.find(42);

  it = list.erase(std::move(it));
  ASSERT_NE(list.end(), it);
  EXPECT_EQ(43, *it);
  it = list.end(); // reset the iterator to clear all internal guard_ptrs

  EXPECT_FALSE(list.contains(42));
}

TYPED_TEST(HarrisMichaelListBasedSet, iterate_list)
{
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<TypeParam>> list;
  list.emplace(41);
  list.emplace(42);
  list.emplace(43);

  auto it = list.begin();
  EXPECT_EQ(41, *it);
  ++it;
  EXPECT_EQ(42, *it);
  ++it;
  EXPECT_EQ(43, *it);
  ++it;
  EXPECT_EQ(list.end(), it);
}

namespace
{
#ifdef DEBUG
  const int MaxIterations = 1000;
#else
  const int MaxIterations = 10000;
#endif
}

TYPED_TEST(HarrisMichaelListBasedSet, parallel_usage)
{
  using Reclaimer = TypeParam;
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<Reclaimer>> list;

  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i)
  {
    threads.push_back(std::thread([i, &list]
    {
      for (int j = 0; j < MaxIterations; ++j)
      {
        typename Reclaimer::region_guard critical_region{};
        EXPECT_EQ(list.end(), list.find(i));
        EXPECT_TRUE(list.emplace(i));
        auto it = list.find(i);
        ASSERT_NE(list.end(), it);
        EXPECT_EQ(i, *it);
        it.reset();
        EXPECT_TRUE(list.erase(i));
        auto result = list.emplace_or_get(i);
        ASSERT_NE(list.end(), result.first);
        EXPECT_TRUE(result.second);
        list.erase(std::move(result.first));

        for (auto& v : list)
          EXPECT_TRUE(v >= 0 && v < 8);
      }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}

TYPED_TEST(HarrisMichaelListBasedSet, parallel_usage_with_same_values)
{
  using Reclaimer = TypeParam;
  xenium::harris_michael_list_based_set<int, xenium::policy::reclaimer<Reclaimer>> list;

  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i)
  {
    threads.push_back(std::thread([&list]
    {
      for (int j = 0; j < MaxIterations / 10; ++j)
        for (int i = 0; i < 10; ++i)
        {
          typename Reclaimer::region_guard critical_region{};
          list.contains(i);
          list.emplace(i);
          auto it = list.find(i);
          it.reset();
          list.erase(i);
          auto result = list.emplace_or_get(i);
          if (result.second) {
            it = list.erase(std::move(result.first));
            it.reset();
          }
          result.first.reset();

          for (auto& v : list)
            EXPECT_TRUE(v >= 0 && v < 10);
        }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}

}