//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef GENERIC_EPOCH_BASED_IMPL
#error "This is an impl file and must not be included directly!"
#endif

#include <boost/config.hpp>

namespace xenium { namespace reclamation {
  namespace scan {
    /**
     * @brief Scan all threads (default behaviour in EBR/NEBR).
     */
    struct all_threads {
      template<class Reclaimer>
      struct type {
        bool scan(typename Reclaimer::epoch_t epoch) {
          auto prevents_update = [epoch](const typename Reclaimer::thread_control_block &data) -> bool {
            // TSan does not support explicit fences, so we cannot rely on the acquire-fence (6)
            // but have to perform an acquire-load here to avoid false positives.
            constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
            return data.is_in_critical_region.load(memory_order) &&
                   data.local_epoch.load(memory_order) != epoch;
          };

          // If any thread hasn't advanced to the current epoch, abort the attempt.
          return !std::any_of(Reclaimer::global_thread_block_list.begin(),
                              Reclaimer::global_thread_block_list.end(),
                              prevents_update);
        }

        void reset() {}
      };
    };

    /**
     * @brief Scan _N_ threads.
     */
    template<unsigned N>
    struct n_threads {
      template<class Reclaimer>
      struct type {
        type() { reset(); }

        bool scan(typename Reclaimer::epoch_t epoch) {
          for (unsigned i = 0; i < N; ++i) {
            // TSan does not support explicit fences, so we cannot rely on the acquire-fence (6)
            // but have to perform an acquire-load here to avoid false positives.
            constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
            if (!thread_iterator->is_in_critical_region.load(memory_order) ||
                thread_iterator->local_epoch.load(memory_order) == epoch) {
              if (++thread_iterator == Reclaimer::global_thread_block_list.end())
                return true;
            }
          }
          return false;
        }

        void reset() {
          thread_iterator = Reclaimer::global_thread_block_list.begin();
        }

      private:
        typename detail::thread_block_list<typename Reclaimer::thread_control_block>::iterator thread_iterator;
      };
    };

    /**
     * @brief Scan a single thread (default behaviour in DEBRA).
     */
    struct one_thread {
      template<class Reclaimer>
      using type = n_threads<1>::type<Reclaimer>;
    };
  }

  namespace abandon {
    /**
     * @brief Never abandon any nodes (except when the thread terminates).
     */
    struct never {
      using retire_list = detail::retire_list<>;
      static void apply(retire_list&, detail::orphan_list<>&) {}
    };

    /**
     * @brief Always abandon the remaining retired nodes when the thread leaves
     * its critical region.
     */
    struct always {
      using retire_list = detail::retire_list<>;
      static void apply(retire_list& retire_list, detail::orphan_list<>& orphans)
      {
        if (!retire_list.empty())
          orphans.add(retire_list.steal());
      }
    };

    /**
     * @brief Abandon the retired nodes upon leaving the critical region when the
     * number of nodes exceeds the specified threshold.
     */
    template <size_t Threshold>
    struct when_exceeds_threshold {
      using retire_list = detail::counting_retire_list<>;
      static void apply(retire_list& retire_list, detail::orphan_list<>& orphans)
      {
        if (retire_list.size() >= Threshold)
          orphans.add(retire_list.steal());
      }
    };
  }

  template <class Traits>
  generic_epoch_based<Traits>::region_guard::region_guard() noexcept
  {
    local_thread_data().enter_region();
  }

  template <class Traits>
  generic_epoch_based<Traits>::region_guard::~region_guard() noexcept
  {
    local_thread_data().leave_region();
  }

  template <class Traits>
  template <class T, class MarkedPtr>
  generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::guard_ptr(const MarkedPtr& p) noexcept :
    base(p)
  {
    if (this->ptr)
      local_thread_data().enter_critical();
  }

  template <class Traits>
  template <class T, class MarkedPtr>
  generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::guard_ptr(const guard_ptr& p) noexcept :
    guard_ptr(MarkedPtr(p))
  {}

  template <class Traits>
  template <class T, class MarkedPtr>
  generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::guard_ptr(guard_ptr&& p) noexcept :
    base(p.ptr)
  {
    p.ptr.reset();
  }

  template <class Traits>
  template <class T, class MarkedPtr>
  auto generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::operator=(const guard_ptr& p) noexcept
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

