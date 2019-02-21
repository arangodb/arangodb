#include <xenium/reclamation/lock_free_ref_count.hpp>
#include <xenium/reclamation/hazard_pointer.hpp>
#include <xenium/reclamation/hazard_eras.hpp>
#include <xenium/reclamation/epoch_based.hpp>
#include <xenium/reclamation/new_epoch_based.hpp>
#include <xenium/reclamation/quiescent_state_based.hpp>
#include <xenium/reclamation/debra.hpp>
#include <xenium/reclamation/generic_epoch_based.hpp>
#include <xenium/reclamation/stamp_it.hpp>
#include <xenium/harris_michael_hash_map.hpp>

#include <gtest/gtest.h>

#include <vector>
#include <thread>

namespace {

template <typename Reclaimer>
struct HarrisMichaelHashMap : ::testing::Test
{
  using hash_map = xenium::harris_michael_hash_map<int, int,
    xenium::policy::reclaimer<Reclaimer>,
    xenium::policy::buckets<10>>;
  hash_map map;
};

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
TYPED_TEST_CASE(HarrisMichaelHashMap, Reclaimers);

TYPED_TEST(HarrisMichaelHashMap, emplace_or_get_returns_an_iterator_and_true_when_successful)
{
  auto result = this->map.emplace_or_get(42, 43);
  EXPECT_TRUE(result.second);
  ASSERT_NE(this->map.end(), result.first);
  EXPECT_EQ(42, result.first->first);
  EXPECT_EQ(43, result.first->second);
}

TYPED_TEST(HarrisMichaelHashMap, emplace_or_get_for_an_existing_element_returns_an_iterator_to_that_element_and_false)
{
  EXPECT_TRUE(this->map.emplace(42, 43));

  auto result = this->map.emplace_or_get(42, 44);
  EXPECT_FALSE(result.second);
  ASSERT_NE(this->map.end(), result.first);
  EXPECT_EQ(42, result.first->first);
  EXPECT_EQ(43, result.first->second);
}

TYPED_TEST(HarrisMichaelHashMap, get_or_emplace_returns_iteratur_to_newly_inserted_element)
{
  auto result = this->map.get_or_emplace(42, 43);
  EXPECT_TRUE(result.second);
  ASSERT_NE(this->map.end(), result.first);
  EXPECT_EQ(42, result.first->first);
  EXPECT_EQ(43, result.first->second);
}

TYPED_TEST(HarrisMichaelHashMap, get_or_emplace_returns_iteratur_to_existing_element)
{
  this->map.emplace(42, 42);
  auto result = this->map.get_or_emplace(42, 43);
  EXPECT_FALSE(result.second);
  ASSERT_NE(this->map.end(), result.first);
  EXPECT_EQ(42, result.first->first);
  EXPECT_EQ(42, result.first->second);
}

TYPED_TEST(HarrisMichaelHashMap, get_or_emplace_lazy_calls_factory_and_returns_iteratur_to_newly_inserted_element)
{
  bool called_factory = false;
  auto result = this->map.get_or_emplace_lazy(42,
    [&](){
      called_factory = true;
      return 43;
    });
  EXPECT_TRUE(result.second);
  ASSERT_NE(this->map.end(), result.first);
  EXPECT_EQ(42, result.first->first);
  EXPECT_EQ(43, result.first->second);
}

TYPED_TEST(HarrisMichaelHashMap, get_or_emplace_lazy_does_not_call_factory_and_returns_iterator_to_existing_element)
{
  bool called_factory = false;
  this->map.emplace(42, 42);
  auto result = this->map.get_or_emplace_lazy(42,
                                              [&](){
                                                called_factory = true;
                                                return 43;
                                              });
  EXPECT_FALSE(result.second);
  ASSERT_NE(this->map.end(), result.first);
  EXPECT_EQ(42, result.first->first);
  EXPECT_EQ(42, result.first->second);
}

TYPED_TEST(HarrisMichaelHashMap, containts_returns_false_for_non_existing_element)
{
  EXPECT_FALSE(this->map.contains(43));
}

TYPED_TEST(HarrisMichaelHashMap, contains_returns_true_for_existing_element)
{
  this->map.emplace(42, 43);
  EXPECT_TRUE(this->map.contains(42));
}

TYPED_TEST(HarrisMichaelHashMap, find_returns_iterator_to_existing_element)
{
  this->map.emplace(42, 43);
  auto it = this->map.find(42);
  ASSERT_NE(this->map.end(), it);
  EXPECT_EQ(42, it->first);
  EXPECT_EQ(43, it->second);
}

TYPED_TEST(HarrisMichaelHashMap, find_returns_end_iterator_for_non_existing_element)
{
  for (int i = 0; i < 200; ++i) {
    if (i != 42)
      this->map.emplace(i, i);
  }
  EXPECT_EQ(this->map.end(), this->map.find(42));
}

TYPED_TEST(HarrisMichaelHashMap, erase_nonexisting_element_returns_false)
{
  EXPECT_FALSE(this->map.erase(42));
}

TYPED_TEST(HarrisMichaelHashMap, erase_existing_element_returns_true_and_removes_element)
{
  this->map.emplace(42, 43);
  EXPECT_TRUE(this->map.erase(42));
}

TYPED_TEST(HarrisMichaelHashMap, erase_existing_element_twice_fails_the_seond_time)
{
  this->map.emplace(42, 43);
  EXPECT_TRUE(this->map.erase(42));
  EXPECT_FALSE(this->map.erase(42));
}

TYPED_TEST(HarrisMichaelHashMap, begin_returns_end_iterator_for_empty_map)
{
  auto it = this->map.begin();
  ASSERT_EQ(this->map.end(), it);
}

TYPED_TEST(HarrisMichaelHashMap, begin_returns_iterator_to_first_entry)
{
  this->map.emplace(42, 43);
  auto it = this->map.begin();
  ASSERT_NE(this->map.end(), it);
  EXPECT_EQ(42, it->first);
  EXPECT_EQ(43, it->second);
}

TYPED_TEST(HarrisMichaelHashMap, drain_densely_populated_map_using_erase)
{
  for (int i = 0; i < 200; ++i)
    this->map.emplace(i, i);

  auto it = this->map.begin();
  while (it != this->map.end())
    it = this->map.erase(std::move(it));

  EXPECT_EQ(this->map.end(), this->map.begin());
}

TYPED_TEST(HarrisMichaelHashMap, drain_sparsely_populated_map_using_erase)
{
  for (int i = 0; i < 4; ++i)
    this->map.emplace(i * 7, i);

  auto it = this->map.begin();
  while (it != this->map.end())
    it = this->map.erase(std::move(it));

  EXPECT_EQ(this->map.end(), this->map.begin());
}

TYPED_TEST(HarrisMichaelHashMap, iterator_covers_all_entries_in_densely_populated_map)
{
  std::map<int, bool> values;
  for (int i = 0; i < 200; ++i) {
    values[i] = false;
    this->map.emplace(i, i);
  }
  for (auto& v : this->map)
    values[v.first] = true;

  for (auto& v : values)
    EXPECT_TRUE(v.second) << v.first << " was not visited";
}

TYPED_TEST(HarrisMichaelHashMap, iterator_covers_all_entries_in_sparsely_populated_map)
{
  std::map<int, bool> values;
  for (int i = 0; i < 4; ++i) {
    values[i * 7] = false;
    this->map.emplace(i * 7, i);
  }
  for (auto& v : this->map)
    values[v.first] = true;

  for (auto& v : values)
    EXPECT_TRUE(v.second) << v.first << " was not visited";
}

TYPED_TEST(HarrisMichaelHashMap, operator_at_returns_accessor_to_existing_element)
{
  using Reclaimer = TypeParam;
  using hash_map = xenium::harris_michael_hash_map<int, std::string,
    xenium::policy::reclaimer<Reclaimer>,
    xenium::policy::buckets<10>>;
  hash_map map;

  const char* val = "foobar";
  map.emplace(42, val);
  auto accessor = map[42];
  EXPECT_STREQ(val, accessor->c_str());
}

TYPED_TEST(HarrisMichaelHashMap, operator_at_returns_accessor_to_newly_inserted_element)
{
  using Reclaimer = TypeParam;
  using hash_map = xenium::harris_michael_hash_map<int, std::string,
    xenium::policy::reclaimer<Reclaimer>,
    xenium::policy::buckets<10>>;
  hash_map map;

  auto accessor = map[42];
  EXPECT_EQ("", *accessor);
}

namespace
{
#ifdef DEBUG
  const int MaxIterations = 1000;
#else
  const int MaxIterations = 10000;
#endif
}

TYPED_TEST(HarrisMichaelHashMap, parallel_usage)
{
  using Reclaimer = TypeParam;

  using hash_map = xenium::harris_michael_hash_map<std::string, int,
    xenium::policy::reclaimer<Reclaimer>,
    xenium::policy::buckets<10>>;
  hash_map map;

  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i)
  {
    threads.push_back(std::thread([i, &map]
    {
      for (int j = 0; j < MaxIterations; ++j)
      {
        std::string k = std::to_string(i);
        typename Reclaimer::region_guard critical_region{};
        EXPECT_EQ(map.end(), map.find(k));
        EXPECT_TRUE(map.emplace(k, i));
        auto it = map.find(k);
        EXPECT_NE(map.end(), it);
        EXPECT_EQ(k, it->first);
        EXPECT_EQ(i, it->second);
        it.reset();
        EXPECT_TRUE(map.erase(k));
        EXPECT_FALSE(map.contains(k));
        auto result = map.get_or_emplace_lazy(k, [i](){ return i; });
        EXPECT_TRUE(result.second);
        it = map.erase(std::move(result.first));
        it.reset();
        EXPECT_FALSE(map.contains(k));

        for (auto& v : map)
          ;
      }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}

TYPED_TEST(HarrisMichaelHashMap, parallel_usage_with_same_values)
{
  using Reclaimer = TypeParam;

  using hash_map = xenium::harris_michael_hash_map<std::string, int,
    xenium::policy::reclaimer<Reclaimer>,
    xenium::policy::buckets<10>>;
  hash_map map;

  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i)
  {
    threads.push_back(std::thread([&map]
    {
      for (int j = 0; j < MaxIterations / 10; ++j)
        for (int i = 0; i < 10; ++i)
        {
          std::string k = std::to_string(i);
          typename Reclaimer::region_guard critical_region{};
          map.contains(k);
          map.emplace(k, i);
          auto it = map.find(k);
          it.reset();
          map.erase(k);
          auto result = map.get_or_emplace(k, i);
          if (result.second) {
            it = map.erase(std::move(result.first));
            it.reset();
          }
          result.first.reset();

          for (auto& v : map)
            ;
        }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}

}