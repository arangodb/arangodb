#include <xenium/reclamation/hazard_pointer.hpp>
#include <xenium/reclamation/hazard_eras.hpp>
#include <xenium/reclamation/epoch_based.hpp>
#include <xenium/reclamation/new_epoch_based.hpp>
#include <xenium/reclamation/quiescent_state_based.hpp>
#include <xenium/reclamation/debra.hpp>
#include <xenium/reclamation/generic_epoch_based.hpp>
#include <xenium/reclamation/stamp_it.hpp>
#include <xenium/vyukov_hash_map.hpp>

#include <gtest/gtest.h>

#include <vector>
#include <thread>

// Simple solution to simulate exception beeing thrown in "compare_key". Such
// exceptions can be caused by the construction of guard_ptr instances (e.g.,
// when using hazard_pointer reclaimer).
struct throwing_key {
  throwing_key() noexcept {}
  throwing_key(int v) noexcept : v(v) {}
  int v;
  bool operator==(const throwing_key& r) const {
    throw std::runtime_error("test exception");
  }
};

template <>
struct xenium::hash<throwing_key> {
  hash_t operator()(const throwing_key& v) const { return v.v; }
};

namespace {

template <typename Reclaimer>
struct VyukovHashMap : ::testing::Test
{
  using hash_map = xenium::vyukov_hash_map<int, int,
    xenium::policy::reclaimer<Reclaimer>>;
  hash_map map{8};
};

using Reclaimers = ::testing::Types<
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
TYPED_TEST_CASE(VyukovHashMap, Reclaimers);

TYPED_TEST(VyukovHashMap, emplace_returns_true_for_successful_insert)
{
  EXPECT_TRUE(this->map.emplace(42, 42));
}

TYPED_TEST(VyukovHashMap, emplace_returns_false_for_failed_insert)
{
  this->map.emplace(42, 42);
  EXPECT_FALSE(this->map.emplace(42, 43));
}

TYPED_TEST(VyukovHashMap, get_or_emplace_returns_accessor_to_newly_inserted_element)
{
  auto result = this->map.get_or_emplace(42, 43);
  EXPECT_TRUE(result.second);
  EXPECT_EQ(43, *result.first);
}

TYPED_TEST(VyukovHashMap, get_or_emplace_returns_accessor_to_existing_element)
{
  this->map.emplace(42, 41);
  auto result = this->map.get_or_emplace(42, 43);
  EXPECT_FALSE(result.second);
  EXPECT_EQ(41, *result.first);
}

TYPED_TEST(VyukovHashMap, get_or_emplace_lazy_calls_factory_and_returns_accessor_to_newly_inserted_element)
{
  bool called_factory = false;
  auto result = this->map.get_or_emplace_lazy(42,
    [&](){
      called_factory = true;
      return 43;
    });
  EXPECT_TRUE(result.second);
  EXPECT_EQ(43, *result.first);
}

TYPED_TEST(VyukovHashMap, get_or_emplace_lazy_does_not_call_factory_and_returns_accessor_to_existing_element)
{
  bool called_factory = false;
  this->map.emplace(42, 41);
  auto result = this->map.get_or_emplace_lazy(42,
                                              [&](){
                                                called_factory = true;
                                                return 43;
                                              });
  EXPECT_FALSE(result.second);
  EXPECT_EQ(41, *result.first);
}

TYPED_TEST(VyukovHashMap, try_get_value_returns_false_key_is_not_found)
{
  typename VyukovHashMap<TypeParam>::hash_map::accessor acc;
  EXPECT_FALSE(this->map.try_get_value(42, acc));
}

TYPED_TEST(VyukovHashMap, try_get_value_returns_true_and_sets_result_if_matching_entry_exists)
{
  this->map.emplace(42, 43);
  typename VyukovHashMap<TypeParam>::hash_map::accessor acc;
  EXPECT_TRUE(this->map.try_get_value(42, acc));
  EXPECT_EQ(43, *acc);
}

TYPED_TEST(VyukovHashMap, find_returns_iterator_to_existing_element)
{
  // We use a for loop to ensure that we cover cases where entries are
  // stored in normal buckets as well as extension buckets.
  for (int i = 0; i < 200; ++i) {
    this->map.emplace(i, i);
    auto it = this->map.find(i);
    ASSERT_NE(this->map.end(), it);
    EXPECT_EQ(i, (*it).first);
    EXPECT_EQ(i, (*it).second);
  }
}

TYPED_TEST(VyukovHashMap, find_returns_end_iterator_for_non_existing_element)
{
  for (int i = 0; i < 200; ++i) {
    if (i != 42)
      this->map.emplace(i, i);
  }
  EXPECT_EQ(this->map.end(), this->map.find(42));
}


TYPED_TEST(VyukovHashMap, erase_nonexisting_element_returns_false)
{
  EXPECT_FALSE(this->map.erase(42));
}

TYPED_TEST(VyukovHashMap, erase_existing_element_returns_true_and_removes_element)
{
  this->map.emplace(42, 43);
  EXPECT_TRUE(this->map.erase(42));
  EXPECT_FALSE(this->map.erase(42));
}

TYPED_TEST(VyukovHashMap, extract_existing_element_returns_true_and_removes_element_and_returns_old_value)
{
  this->map.emplace(42, 43);
  typename VyukovHashMap<TypeParam>::hash_map::accessor acc;
  EXPECT_TRUE(this->map.extract(42, acc));
  EXPECT_EQ(43, *acc);
  EXPECT_FALSE(this->map.erase(42));
}

TYPED_TEST(VyukovHashMap, map_grows_if_needed)
{
  for (int i = 0; i < 10000; ++i)
    EXPECT_TRUE(this->map.emplace(i, i));
}

TYPED_TEST(VyukovHashMap, with_managed_pointer_value)
{
  struct node : TypeParam::template enable_concurrent_ptr<node> {
    node(int v): v(v) {}
    int v;
  };

  using hash_map = xenium::vyukov_hash_map<int, xenium::managed_ptr<node, TypeParam>,
    xenium::policy::reclaimer<TypeParam>>;
  hash_map map;

  EXPECT_TRUE(map.emplace(42, new node(43)));
  typename hash_map::accessor acc;
  EXPECT_TRUE(map.try_get_value(42, acc));
  EXPECT_EQ(43, acc->v);
  EXPECT_TRUE(map.erase(42));

  auto n = new node(44);
  EXPECT_TRUE(map.emplace(42, n));

  auto it = map.begin();
  EXPECT_EQ(42, (*it).first);
  EXPECT_EQ(n, (*it).second);
  it.reset();
  
  for (auto v : map) {
    EXPECT_EQ(42, v.first);
    EXPECT_EQ(n, v.second);
  }

  acc.reset();
  bool inserted;
  std::tie(acc, inserted) = map.get_or_emplace(42, nullptr);
  EXPECT_FALSE(inserted);
  EXPECT_EQ(44, acc->v);

  acc.reset();
  EXPECT_TRUE(map.extract(42, acc));
  EXPECT_EQ(44, acc->v);
  acc.reclaim();
}

TYPED_TEST(VyukovHashMap, with_string_value)
{
  using hash_map = xenium::vyukov_hash_map<int, std::string, xenium::policy::reclaimer<TypeParam>>;
  hash_map map;

  EXPECT_TRUE(map.emplace(42, "foo"));
  typename hash_map::accessor acc;
  EXPECT_TRUE(map.try_get_value(42, acc));
  EXPECT_EQ("foo", *acc);
  EXPECT_TRUE(map.erase(42));

  EXPECT_TRUE(map.emplace(42, "bar"));
  auto it = map.begin();
  EXPECT_EQ(42, (*it).first);
  EXPECT_EQ("bar", (*it).second);

  it.reset();

  for (auto v : map) {
    EXPECT_EQ(42, v.first);
    EXPECT_EQ("bar", v.second);
  }
  
  acc.reset();
  bool inserted;
  std::tie(acc, inserted) = map.get_or_emplace(42, "xyz");
  EXPECT_FALSE(inserted);
  EXPECT_EQ("bar", *acc);

  EXPECT_TRUE(map.extract(42, acc));
  EXPECT_EQ("bar", *acc);
}

TYPED_TEST(VyukovHashMap, with_string_key)
{
  using hash_map = xenium::vyukov_hash_map<std::string, int, xenium::policy::reclaimer<TypeParam>>;
  hash_map map;

  EXPECT_TRUE(map.emplace("foo", 42));
  typename hash_map::accessor acc;
  EXPECT_TRUE(map.try_get_value("foo", acc));
  EXPECT_EQ(42, *acc);
  EXPECT_TRUE(map.erase("foo"));

  EXPECT_TRUE(map.emplace("foo", 43));
  auto it = map.begin();
  EXPECT_EQ("foo", (*it).first);
  EXPECT_EQ(43, (*it).second);
  EXPECT_EQ("foo", it->first);
  EXPECT_EQ(43, it->second);
  it.reset();

  for (auto& v : map) {
    EXPECT_EQ("foo", v.first);
    EXPECT_EQ(43, v.second);
  }

  acc.reset();
  bool inserted;
  std::tie(acc, inserted) = map.get_or_emplace("foo", 42);
  EXPECT_FALSE(inserted);
  EXPECT_EQ(43, *acc);

  EXPECT_TRUE(map.extract("foo", acc));
  EXPECT_EQ(43, *acc);
}

TYPED_TEST(VyukovHashMap, with_string_key_and_managed_ptr_value)
{
  struct node : TypeParam::template enable_concurrent_ptr<node> {
    node(int v): v(v) {}
    int v;
  };

  using hash_map = xenium::vyukov_hash_map<std::string, xenium::managed_ptr<node, TypeParam>,
    xenium::policy::reclaimer<TypeParam>>;

  hash_map map;

  EXPECT_TRUE(map.emplace("foo", new node(42)));
  typename hash_map::accessor acc;
  EXPECT_TRUE(map.try_get_value("foo", acc));
  EXPECT_EQ(42, acc->v);
  acc.reset();
  EXPECT_TRUE(map.erase("foo"));

  EXPECT_TRUE(map.emplace("foo", new node(43)));
  auto it = map.begin();
  EXPECT_EQ("foo", (*it).first);
  EXPECT_EQ(43, (*it).second->v);
  it.reset();

  for (auto v : map) {
    EXPECT_EQ("foo", v.first);
    EXPECT_EQ(43, v.second->v);
  }

  bool inserted;
  std::tie(acc, inserted) = map.get_or_emplace("foo", nullptr);
  EXPECT_FALSE(inserted);
  EXPECT_EQ(43, acc->v);
  acc.reset();

  EXPECT_TRUE(map.extract("foo", acc));
  EXPECT_EQ(43, acc->v);
  auto n = &*acc;
  acc.reset();

  std::tie(acc, inserted) = map.get_or_emplace("foo", n);
  EXPECT_TRUE(inserted);
  EXPECT_EQ(43, acc->v);
}

TYPED_TEST(VyukovHashMap, emplace_unlocks_bucket_in_case_of_exception)
{
  this->map.emplace(42, 42);
  EXPECT_THROW(
    this->map.get_or_emplace_lazy(43, []() -> int { throw std::runtime_error("test exception"); }),
    std::runtime_error
  );
  typename VyukovHashMap<TypeParam>::hash_map::accessor acc;
  EXPECT_TRUE(this->map.erase(42));
}

TYPED_TEST(VyukovHashMap, erase_unlocks_bucket_in_case_of_exception)
{
  using hash_map = xenium::vyukov_hash_map<throwing_key, int, xenium::policy::reclaimer<TypeParam>>;
  hash_map map;
  
  map.emplace(throwing_key{42}, 42);
  EXPECT_THROW(map.erase(throwing_key{42}), std::runtime_error);
  auto it = map.begin();
  EXPECT_EQ(42, (*it).first.v);
}

TYPED_TEST(VyukovHashMap, correctly_handles_hash_collisions_of_nontrivial_keys)
{
  struct dummy_hash {
    dummy_hash() = default;
    std::size_t operator()(const std::string&) { return 1; }
  };
  using hash_map = xenium::vyukov_hash_map<std::string, int,
    xenium::policy::reclaimer<TypeParam>, xenium::policy::hash<dummy_hash>>;
  hash_map map;

  EXPECT_TRUE(map.emplace("foo", 42));
  EXPECT_TRUE(map.emplace("bar", 43));
  typename hash_map::accessor acc;
  EXPECT_TRUE(map.try_get_value("foo", acc));
  EXPECT_EQ(42, *acc);
  EXPECT_TRUE(map.try_get_value("bar", acc));
  EXPECT_EQ(43, *acc);
  
  EXPECT_TRUE(map.extract("foo", acc));
  EXPECT_EQ(42, *acc);
}

TYPED_TEST(VyukovHashMap, begin_returns_end_iterator_for_empty_map)
{
  auto it = this->map.begin();
  ASSERT_EQ(this->map.end(), it);
}

TYPED_TEST(VyukovHashMap, begin_returns_iterator_to_first_entry)
{
  this->map.emplace(42, 43);
  auto it = this->map.begin();
  ASSERT_NE(this->map.end(), it);
  EXPECT_EQ(42, (*it).first);
  EXPECT_EQ(43, (*it).second);
  ++it;
  ASSERT_EQ(this->map.end(), it);
}

TYPED_TEST(VyukovHashMap, drain_densely_populated_map_using_erase)
{
  for (int i = 0; i < 200; ++i)
    this->map.emplace(i, i);

  auto it = this->map.begin();
  while (it != this->map.end())
    this->map.erase(it);

  EXPECT_EQ(this->map.end(), this->map.begin());
}

TYPED_TEST(VyukovHashMap, drain_sparsely_populated_map_using_erase)
{
  for (int i = 0; i < 4; ++i)
    this->map.emplace(i * 7, i);

  auto it = this->map.begin();
  while (it != this->map.end())
    this->map.erase(it);

  EXPECT_EQ(this->map.end(), this->map.begin());
}

TYPED_TEST(VyukovHashMap, iterator_covers_all_entries_in_densely_populated_map)
{
  std::map<int, bool> values;
  for (int i = 0; i < 200; ++i) {
    values[i] = false;
    this->map.emplace(i, i);
  }
  for (auto v : this->map)
    values[v.first] = true;
  for (auto& v : values)
    EXPECT_TRUE(v.second) << v.first << " was not visited";
}

TYPED_TEST(VyukovHashMap, iterator_covers_all_entries_in_sparsely_populated_map)
{
  std::map<int, bool> values;
  for (int i = 0; i < 4; ++i) {
    values[i * 7] = false;
    this->map.emplace(i * 7, i);
  }
  for (auto v : this->map)
    values[v.first] = true;

  for (auto& v : values)
    EXPECT_TRUE(v.second) << v.first << " was not visited";
}

#ifdef DEBUG
  const int MaxIterations = 2000;
#else
  const int MaxIterations = 40000;
#endif

TYPED_TEST(VyukovHashMap, parallel_usage)
{
  using Reclaimer = TypeParam;

  using hash_map = xenium::vyukov_hash_map<int, int,
    xenium::policy::reclaimer<Reclaimer>>;
  hash_map map(8);

  static constexpr int keys_per_thread = 8;

  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i)
  {
    threads.push_back(std::thread([i, &map]
    {
      for (int k = i * keys_per_thread; k < (i + 1) * keys_per_thread; ++k) {
        for (int j = 0; j < MaxIterations / keys_per_thread; ++j) {
          typename Reclaimer::region_guard critical_region{};
          EXPECT_TRUE(map.emplace(k, k));
          for (int x = 0; x < 10; ++x) {
            typename hash_map::accessor acc;
            EXPECT_TRUE(map.try_get_value(k, acc));
            EXPECT_EQ(k, *acc);
          }
          if ((j + i) % 8 == 0) {
            for (auto v : map)
              EXPECT_EQ(v.first, v.second);
          }
          EXPECT_TRUE(map.erase(k));
        }
      }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}

TYPED_TEST(VyukovHashMap, parallel_usage_with_nontrivial_types)
{
  using Reclaimer = TypeParam;

  using hash_map = xenium::vyukov_hash_map<std::string, std::string,
    xenium::policy::reclaimer<Reclaimer>>;
  hash_map map(8);

  static constexpr int keys_per_thread = 8;

  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i)
  {
    threads.push_back(std::thread([i, &map]
    {
      for (int k = i * keys_per_thread; k < (i + 1) * keys_per_thread; ++k) {
        for (int j = 0; j < (MaxIterations / keys_per_thread) / 2; ++j) {
          std::string v = std::to_string(k);
          typename Reclaimer::region_guard critical_region{};
          EXPECT_TRUE(map.emplace(v, v));
          for (int x = 0; x < 10; ++x) {
            typename hash_map::accessor acc;
            EXPECT_TRUE(map.try_get_value(v, acc));
            EXPECT_EQ(v, *acc);
          }
          if ((j + i) % 8 == 0) {
            for (auto& v : map)
              EXPECT_EQ(v.first, v.second);
          }
          EXPECT_TRUE(map.erase(v));
        }
      }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}

TYPED_TEST(VyukovHashMap, parallel_usage_with_same_values)
{
  using Reclaimer = TypeParam;

  using hash_map = xenium::vyukov_hash_map<int, int,
    xenium::policy::reclaimer<Reclaimer>>;
  hash_map map(8);

  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i)
  {
    threads.push_back(std::thread([&map]
    {
      for (int j = 0; j < MaxIterations / 10; ++j)
        for (int i = 0; i < 10; ++i)
        {
          int k = i;
          typename Reclaimer::region_guard critical_region{};
          map.emplace(k, i);
          typename hash_map::accessor acc;
          if (map.try_get_value(k, acc))
            EXPECT_EQ(k, *acc);

          if (j % 4 == 0) {
            for (auto v : map)
              ;
          }
          map.erase(k);
        }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}

}