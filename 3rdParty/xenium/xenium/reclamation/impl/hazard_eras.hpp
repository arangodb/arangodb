//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef HAZARD_ERAS_IMPL
#error "This is an impl file and must not be included directly!"
#endif

#include <xenium/aligned_object.hpp>
#include <xenium/detail/port.hpp>

#include <algorithm>
#include <new>
#include <vector>

namespace xenium { namespace reclamation {

  template <typename Policy>
  template <class T, class MarkedPtr>
  hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::guard_ptr(const MarkedPtr& p) :
    base(p),
    he()
  {
    if (this->ptr.get() != nullptr)
    {
      auto era = era_clock.load(std::memory_order_relaxed);
      he = local_thread_data.alloc_hazard_era(era);
    }
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::guard_ptr(const guard_ptr& p) :
    base(p.ptr),
    he(p.he)
  {
    if (he)
      he->add_guard();
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::guard_ptr(guard_ptr&& p) noexcept :
    base(p.ptr),
    he(p.he)
  {
    p.ptr.reset();
    p.he = nullptr;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  auto hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::operator=(const guard_ptr& p) noexcept
    -> guard_ptr&
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = p.ptr;
    he = p.he;
    if (he)
      he->add_guard();
    return *this;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  auto hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::operator=(guard_ptr&& p) noexcept
    -> guard_ptr&
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = std::move(p.ptr);
    this->he = p.he;
    p.ptr.reset();
    p.he = nullptr;
    return *this;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  void hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::acquire(const concurrent_ptr<T>& p,
    std::memory_order order)
  {
    if (order == std::memory_order_relaxed || order == std::memory_order_consume) {
      // we have to use memory_order_acquire (or something stricter) to ensure that
      // the era_clock.load cannot return an outdated value.
      order = std::memory_order_acquire;
    }
    auto prev_era = he == nullptr ? 0 : he->get_era();
    for (;;) {
      // (1) - this load operation synchronizes-with any release operation on p.
      // we have to use acquire here to ensure that the subsequent era_clock.load
      // sees a value >= p.construction_era
      auto ptr = p.load(order);

      auto era = era_clock.load(std::memory_order_relaxed);
      if (era == prev_era) {
        this->ptr = ptr;
        return;
      }

      if (he != nullptr)
      {
        assert(he->get_era() != era);
        if (he->guards() == 1)
        {
          // we are the only guard using this HE instance -> reuse it and simply update the era
          he->set_era(era);
          prev_era = era;
          continue;
        }
        else {
          he->release_guard();
          he = nullptr;
        }
      }
      assert(he == nullptr);
      he = local_thread_data.alloc_hazard_era(era);
      prev_era = era;
    }
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  bool hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::acquire_if_equal(
    const concurrent_ptr<T>& p,
    const MarkedPtr& expected,
    std::memory_order order)
  {
    if (order == std::memory_order_relaxed || order == std::memory_order_consume) {
      // we have to use memory_order_acquire (or something stricter) to ensure that
      // the era_clock.load cannot return an outdated value.
      order = std::memory_order_acquire;
    }

    // (2) - this load operation synchronizes-with any release operation on p.
    // we have to use acquire here to ensure that the subsequent era_clock.load
    // sees a value >= p.construction_era
    auto p1 = p.load(order);
    if (p1 == nullptr || p1 != expected) {
      reset();
      return p1 == expected;
    }

    const auto era = era_clock.load(std::memory_order_relaxed);
    if (he != nullptr && he->guards() == 1)
      he->set_era(era);
    else {
      if (he != nullptr)
        he->release_guard();

      he = local_thread_data.alloc_hazard_era(era);
    }

    this->ptr = p.load(std::memory_order_relaxed);
    if (this->ptr != p1)
    {
      reset();
      return false;
    }
    return true;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  void hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::reset() noexcept
  {
    local_thread_data.release_hazard_era(he);
    assert(this->he == nullptr);
    this->ptr.reset();
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  void hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::do_swap(guard_ptr& g) noexcept
  {
    std::swap(he, g.he);
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  void hazard_eras<Policy>::guard_ptr<T, MarkedPtr>::reclaim(Deleter d) noexcept
  {
    auto p = this->ptr.get();
    reset();
    p->set_deleter(std::move(d));
    // (3) - this release fetch-add synchronizes-with the seq-cst fence (5)
    p->retirement_era = era_clock.fetch_add(1, std::memory_order_release);

    if (local_thread_data.add_retired_node(p) >= Policy::retired_nodes_threshold())
      local_thread_data.scan();
  }

  template <class Policy, class Derived>
  struct alignas(64) basic_he_thread_control_block :
    detail::thread_block_list<Derived, detail::deletable_object_with_eras>::entry,
    aligned_object<basic_he_thread_control_block<Policy, Derived>>
  {
    using era_t = uint64_t;

    ~basic_he_thread_control_block() {
      assert(last_hazard_era != nullptr);
    }

    struct hazard_era
    {
      hazard_era(): value(nullptr), guard_cnt(0) {}

      void set_era(era_t era)
      {
        assert(era != 0);
        // (4) - this release-store synchronizes-with the acquire-fence (10)
        value.store(marked_ptr(reinterpret_cast<void**>(era << 1)), std::memory_order_release);
        // This release is required because when acquire/acquire_if_equal is called on a
        // guard_ptr with with an active HE entry, set_era is called without an intermediate
        // call to set_link, i.e., the protected era is updated. This ensures the required
        // happens-before relation between releasing a guard_ptr to a node and reclamation
        // of that node.

        // (5) - this seq_cst-fence enforces a total order with the seq_cst-fence (9)
        //       and synchronizes-with the release-fetch-add (3)
        std::atomic_thread_fence(std::memory_order_seq_cst);
      }

      era_t get_era() const
      {
        era_t result;
        bool success = try_get_era(result);
        assert(success);
        assert(result != 0);
        return result;
      }

      uint64_t guards() const { return guard_cnt; }
      uint64_t add_guard() { return ++guard_cnt; }
      uint64_t release_guard() { return --guard_cnt; }

      bool try_get_era(era_t& result) const
      {
        // TSan does not support explicit fences, so we cannot rely on the acquire-fence (10)
        // but have to perform an acquire-load here to avoid false positives.
        constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
        auto v = value.load(memory_order);
        if (v.mark() == 0)
        {
          result = reinterpret_cast<era_t>(v.get()) >> 1;
          return true;
        }
        return false; // value contains a link
      }

      void set_link(hazard_era* link)
      {
        assert(guard_cnt == 0);
        // (6) - this release store synchronizes-with the acquire fence (10)
        value.store(marked_ptr(reinterpret_cast<void**>(link), 1), std::memory_order_release);
      }
      hazard_era* get_link() const
      {
        assert(is_link());
        return reinterpret_cast<hazard_era*>(value.load(std::memory_order_relaxed).get());
      }

      bool is_link() const
      {
        return value.load(std::memory_order_relaxed).mark() != 0;
      }
    private:
      using marked_ptr = detail::marked_ptr<void*, 1>;
      // since we use the hazard era array to build our internal linked list of hazard eras we set
      // the LSB to signal that this is an internal pointer and not a pointer to a protected object.
      std::atomic<marked_ptr> value;
      // the number of guard_ptrs that reference this instance and therefore observe the same era
      uint64_t guard_cnt;
    };

    using hint = hazard_era*;

    void initialize(hint& hint)
    {
      Policy::number_of_active_hes.fetch_add(self().number_of_hps(), std::memory_order_relaxed);
      hint = initialize_block(self());
    }

    void abandon()
    {
      Policy::number_of_active_hes.fetch_sub(self().number_of_hps(), std::memory_order_relaxed);
      detail::thread_block_list<Derived, detail::deletable_object_with_eras>::entry::abandon();
    }

    hazard_era* alloc_hazard_era(hint& hint, era_t era)
    {
      if (last_hazard_era && last_era == era) {
        last_hazard_era->add_guard();
        return last_hazard_era;
      }
      auto result = hint;
      if (result == nullptr)
        result = self().need_more_hes();

      hint = result->get_link();
      result->set_era(era);
      result->add_guard();

      last_hazard_era = result;
      last_era = era;
      return result;
    }

    void release_hazard_era(hazard_era*& he, hint& hint)
    {
      if (he != nullptr)
      {
        if (he->release_guard() == 0)
        {
          if (he == last_hazard_era)
            last_hazard_era = nullptr;

          he->set_link(hint);
          hint = he;
        }
        he = nullptr;
      }
    }

  protected:
    Derived& self() { return static_cast<Derived&>(*this); }

    hazard_era* begin() { return &eras[0]; }
    hazard_era* end() { return &eras[Policy::K]; }
    const hazard_era* begin() const { return &eras[0]; }
    const hazard_era* end() const { return &eras[Policy::K]; }

    template <typename T>
    static hazard_era* initialize_block(T& block)
    {
      auto begin = block.begin();
      auto end = block.end() - 1; // the last element is handled specially, so loop only over n-1 entries
      for (auto it = begin; it != end;)
      {
        auto next = it + 1;
        new(it) hazard_era;
        it->set_link(next);
        it = next;
      }
      new(end) hazard_era();
      end->set_link(block.initialize_next_block());
      return begin;
    }

    static void gather_protected_eras(std::vector<era_t>& protected_eras,
      const hazard_era* begin, const hazard_era* end)
    {
      for (auto it = begin; it != end; ++it)
      {
        era_t era;
        if (it->try_get_era(era))
          protected_eras.push_back(era);
      }
    }

    hazard_era* last_hazard_era;
    era_t last_era;
    hazard_era eras[Policy::K];
  };

  template <class Policy>
  struct static_he_thread_control_block :
    basic_he_thread_control_block<Policy, static_he_thread_control_block<Policy>>
  {
    using base = basic_he_thread_control_block<Policy, static_he_thread_control_block>;
    using hazard_era = typename base::hazard_era;
    using era_t = typename base::era_t;
    friend base;

    void gather_protected_eras(std::vector<era_t>& protected_eras) const
    {
      base::gather_protected_eras(protected_eras, this->begin(), this->end());
    }
  private:
    hazard_era* need_more_hes() {
      throw bad_hazard_era_alloc("hazard era pool exceeded"); }
    constexpr size_t number_of_hps() const { return Policy::K; }
    constexpr hazard_era* initialize_next_block() const { return nullptr; }
  };

  template <class Policy>
  struct dynamic_he_thread_control_block :
    basic_he_thread_control_block<Policy, dynamic_he_thread_control_block<Policy>>
  {
    using base = basic_he_thread_control_block<Policy, dynamic_he_thread_control_block>;
    using hazard_era = typename base::hazard_era;
    using era_t = typename base::era_t;
    friend base;

    void gather_protected_eras(std::vector<era_t>& protected_eras) const
    {
      gather_protected_eras(*this, protected_eras);
    }

  private:
    struct alignas(64) hazard_eras_block : aligned_object<hazard_eras_block>
    {
      hazard_eras_block(size_t size) : size(size) {}

      hazard_era* begin() { return reinterpret_cast<hazard_era*>(this + 1); }
      hazard_era* end() { return begin() + size; }

      const hazard_era* begin() const { return reinterpret_cast<const hazard_era*>(this + 1); }
      const hazard_era* end() const { return begin() + size; }

      const hazard_eras_block* next_block() const { return next; }
      hazard_era* initialize_next_block() { return next ? base::initialize_block(*next) : nullptr; }

      hazard_eras_block* next = nullptr;
      const size_t size;
    };

    const hazard_eras_block* next_block() const
    {
      // (7) - this acquire-load synchronizes-with the release-store (8)
      return he_block.load(std::memory_order_acquire);
    }
    size_t number_of_hps() const { return total_number_of_hps; }
    hazard_era* need_more_hes() { return allocate_new_hazard_eras_block(); }


    hazard_era* initialize_next_block()
    {
      auto block = he_block.load(std::memory_order_relaxed);
      return block ? base::initialize_block(*block) : nullptr;
    }

    template <typename T>
    static void gather_protected_eras(const T& block, std::vector<era_t>& protected_eras)
    {
      base::gather_protected_eras(protected_eras, block.begin(), block.end());

      auto next = block.next_block();
      if (next)
        gather_protected_eras(*next, protected_eras);
    }

    hazard_era* allocate_new_hazard_eras_block()
    {
      size_t hps = std::max(static_cast<size_t>(Policy::K), total_number_of_hps / 2);
      total_number_of_hps += hps;
      Policy::number_of_active_hes.fetch_add(hps, std::memory_order_relaxed);

      size_t buffer_size = sizeof(hazard_eras_block) + hps * sizeof(hazard_era);
      void* buffer = hazard_eras_block::operator new(buffer_size);
      auto block = ::new(buffer) hazard_eras_block(hps);
      auto result = this->initialize_block(*block);
      block->next = he_block.load(std::memory_order_relaxed);
      // (8) - this release-store synchronizes-with the acquire-load (7)
      he_block.store(block, std::memory_order_release);
      return result;
    }

    size_t total_number_of_hps = Policy::K;
    std::atomic<hazard_eras_block*> he_block;
  };

  template <typename Policy>
  struct alignas(64) hazard_eras<Policy>::thread_data : aligned_object<thread_data>
  {
    using HE = typename thread_control_block::hazard_era*;

    ~thread_data()
    {
      if (retire_list != nullptr)
      {
        scan();
        if (retire_list != nullptr)
          global_thread_block_list.abandon_retired_nodes(retire_list);
      }

      if (control_block != nullptr)
        global_thread_block_list.release_entry(control_block);
    }

    HE alloc_hazard_era(era_t era)
    {
      ensure_has_control_block();
      return control_block->alloc_hazard_era(hint, era);
    }

    void release_hazard_era(HE& he)
    {
      control_block->release_hazard_era(he, hint);
    }

    std::size_t add_retired_node(detail::deletable_object_with_eras* p)
    {
      p->next = retire_list;
      retire_list = p;
      return ++number_of_retired_nodes;
    }

    void scan()
    {
      std::vector<era_t> protected_eras;
      protected_eras.reserve(Policy::number_of_active_hazard_eras());

      // (9) - this seq_cst-fence enforces a total order with the seq_cst-fence (5)
      std::atomic_thread_fence(std::memory_order_seq_cst);

      auto adopted_nodes = global_thread_block_list.adopt_abandoned_retired_nodes();

      std::for_each(global_thread_block_list.begin(), global_thread_block_list.end(),
        [&protected_eras](const auto& entry)
        {
          // TSan does not support explicit fences, so we cannot rely on the acquire-fence (10)
          // but have to perform an acquire-load here to avoid false positives.
          constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
          if (entry.is_active(memory_order))
            entry.gather_protected_eras(protected_eras);
        });

      // (10) - this acquire-fence synchronizes-with the release-store (4, 6)
      std::atomic_thread_fence(std::memory_order_acquire);

      std::sort(protected_eras.begin(), protected_eras.end());
      auto last = std::unique(protected_eras.begin(), protected_eras.end());
      protected_eras.erase(last, protected_eras.end());

      auto list = retire_list;
      retire_list = nullptr;
      number_of_retired_nodes = 0;
      reclaim_nodes(list, protected_eras);
      reclaim_nodes(adopted_nodes, protected_eras);
    }

  private:
    void ensure_has_control_block()
    {
      if (control_block == nullptr)
      {
        control_block = global_thread_block_list.acquire_inactive_entry();
        control_block->initialize(hint);
        control_block->activate();
      }
    }

    void reclaim_nodes(detail::deletable_object_with_eras* list,
      const std::vector<era_t>& protected_eras)
    {
      while (list != nullptr)
      {
        auto cur = list;
        list = list->next;

        auto era_it = std::lower_bound(protected_eras.begin(), protected_eras.end(), cur->construction_era);
        if (era_it == protected_eras.end() || *era_it > cur->retirement_era)
          cur->delete_self();
        else
          add_retired_node(cur);
      }
    }

    detail::deletable_object_with_eras* retire_list = nullptr;
    std::size_t number_of_retired_nodes = 0;
    typename thread_control_block::hint hint;

    thread_control_block* control_block = nullptr;

    friend class hazard_eras;
    ALLOCATION_COUNTER(hazard_eras);
  };

  template <size_t K, size_t A, size_t B, template <class> class ThreadControlBlock>
  std::atomic<size_t> generic_hazard_eras_policy<K ,A, B, ThreadControlBlock>::number_of_active_hes;

  template <typename Policy>
  std::atomic<typename hazard_eras<Policy>::era_t>
    hazard_eras<Policy>::era_clock{1};

  template <typename Policy>
  detail::thread_block_list<typename hazard_eras<Policy>::thread_control_block, detail::deletable_object_with_eras>
    hazard_eras<Policy>::global_thread_block_list;

  template <typename Policy>
  thread_local typename hazard_eras<Policy>::thread_data hazard_eras<Policy>::local_thread_data;

#ifdef TRACK_ALLOCATIONS
  template <typename Policy>
  detail::allocation_tracker hazard_eras<Policy>::allocation_tracker;

  template <typename Policy>
  inline void hazard_eras<Policy>::count_allocation()
  { local_thread_data.allocation_counter.count_allocation(); }

  template <typename Policy>
  inline void hazard_eras<Policy>::count_reclamation()
  { local_thread_data.allocation_counter.count_reclamation(); }
#endif
}}
