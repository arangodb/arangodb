//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_DETAIL_ALLOCATION_TRACKER_HPP
#define XENIUM_DETAIL_ALLOCATION_TRACKER_HPP

#ifndef TRACK_ALLOCATIONS
namespace xenium { namespace reclamation { namespace detail {
  template <typename>
  struct tracked_object {};
}}}

#define ALLOCATION_COUNTER(tracker)
#define ALLOCATION_TRACKER
#define ALLOCATION_TRACKING_FUNCTIONS

#else

#include <atomic>
#include <cassert>
#include <cstdint>
#include <utility>

namespace xenium { namespace reclamation { namespace detail {
  struct allocation_tracker;

  template <typename Tracker>
  struct tracked_object {
    tracked_object() noexcept { Tracker::count_allocation(); }
    tracked_object(const tracked_object&) noexcept  { Tracker::count_allocation(); }
    tracked_object(tracked_object&&) noexcept  { Tracker::count_allocation(); }
    virtual ~tracked_object() noexcept { Tracker::count_reclamation(); }
  };

  struct allocation_counter
  {
    ~allocation_counter() { vals->dead = true; }

    struct values
    {
      values() :
        allocated_instances(),
        reclaimed_instances(),
        dead(false)
      {}
      std::atomic<std::size_t> allocated_instances;
      std::atomic<std::size_t> reclaimed_instances;
      std::atomic<bool> dead;
      values* next;
    };
    void count_allocation()
    {
      assert(vals->dead == false);
      auto v = vals->allocated_instances.load(std::memory_order_relaxed);
      vals->allocated_instances.store(v + 1, std::memory_order_relaxed);
    }
    void count_reclamation()
    {
      assert(vals->dead == false);
      auto v = vals->reclaimed_instances.load(std::memory_order_relaxed);
      vals->reclaimed_instances.store(v + 1, std::memory_order_relaxed);
    }
  protected:
    values* vals = new values();;
  };

  template <typename Tracker>
  struct registered_allocation_counter : allocation_counter
  {
    registered_allocation_counter()
    {
      auto h = Tracker::allocation_tracker.head.load(std::memory_order_relaxed);
      do
      {
        vals->next = h;
      } while (!Tracker::allocation_tracker.head.compare_exchange_weak(h, vals, std::memory_order_release));
    }
  };
  struct allocation_tracker
  {
    std::pair<std::size_t, std::size_t> get_counters() const
    {
      std::size_t allocated_instances = collapsed_allocated_instances;
      std::size_t reclaimed_instances = collapsed_reclaimed_instances;
      auto p = head.load(std::memory_order_acquire);
      while (p)
      {
        allocated_instances += p->allocated_instances.load(std::memory_order_relaxed);
        reclaimed_instances += p->reclaimed_instances.load(std::memory_order_relaxed);
        p = p->next;
      }
      return std::make_pair(allocated_instances, reclaimed_instances);
    }

    void collapse_counters()
    {
      auto p = head.load(std::memory_order_acquire);
      allocation_counter::values* remaining = nullptr;
      while (p)
      {
        auto next = p->next;
        if (p->dead.load(std::memory_order_relaxed))
        {
          collapsed_allocated_instances += p->allocated_instances.load(std::memory_order_relaxed);
          collapsed_reclaimed_instances += p->reclaimed_instances.load(std::memory_order_relaxed);
          delete p;
        }
        else
        {
          p->next = remaining;
          remaining = p;
        }
        p = next;
      }
      head.store(remaining, std::memory_order_relaxed);
    }
  private:
    template <typename>
    friend struct registered_allocation_counter;
    std::atomic<allocation_counter::values*> head;
    std::size_t collapsed_allocated_instances = 0;
    std::size_t collapsed_reclaimed_instances = 0;
  };
}}}

#define ALLOCATION_COUNTER(tracker) \
  detail::registered_allocation_counter<tracker> allocation_counter;

#define ALLOCATION_TRACKER \
  static detail::allocation_tracker allocation_tracker;

#define ALLOCATION_TRACKING_FUNCTIONS \
  template <typename> friend struct detail::tracked_object; \
  static void count_allocation(); \
  static void count_reclamation();

#endif

#endif
