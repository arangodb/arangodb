#include <xenium/reclamation/lock_free_ref_count.hpp>
#include <xenium/reclamation/hazard_pointer.hpp>
#include <xenium/reclamation/hazard_eras.hpp>
#include <xenium/reclamation/epoch_based.hpp>
#include <xenium/reclamation/new_epoch_based.hpp>
#include <xenium/reclamation/quiescent_state_based.hpp>
#include <xenium/reclamation/debra.hpp>
#include <xenium/reclamation/stamp_it.hpp>
#include <xenium/ramalhete_queue.hpp>

#include <gtest/gtest.h>

#include <vector>
#include <thread>

namespace {

template <typename Reclaimer>
struct RamalheteQueue : testing::Test {};

int* v1 = new int(42);
int* v2 = new int(43);

using Reclaimers = ::testing::Types<
    xenium::reclamation::lock_free_ref_count<>,
    xenium::reclamation::hazard_pointer<xenium::reclamation::static_hazard_pointer_policy<2>>,
    xenium::reclamation::hazard_eras<xenium::reclamation::static_hazard_eras_policy<2>>,
    xenium::reclamation::epoch_based<10>,
    xenium::reclamation::new_epoch_based<10>,
    xenium::reclamation::quiescent_state_based,
    xenium::reclamation::debra<20>,
    xenium::reclamation::stamp_it
  >;
TYPED_TEST_CASE(RamalheteQueue, Reclaimers);

TYPED_TEST(RamalheteQueue, push_try_pop_returns_pushed_element)
{
  xenium::ramalhete_queue<int*, xenium::policy::reclaimer<TypeParam>> queue;
  queue.push(v1);
  int* elem;
  EXPECT_TRUE(queue.try_pop(elem));
  EXPECT_EQ(v1, elem);
}

TYPED_TEST(RamalheteQueue, push_two_items_pop_them_in_FIFO_order)
{
  xenium::ramalhete_queue<int*, xenium::policy::reclaimer<TypeParam>> queue;
  queue.push(v1);
  queue.push(v2);
  int* elem1;
  int* elem2;
  EXPECT_TRUE(queue.try_pop(elem1));
  EXPECT_TRUE(queue.try_pop(elem2));
  EXPECT_EQ(v1, elem1);
  EXPECT_EQ(v2, elem2);
}

TYPED_TEST(RamalheteQueue, parallel_usage)
{
  using Reclaimer = TypeParam;
  xenium::ramalhete_queue<int*, xenium::policy::reclaimer<TypeParam>> queue;

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
        queue.push(new int(i));
        int* elem;
        EXPECT_TRUE(queue.try_pop(elem));
        EXPECT_TRUE(*elem >= 0 && *elem <= 4);
        delete elem;
      }
    }));
  }

  for (auto& thread : threads)
    thread.join();
}
}