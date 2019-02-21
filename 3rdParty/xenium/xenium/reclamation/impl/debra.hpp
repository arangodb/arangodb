//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef DEBRA_IMPL
#error "This is an impl file and must not be included directly!"
#endif

#include <xenium/reclamation/detail/orphan.hpp>
#include <xenium/detail/port.hpp>

#include <algorithm>

namespace xenium { namespace reclamation {

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::guard_ptr(const MarkedPtr& p) noexcept :
    base(p)
  {
    if (this->ptr)
      local_thread_data().enter_critical();
  }

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::guard_ptr(const guard_ptr& p) noexcept :
    guard_ptr(MarkedPtr(p))
  {}

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::guard_ptr(guard_ptr&& p) noexcept :
    base(p.ptr)
  {
    p.ptr.reset();
  }

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  auto debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::operator=(const guard_ptr& p) noexcept
    -> guard_ptr&
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = p.ptr;
    if (this->ptr)
      local_thread_data().enter_critical();

    return *this;
  }

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  auto debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::operator=(guard_ptr&& p) noexcept
    -> guard_ptr&
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = std::move(p.ptr);
    p.ptr.reset();

    return *this;
  }

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  void debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::acquire(const concurrent_ptr<T>& p,
    std::memory_order order) noexcept
  {
    if (p.load(std::memory_order_relaxed) == nullptr)
    {
      reset();
      return;
    }

    if (!this->ptr)
      local_thread_data().enter_critical();
    // (1) - this load operation potentially synchronizes-with any release operation on p.
    this->ptr = p.load(order);
    if (!this->ptr)
      local_thread_data().leave_critical();
  }

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  bool debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::acquire_if_equal(
    const concurrent_ptr<T>& p,
    const MarkedPtr& expected,
    std::memory_order order) noexcept
  {
    auto actual = p.load(std::memory_order_relaxed);
    if (actual == nullptr || actual != expected)
    {
      reset();
      return actual == expected;
    }

    if (!this->ptr)
      local_thread_data().enter_critical();
    // (2) - this load operation potentially synchronizes-with any release operation on p.
    this->ptr = p.load(order);
    if (!this->ptr || this->ptr != expected)
    {
      local_thread_data().leave_critical();
      this->ptr.reset();
    }

    return this->ptr == expected;
  }

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  void debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::reset() noexcept
  {
    if (this->ptr)
      local_thread_data().leave_critical();
    this->ptr.reset();
  }

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  void debra<UpdateThreshold>::guard_ptr<T, MarkedPtr>::reclaim(Deleter d) noexcept
  {
    this->ptr->set_deleter(std::move(d));
    local_thread_data().add_retired_node(this->ptr.get());
    reset();
  }

  template <std::size_t UpdateThreshold>
  struct debra<UpdateThreshold>::thread_control_block :
    detail::thread_block_list<thread_control_block>::entry
  {
    thread_control_block() :
      is_in_critical_region(false),
      local_epoch(number_epochs)
    {}

    std::atomic<bool> is_in_critical_region;
    std::atomic<epoch_t> local_epoch;
  };

  template <std::size_t UpdateThreshold>
  struct debra<UpdateThreshold>::thread_data
  {
    ~thread_data()
    {
      if (control_block == nullptr)
        return; // no control_block -> nothing to do

      // we can avoid creating an orphan in case we have no retired nodes left.
      if (std::any_of(retire_lists.begin(), retire_lists.end(), [](auto p) { return p != nullptr; }))
      {
        // global_epoch - 1 (mod number_epochs) guarantees a full cycle, making sure no
        // other thread may still have a reference to an object in one of the retire lists.
        auto target_epoch = (global_epoch.load(std::memory_order_relaxed) + number_epochs - 1) % number_epochs;
        assert(target_epoch < number_epochs);
        global_thread_block_list.abandon_retired_nodes(new detail::orphan<number_epochs>(target_epoch, retire_lists));
      }

      assert(control_block->is_in_critical_region.load(std::memory_order_relaxed) == false);
      global_thread_block_list.release_entry(control_block);
    }

    void enter_critical()
    {
      if (++enter_count == 1)
        do_enter_critical();
    }

    void leave_critical()
    {
      assert(enter_count > 0);
      if (--enter_count == 0)
        do_leave_critical();
    }

    void add_retired_node(detail::deletable_object* p)
    {
      add_retired_node(p, control_block->local_epoch.load(std::memory_order_relaxed));
    }

  private:
    void ensure_has_control_block()
    {
      if (control_block == nullptr)
      {
        control_block = global_thread_block_list.acquire_entry();
        thread_iterator = global_thread_block_list.begin();
      }
    }

    void do_enter_critical()
    {
      ensure_has_control_block();

      control_block->is_in_critical_region.store(true, std::memory_order_relaxed);
      // (3) - this seq_cst-fence enforces a total order with itself
      std::atomic_thread_fence(std::memory_order_seq_cst);

      // (4) - this acquire-load synchronizes-with the release-CAS (7)
      auto epoch = global_epoch.load(std::memory_order_acquire);
      if (control_block->local_epoch.load(std::memory_order_relaxed) != epoch) // New epoch?
      {
        entries_since_update = 0;
        update_local_epoch(epoch);
      }
      else if (entries_since_update++ == UpdateThreshold)
      {
        entries_since_update = 0;

        // TSan does not support explicit fences, so we cannot rely on the acquire-fence (6)
        // but have to perform an acquire-load here to avoid false positives.
        constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
        if (!thread_iterator->is_in_critical_region.load(memory_order) ||
            thread_iterator->local_epoch.load(memory_order) == epoch)
        {
          if (++thread_iterator == global_thread_block_list.end())
          {
            epoch = update_global_epoch(epoch, epoch + 1);
            update_local_epoch(epoch);
          }
        }
      }
    }

    void update_local_epoch(epoch_t epoch)
    {
      // we either just updated the global_epoch or we are observing a new epoch from some other thread
      // either way - we can reclaim all the objects from the old 'incarnation' of this epoch
      auto idx = epoch % number_epochs;
      detail::delete_objects(retire_lists[idx]);

      control_block->local_epoch.store(epoch, std::memory_order_relaxed);
      thread_iterator = global_thread_block_list.begin();
    };

    void do_leave_critical()
    {
      // (5) - this release-store synchronizes-with the acquire-fence (6)
      control_block->is_in_critical_region.store(false, std::memory_order_release);
    }

    void add_retired_node(detail::deletable_object* p, size_t epoch)
    {
      auto idx = epoch % number_epochs;
      p->next = retire_lists[idx];
      retire_lists[idx] = p;
    }

    epoch_t update_global_epoch(epoch_t curr_epoch, epoch_t new_epoch)
    {
      // (6) - this acquire-fence synchronizes-with the release-store (5)
      std::atomic_thread_fence(std::memory_order_acquire);

      // (7) - this release-CAS synchronizes-with the acquire-load (4)
      bool success = global_epoch.compare_exchange_strong(curr_epoch, new_epoch,
                                                          std::memory_order_release,
                                                          std::memory_order_relaxed);
      if (success)
      {
        adopt_orphans();
        return new_epoch;
      }
      else
        return curr_epoch; // some other thread was faster -> return the reloaded value
    }

    void adopt_orphans()
    {
      auto cur = global_thread_block_list.adopt_abandoned_retired_nodes();
      for (detail::deletable_object* next = nullptr; cur != nullptr; cur = next)
      {
        next = cur->next;
        cur->next = nullptr;
        add_retired_node(cur, static_cast<detail::orphan<number_epochs>*>(cur)->target_epoch);
      }
    }

    unsigned enter_count = 0;
    unsigned entries_since_update = 0;
    typename detail::thread_block_list<thread_control_block>::iterator thread_iterator;
    thread_control_block* control_block = nullptr;
    std::array<detail::deletable_object*, number_epochs> retire_lists = {};

    friend class debra;
    ALLOCATION_COUNTER(debra);
  };

  template <std::size_t UpdateThreshold>
  std::atomic<typename debra<UpdateThreshold>::epoch_t> debra<UpdateThreshold>::global_epoch;

  template <std::size_t UpdateThreshold>
  detail::thread_block_list<typename debra<UpdateThreshold>::thread_control_block>
    debra<UpdateThreshold>::global_thread_block_list;

  template <std::size_t UpdateThreshold>
  inline typename debra<UpdateThreshold>::thread_data& debra<UpdateThreshold>::local_thread_data()
  {
    static thread_local thread_data local_thread_data;
    return local_thread_data;
  }

#ifdef TRACK_ALLOCATIONS
  template <std::size_t UpdateThreshold>
  detail::allocation_tracker debra<UpdateThreshold>::allocation_tracker;

  template <std::size_t UpdateThreshold>
  inline void debra<UpdateThreshold>::count_allocation()
  { local_thread_data().allocation_counter.count_allocation(); }

  template <std::size_t UpdateThreshold>
  inline void debra<UpdateThreshold>::count_reclamation()
  { local_thread_data().allocation_counter.count_reclamation(); }
#endif
}}
