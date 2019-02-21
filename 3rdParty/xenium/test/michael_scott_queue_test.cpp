#include <xenium/reclamation/lock_free_ref_count.hpp>
#include <xenium/reclamation/hazard_pointer.hpp>
#include <xenium/reclamation/hazard_eras.hpp>
#include <xenium/reclamation/epoch_based.hpp>
#include <xenium/reclamation/new_epoch_based.hpp>
#include <xenium/reclamation/quiescent_state_based.hpp>
#include <xenium/reclamation/debra.hpp>
#include <xenium/reclamation/generic_epoch_based.hpp>
#include <xenium/reclamation/stamp_it.hpp>
#include <xenium/michael_scott_queue.hpp>

#include <gtest/gtest.h>

#include <vector>
#include <thread>

namespace {

template <typename Reclaimer>
struct MichaelScottQueue : testing::Test {};

using Reclaimers = ::testing::Types<
    xenium::reclamation::lock_free_ref_count<>,
    xenium::reclamation::hazard_pointer<xenium::reclamation::static_hazard_pointer_policy<2>>,
    xenium::reclamation::hazard_eras<xenium::reclamation::static_hazard_eras_policy<2>>,
    xenium::reclamation::epoch_based<10>,
    xenium::reclamation::new_epoch_based<10>,
    xenium::reclamation::quiescent_state_based,
    xenium::reclamation::debra<20>,
    xenium::reclamation::stamp_it,
    xenium::reclamation::epoch_based2<>,
    xenium::reclamation::new_epoch_based2<>,
    xenium::reclamation::debra2<>
  >;
TYPED_TEST_CASE(MichaelScottQueue, Reclaimers);

TYPED_TEST(MichaelScottQueue, push_try_pop_returns_pushed_element)
{
  xenium::michael_scott_queue<int, xenium::policy::reclaimer<TypeParam>> queue;
  queue.push(42);
  int elem;
  EXPECT_TRUE(queue.try_pop(elem));
  EXPECT_EQ(42, elem);
}

TYPED_TEST(MichaelScottQueue, push_two_items_pop_them_in_FIFO_order)
{
  xenium::michael_scott_queue<int, xenium::policy::reclaimer<TypeParam>> queue;
  queue.push(42);
  queue.push(43);
  int elem1, elem2;
  EXPECT_TRUE(queue.try_pop(elem1));
  EXPECT_TRUE(queue.try_pop(elem2));
  EXPECT_EQ(42, elem1);
  EXPECT_EQ(43, elem2);
}

TYPED_TEST(MichaelScottQueue, supports_move_only_types)
{
  xenium::michael_scott_queue<std::unique_ptr<int>, xenium::policy::reclaimer<TypeParam>> queue;
  queue.push(std::unique_ptr<int>(new int(42)));

  std::unique_ptr<int> elem;
  ASSERT_TRUE(queue.try_pop(elem));
  ASSERT_NE(nullptr, elem);
  EXPECT_EQ(42, *elem);
}

TYPED_TEST(MichaelScottQueue, parallel_usage)
{
  using Reclaimer = TypeParam;
  xenium::michael_scott_queue<int, xenium::policy::reclaimer<Reclaimer>> queue;

  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i)
  {
    threads.push_back(std::thread([i, &queue]
    {
    #ifdef DEBUG
      const int MaxIterations = 1000;
    #else
      const int MaxIterations = 10000;
    #endif
      for (int j = 0; j < MaxIterations; ++j)
      {
        typename Reclaimer::region_guard critical_region{};
        queue.push(i);
        int v;
        EXPECT_TRUE(queue.try_pop(v));
      }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}
}