  template <class Traits>
  template <class T, class MarkedPtr>
  auto generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::operator=(guard_ptr&& p) noexcept
  -> guard_ptr&
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = std::move(p.ptr);
    p.ptr.reset();

    return *this;
  }

  template <class Traits>
  template <class T, class MarkedPtr>
  void generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::acquire(
    const concurrent_ptr<T>& p, std::memory_order order) noexcept
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

  template <class Traits>
  template <class T, class MarkedPtr>
  bool generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::acquire_if_equal(
    const concurrent_ptr<T>& p, const MarkedPtr& expected, std::memory_order order) noexcept
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

  template <class Traits>
  template <class T, class MarkedPtr>
  void generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::reset() noexcept
  {
    if (this->ptr)
      local_thread_data().leave_critical();
    this->ptr.reset();
  }

  template <class Traits>
  template <class T, class MarkedPtr>
  void generic_epoch_based<Traits>::guard_ptr<T, MarkedPtr>::reclaim(Deleter d) noexcept
  {
    this->ptr->set_deleter(std::move(d));
    local_thread_data().add_retired_node(this->ptr.get());
    reset();
  }

  template <class Traits>
  struct generic_epoch_based<Traits>::thread_control_block :
    detail::thread_block_list<thread_control_block>::entry
  {
    thread_control_block() :
      is_in_critical_region(false),
      local_epoch(number_epochs)
    {}

    std::atomic<bool> is_in_critical_region;
    std::atomic<epoch_t> local_epoch;
  };

  template <class Traits>
  struct generic_epoch_based<Traits>::thread_data
  {
    ~thread_data()
    {
      if (control_block == nullptr)
        return; // no control_block -> nothing to do

      for (unsigned i = 0; i < number_epochs; ++i)
        if (!retire_lists[i].empty())
          orphans[i].add(retire_lists[i].steal());

      assert(control_block->is_in_critical_region.load(std::memory_order_relaxed) == false);
      global_thread_block_list.release_entry(control_block);
    }

    void enter_region()
    {
      ensure_has_control_block();
      if (Traits::region_extension_type != region_extension::none && ++region_entries == 1)
      {
        if (Traits::region_extension_type == region_extension::eager)
          set_critical_region_flag();
      }
    }

    void leave_region()
    {
      if (Traits::region_extension_type != region_extension::none && --region_entries == 0)
      {
        clear_critical_region_flag();
      }
    }

    void enter_critical()
    {
      enter_region();
      if (++nested_critical_entries == 1)
        do_enter_critical();
    }

    void leave_critical()
    {
      if (--nested_critical_entries == 0 && Traits::region_extension_type == region_extension::none)
        clear_critical_region_flag();
      leave_region();
    }

  private:
    void ensure_has_control_block()
    {
      if (BOOST_UNLIKELY(control_block == nullptr))
        acquire_control_block();
    }

    BOOST_NOINLINE void acquire_control_block()
    {
      assert(control_block == nullptr);
      control_block = global_thread_block_list.acquire_entry();
      assert(control_block->is_in_critical_region.load() == false);
      auto epoch = global_epoch.load(std::memory_order_relaxed);
      control_block->local_epoch.store(epoch, std::memory_order_relaxed);
      local_epoch_idx = epoch % number_epochs;
      scan_strategy.reset();
    }

