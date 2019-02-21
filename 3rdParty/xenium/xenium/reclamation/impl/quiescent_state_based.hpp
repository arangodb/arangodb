//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef QUIESCENT_STATE_BASED_IMPL
#error "This is an impl file and must not be included directly!"
#endif

#include <xenium/reclamation/detail/orphan.hpp>
#include <xenium/detail/port.hpp>

#include <algorithm>

namespace xenium { namespace reclamation {

  struct quiescent_state_based::thread_control_block :
    detail::thread_block_list<thread_control_block>::entry
  {
    std::atomic<unsigned> local_epoch;
  };

  struct quiescent_state_based::thread_data
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

      global_thread_block_list.release_entry(control_block);
    }

    void enter_region()
    {
      ensure_has_control_block();
      ++region_entries;
    }

    void leave_region()
    {
      if (--region_entries == 0)
        quiescent_state();
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
        auto epoch = global_epoch.load(std::memory_order_relaxed);
        do {
          control_block->local_epoch.store(epoch, std::memory_order_relaxed);

          // (1) - this acq_rel-CAS synchronizes-with the acquire-load (2)
          //       and the acq_rel-CAS (5)
        } while (!global_epoch.compare_exchange_weak(epoch, epoch,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed));
      }
    }

    void quiescent_state()
    {
      // (2) - this acquire-load synchronizes-with the acq_rel-CAS (1, 5)
      auto epoch = global_epoch.load(std::memory_order_acquire);

      if (control_block->local_epoch.load(std::memory_order_relaxed) == epoch)
      {
        const auto new_epoch = (epoch + 1) % number_epochs;
        if (!try_update_epoch(epoch, new_epoch))
          return;

        epoch = new_epoch;
      }

      // we either just updated the global_epoch or we are observing a new epoch from some other thread
      // either way - we can reclaim all the objects from the old 'incarnation' of this epoch

      // (3) - this release-store synchronizes-with the acquire-fence (4)
      control_block->local_epoch.store(epoch, std::memory_order_release);
      detail::delete_objects(retire_lists[epoch]);
    }

    void add_retired_node(detail::deletable_object* p, size_t epoch)
    {
      assert(epoch < number_epochs);
      p->next = retire_lists[epoch];
      retire_lists[epoch] = p;
    }

    bool try_update_epoch(unsigned curr_epoch, unsigned new_epoch)
    {
      const auto old_epoch = (curr_epoch + number_epochs - 1) % number_epochs;
      auto prevents_update = [old_epoch](const thread_control_block& data)
      {
        // TSan does not support explicit fences, so we cannot rely on the acquire-fence (6)
        // but have to perform an acquire-load here to avoid false positives.
        constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
        return data.local_epoch.load(memory_order) == old_epoch &&
               data.is_active(memory_order);
      };

      // If any thread hasn't advanced to the current epoch, abort the attempt.
      bool cannot_update = std::any_of(global_thread_block_list.begin(), global_thread_block_list.end(),
                                       prevents_update);
      if (cannot_update)
        return false;

      if (global_epoch.load(std::memory_order_relaxed) == curr_epoch)
      {
        // (4) - this acquire-fence synchronizes-with the release-store (3)
        std::atomic_thread_fence(std::memory_order_acquire);

        // (5) - this acq_rel-CAS synchronizes-with the acquire-load (2)
        //       and the acq_rel-CAS (1)
        bool success = global_epoch.compare_exchange_strong(curr_epoch, new_epoch,
                                                            std::memory_order_acq_rel,
                                                            std::memory_order_relaxed);
        if (success)
          adopt_orphans();
      }

      // return true regardless of whether the CAS operation was successful or not
      // it's not import that THIS thread updated the epoch, but it got updated in any case
      return true;
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

    unsigned region_entries = 0;
    thread_control_block* control_block = nullptr;
    std::array<detail::deletable_object*, number_epochs> retire_lists = {};

    friend class quiescent_state_based;
    ALLOCATION_COUNTER(quiescent_state_based);
  };

  inline quiescent_state_based::region_guard::region_guard() noexcept
  {
      local_thread_data().enter_region();
  }

  inline quiescent_state_based::region_guard::~region_guard() noexcept
  {
      local_thread_data().leave_region();
  }

  template <class T, class MarkedPtr>
  quiescent_state_based::guard_ptr<T, MarkedPtr>::guard_ptr(const MarkedPtr& p) noexcept :
    base(p)
  {
    if (this->ptr)
      local_thread_data().enter_region();
  }

  template <class T, class MarkedPtr>
  quiescent_state_based::guard_ptr<T, MarkedPtr>::guard_ptr(const guard_ptr& p) noexcept :
    guard_ptr(MarkedPtr(p))
  {}

  template <class T, class MarkedPtr>
  quiescent_state_based::guard_ptr<T, MarkedPtr>::guard_ptr(guard_ptr&& p) noexcept :
    base(p.ptr)
  {
    p.ptr.reset();
  }

  template <class T, class MarkedPtr>
  typename quiescent_state_based::template guard_ptr<T, MarkedPtr>&
    quiescent_state_based::guard_ptr<T, MarkedPtr>::operator=(const guard_ptr& p) noexcept
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = p.ptr;
    if (this->ptr)
      local_thread_data().enter_region();

    return *this;
  }

  template <class T, class MarkedPtr>
  typename quiescent_state_based::template guard_ptr<T, MarkedPtr>&
    quiescent_state_based::guard_ptr<T, MarkedPtr>::operator=(guard_ptr&& p) noexcept
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = std::move(p.ptr);
    p.ptr.reset();

    return *this;
  }

  template <class T, class MarkedPtr>
  void quiescent_state_based::guard_ptr<T, MarkedPtr>::acquire(const concurrent_ptr<T>& p,
    std::memory_order order) noexcept
  {
    if (p.load(std::memory_order_relaxed) == nullptr)
    {
      reset();
      return;
    }

    if (!this->ptr)
      local_thread_data().enter_region();
    // (6) - this load operation potentially synchronizes-with any release operation on p.
    this->ptr = p.load(order);
    if (!this->ptr)
      local_thread_data().leave_region();
  }

  template <class T, class MarkedPtr>
  bool quiescent_state_based::guard_ptr<T, MarkedPtr>::acquire_if_equal(
    const concurrent_ptr<T>& p, const MarkedPtr& expected, std::memory_order order) noexcept
  {
    auto actual = p.load(std::memory_order_relaxed);
    if (actual == nullptr || actual != expected)
    {
      reset();
      return actual == expected;
    }

    if (!this->ptr)
      local_thread_data().enter_region();
    // (7) - this load operation potentially synchronizes-with any release operation on p.
    this->ptr = p.load(order);
    if (!this->ptr || this->ptr != expected)
    {
      local_thread_data().leave_region();
      this->ptr.reset();
    }

    return this->ptr == expected;
  }

  template <class T, class MarkedPtr>
  void quiescent_state_based::guard_ptr<T, MarkedPtr>::reset() noexcept
  {
    if (this->ptr)
      local_thread_data().leave_region();
    this->ptr.reset();
  }

  template <class T, class MarkedPtr>
  void quiescent_state_based::guard_ptr<T, MarkedPtr>::reclaim(Deleter d) noexcept
  {
    this->ptr->set_deleter(std::move(d));
    local_thread_data().add_retired_node(this->ptr.get());
    reset();
  }

  inline quiescent_state_based::thread_data& quiescent_state_based::local_thread_data()
  {
    // workaround for a GCC issue with weak references for static thread_local variables
    static thread_local thread_data local_thread_data;
    return local_thread_data;
  }

  SELECT_ANY std::atomic<unsigned> quiescent_state_based::global_epoch;
  SELECT_ANY detail::thread_block_list<quiescent_state_based::thread_control_block>
    quiescent_state_based::global_thread_block_list;

#ifdef TRACK_ALLOCATIONS
  SELECT_ANY detail::allocation_tracker quiescent_state_based::allocation_tracker;

  inline void quiescent_state_based::count_allocation()
  { local_thread_data().allocation_counter.count_allocation(); }

  inline void quiescent_state_based::count_reclamation()
  { local_thread_data().allocation_counter.count_reclamation(); }
#endif
}}