    void set_critical_region_flag()
    {
      assert(!control_block->is_in_critical_region.load(std::memory_order_relaxed));
      control_block->is_in_critical_region.store(true, std::memory_order_relaxed);
      // (3) - this seq_cst-fence enforces a total order with itself
      std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void clear_critical_region_flag()
    {
      //if (Traits::region_extension_type == region_extension::lazy && control_block == nullptr)
      //  return;

      assert(control_block != nullptr);
      assert(control_block->is_in_critical_region.load(std::memory_order_relaxed));

      // (4) - this release-store synchronizes-with the acquire-fence (6)
      control_block->is_in_critical_region.store(false, std::memory_order_release);
      for (unsigned i = 0; i < number_epochs; ++i)
        Traits::abandon_strategy::apply(retire_lists[i], orphans[i]);
    }

    void do_enter_critical()
    {
      if (Traits::region_extension_type == region_extension::none)
        set_critical_region_flag();
      else if (Traits::region_extension_type == region_extension::lazy)
      {
        if (!control_block->is_in_critical_region.load(std::memory_order_relaxed))
          set_critical_region_flag();
      }

      assert(control_block->is_in_critical_region.load(std::memory_order_relaxed));

      // (5) - this acquire-load synchronizes-with the release-CAS (7)
      auto epoch = global_epoch.load(std::memory_order_acquire);
      if (control_block->local_epoch.load(std::memory_order_relaxed) != epoch) // New epoch?
      {
        critical_entries_since_update = 0;
        update_local_epoch(epoch);
      }
      else if (critical_entries_since_update++ == Traits::scan_frequency)
      {
        critical_entries_since_update = 0;
        if (scan_strategy.scan(epoch))
        {
          epoch = update_global_epoch(epoch, epoch + 1);
          update_local_epoch(epoch);
        }
      }
    }

    void update_local_epoch(epoch_t new_epoch)
    {
      // we either just updated the global_epoch or we are observing a new epoch
      // from some other thread either way - we can reclaim all the objects from
      // the old 'incarnation' of this epoch, as well as from other epochs we
      // might have missed when we were not in a critical region.

      const auto old_epoch = control_block->local_epoch.load(std::memory_order_relaxed);
      assert(new_epoch > old_epoch);
      control_block->local_epoch.store(new_epoch, std::memory_order_relaxed);

      auto diff = std::min<int>(number_epochs, new_epoch - old_epoch);
      epoch_t epoch_idx;
      for (int i = diff - 1; i >= 0; --i)
      {
        epoch_idx = (new_epoch - i) % number_epochs;
        auto nodes = retire_lists[epoch_idx].steal();
        detail::delete_objects(nodes.first);
      }
      local_epoch_idx = epoch_idx;

      scan_strategy.reset();
    }

    epoch_t update_global_epoch(epoch_t curr_epoch, epoch_t new_epoch)
    {
      if (global_epoch.load(std::memory_order_relaxed) == curr_epoch)
      {
        // (6) - this acquire-fence synchronizes-with the release-store (4)
        std::atomic_thread_fence(std::memory_order_acquire);

        // (7) - this release-CAS synchronizes-with the acquire-load (5)
        bool success = global_epoch.compare_exchange_strong(curr_epoch, new_epoch,
                                                            std::memory_order_release,
                                                            std::memory_order_relaxed);
        if (BOOST_LIKELY(success))
          reclaim_orphans(new_epoch);
      }
      return new_epoch;
    }

    void add_retired_node(detail::deletable_object* p)
    {
      retire_lists[local_epoch_idx].push(p);
    }

    void reclaim_orphans(epoch_t epoch)
    {
      auto idx = epoch % number_epochs;
      auto nodes = orphans[idx].adopt();
      detail::delete_objects(nodes);
    }

    unsigned critical_entries_since_update = 0;
    unsigned nested_critical_entries = 0;
    unsigned region_entries = 0;
    typename Traits::template scan_strategy<generic_epoch_based> scan_strategy;
    thread_control_block* control_block = nullptr;
    epoch_t local_epoch_idx;
    std::array<typename Traits::abandon_strategy::retire_list, number_epochs> retire_lists = {};

    friend class generic_epoch_based;
    ALLOCATION_COUNTER(generic_epoch_based);
  };

  template <class Traits>
  std::atomic<typename generic_epoch_based<Traits>::epoch_t> generic_epoch_based<Traits>::global_epoch;

  template <class Traits>
  detail::thread_block_list<typename generic_epoch_based<Traits>::thread_control_block>
    generic_epoch_based<Traits>::global_thread_block_list;

  template <class Traits>
  std::array<detail::orphan_list<>, generic_epoch_based<Traits>::number_epochs>
    generic_epoch_based<Traits>::orphans;

  template <class Traits>
  inline typename generic_epoch_based<Traits>::thread_data& generic_epoch_based<Traits>::local_thread_data()
  {
    static thread_local thread_data local_thread_data;
    return local_thread_data;
  }

#ifdef TRACK_ALLOCATIONS
  template <class Traits>
  detail::allocation_tracker generic_epoch_based<Traits>::allocation_tracker;

  template <class Traits>
  inline void generic_epoch_based<Traits>::count_allocation()
  { local_thread_data().allocation_counter.count_allocation(); }

  template <class Traits>
  inline void generic_epoch_based<Traits>::count_reclamation()
  { local_thread_data().allocation_counter.count_reclamation(); }
#endif
}}
