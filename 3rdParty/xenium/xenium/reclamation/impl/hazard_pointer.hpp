//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef HAZARD_POINTER_IMPL
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
  hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::guard_ptr(const MarkedPtr& p) :
    base(p),
    hp()
  {
    if (this->ptr.get() != nullptr)
    {
      hp = local_thread_data.alloc_hazard_pointer();
      hp->set_object(this->ptr.get());
    }
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::guard_ptr(const guard_ptr& p) :
    guard_ptr(p.ptr)
  {}

  template <typename Policy>
  template <class T, class MarkedPtr>
  hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::guard_ptr(guard_ptr&& p) noexcept :
    base(p.ptr),
    hp(p.hp)
  {
    p.ptr.reset();
    p.hp = nullptr;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  auto hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::operator=(const guard_ptr& p) noexcept
    -> guard_ptr&
  {
    if (&p == this)
      return *this;

    if (hp == nullptr)
      hp = local_thread_data.alloc_hazard_pointer();
    this->ptr = p.ptr;
    hp->set_object(this->ptr.get());
    return *this;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  auto hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::operator=(guard_ptr&& p) noexcept
    -> guard_ptr&
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = std::move(p.ptr);
    hp = p.hp;
    p.ptr.reset();
    p.hp = nullptr;
    return *this;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  void hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::acquire(const concurrent_ptr<T>& p,
    std::memory_order order)
  {
    auto p1 = p.load(std::memory_order_relaxed);
    if (p1 == this->ptr)
      return;
    if (p1 != nullptr && hp == nullptr)
      hp = local_thread_data.alloc_hazard_pointer();
    auto p2 = p1;
    do
    {
      if (p2 == nullptr)
      {
        reset();
        return;
      }

      p1 = p2;
      hp->set_object(p1.get());
      // (1) - this load operation potentially synchronizes-with any release operation on p.
      p2 = p.load(order);
    } while (p1.get() != p2.get());

    this->ptr = p2;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  bool hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::acquire_if_equal(
    const concurrent_ptr<T>& p,
    const MarkedPtr& expected,
    std::memory_order order)
  {
    auto p1 = p.load(std::memory_order_relaxed);
    if (p1 == nullptr || p1 != expected)
    {
      reset();
      return p1 == expected;
    }

    if (hp == nullptr)
      hp = local_thread_data.alloc_hazard_pointer();
    hp->set_object(p1.get());
    // (2) - this load operation potentially synchronizes-with any release operation on p.
    this->ptr = p.load(order);
    if (this->ptr != p1)
    {
      reset();
      return false;
    }
    return true;
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  void hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::reset() noexcept
  {
    local_thread_data.release_hazard_pointer(hp);
    this->ptr.reset();
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  void hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::do_swap(guard_ptr& g) noexcept
  {
    std::swap(hp, g.hp);
  }

  template <typename Policy>
  template <class T, class MarkedPtr>
  void hazard_pointer<Policy>::guard_ptr<T, MarkedPtr>::reclaim(Deleter d) noexcept
  {
    auto p = this->ptr.get();
    reset();
    p->set_deleter(std::move(d));
    if (local_thread_data.add_retired_node(p) >= Policy::retired_nodes_threshold())
      local_thread_data.scan();
  }

  template <class Policy, class Derived>
  struct alignas(64) basic_hp_thread_control_block :
    detail::thread_block_list<Derived>::entry,
    aligned_object<basic_hp_thread_control_block<Policy, Derived>>
  {
    struct hazard_pointer
    {
      void set_object(detail::deletable_object* obj)
      {
        // (3) - this release-store synchronizes-with the acquire-fence (9)
        value.store(reinterpret_cast<void**>(obj), std::memory_order_release);
        // This release is required because when acquire/acquire_if_equal is called on a
        // guard_ptr with with an active HE entry, set_era is called without an intermediate
        // call to set_link, i.e., the protected era is updated. This ensures the required
        // happens-before relation between releasing a guard_ptr to a node and reclamation
        // of that node.

        // (4) - this seq_cst-fence enforces a total order with the seq_cst-fence (8)
        std::atomic_thread_fence(std::memory_order_seq_cst);
      }

      bool try_get_object(detail::deletable_object*& result) const
      {
        // TSan does not support explicit fences, so we cannot rely on the acquire-fence (9)
        // but have to perform an acquire-load here to avoid false positives.
        constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
        auto v = value.load(memory_order);
        if (v.mark() == 0)
        {
          result = reinterpret_cast<detail::deletable_object*>(v.get());
          return true;
        }
        return false; // value contains a link
      }

      void set_link(hazard_pointer* link)
      {
        // (5) - this release store synchronizes-with the acquire fence (9)
        value.store(detail::marked_ptr<void*, 1>(reinterpret_cast<void**>(link), 1), std::memory_order_release);
      }
      hazard_pointer* get_link() const
      {
        assert(is_link());
        return reinterpret_cast<hazard_pointer*>(value.load(std::memory_order_relaxed).get());
      }

      bool is_link() const
      {
        return value.load(std::memory_order_relaxed).mark() != 0;
      }
    private:
      // since we use the hazard pointer array to build our internal linked list of hazard pointers
      // we set the LSB to signal that this is an internal pointer and not a pointer to a protected object.
      std::atomic<detail::marked_ptr<void*, 1>> value;
    };

    using hint = hazard_pointer*;

    void initialize(hint& hint)
    {
      Policy::number_of_active_hps.fetch_add(self().number_of_hps(), std::memory_order_relaxed);
      hint = initialize_block(self());
    }

    void abandon()
    {
      Policy::number_of_active_hps.fetch_sub(self().number_of_hps(), std::memory_order_relaxed);
      detail::thread_block_list<Derived>::entry::abandon();
    }

    hazard_pointer* alloc_hazard_pointer(hint& hint)
    {
      auto result = hint;
      if (result == nullptr)
        result = self().need_more_hps();

      hint = result->get_link();
      return result;
    }

    void release_hazard_pointer(hazard_pointer*& hp, hint& hint)
    {
      if (hp != nullptr)
      {
        hp->set_link(hint);
        hint = hp;
        hp = nullptr;
      }
    }

  protected:
    Derived& self() { return static_cast<Derived&>(*this); }

    hazard_pointer* begin() { return &pointers[0]; }
    hazard_pointer* end() { return &pointers[Policy::K]; }
    const hazard_pointer* begin() const { return &pointers[0]; }
    const hazard_pointer* end() const { return &pointers[Policy::K]; }

    template <typename T>
    static hazard_pointer* initialize_block(T& block)
    {
      auto begin = block.begin();
      auto end = block.end() - 1; // the last element is handled specially, so loop only over n-1 entries
      for (auto it = begin; it != end;)
      {
        auto next = it + 1;
        it->set_link(next);
        it = next;
      }
      end->set_link(block.initialize_next_block());
      return begin;
    }

    static void gather_protected_pointers(std::vector<const detail::deletable_object*>& protected_ptrs,
      const hazard_pointer* begin, const hazard_pointer* end)
    {
      for (auto it = begin; it != end; ++it)
      {
        detail::deletable_object* obj;
        if (it->try_get_object(obj))
          protected_ptrs.push_back(obj);
      }
    }

    hazard_pointer pointers[Policy::K];
  };

  template <class Policy>
  struct static_hp_thread_control_block :
    basic_hp_thread_control_block<Policy, static_hp_thread_control_block<Policy>>
  {
    using base = basic_hp_thread_control_block<Policy, static_hp_thread_control_block>;
    using hazard_pointer = typename base::hazard_pointer;
    friend base;

    void gather_protected_pointers(std::vector<const detail::deletable_object*>& protected_ptrs) const
    {
      base::gather_protected_pointers(protected_ptrs, this->begin(), this->end());
    }
  private:
    hazard_pointer* need_more_hps() { throw bad_hazard_pointer_alloc("hazard pointer pool exceeded"); }
    constexpr size_t number_of_hps() const { return Policy::K; }
    constexpr hazard_pointer* initialize_next_block() const { return nullptr; }
  };

  template <class Policy>
  struct dynamic_hp_thread_control_block :
    basic_hp_thread_control_block<Policy, dynamic_hp_thread_control_block<Policy>>
  {
    using base = basic_hp_thread_control_block<Policy, dynamic_hp_thread_control_block>;
    using hazard_pointer = typename base::hazard_pointer;
    friend base;

    void gather_protected_pointers(std::vector<const detail::deletable_object*>& protected_ptrs) const
    {
      gather_protected_pointers(*this, protected_ptrs);
    }

  private:
    struct alignas(64) hazard_pointer_block : aligned_object<hazard_pointer_block>
    {
      hazard_pointer_block(size_t size) : size(size) {}

      hazard_pointer* begin() { return reinterpret_cast<hazard_pointer*>(this + 1); }
      hazard_pointer* end() { return begin() + size; }

      const hazard_pointer* begin() const { return reinterpret_cast<const hazard_pointer*>(this + 1); }
      const hazard_pointer* end() const { return begin() + size; }

      const hazard_pointer_block* next_block() const { return next; }
      hazard_pointer* initialize_next_block() { return next ? base::initialize_block(*next) : nullptr; }

      hazard_pointer_block* next = nullptr;
      const size_t size;
    };

    const hazard_pointer_block* next_block() const
    {
      // (6) - this acquire-load synchronizes-with the release-store (7)
      return hp_block.load(std::memory_order_acquire);
    }
    size_t number_of_hps() const { return total_number_of_hps; }
    hazard_pointer* need_more_hps() { return allocate_new_hazard_pointer_block(); }

    
    hazard_pointer* initialize_next_block()
    {
      auto block = hp_block.load(std::memory_order_relaxed);
      return block ? base::initialize_block(*block) : nullptr;
    }

    template <typename T>
    static void gather_protected_pointers(const T& block, std::vector<const detail::deletable_object*>& protected_ptrs)
    {
      base::gather_protected_pointers(protected_ptrs, block.begin(), block.end());

      auto next = block.next_block();
      if (next)
        gather_protected_pointers(*next, protected_ptrs);
    }

    static detail::deletable_object* as_internal_pointer(hazard_pointer* p)
    {
      // since we use the hazard pointer array to build our internal linked list of hazard pointers
      // we set the LSB to signal that this is an internal pointer and not a pointer to a protected object.
      auto marked = reinterpret_cast<size_t>(p) | 1;
      return reinterpret_cast<detail::deletable_object*>(marked);
    }

    hazard_pointer* allocate_new_hazard_pointer_block()
    {
      size_t hps = std::max(static_cast<size_t>(Policy::K), total_number_of_hps / 2);
      total_number_of_hps += hps;
      Policy::number_of_active_hps.fetch_add(hps, std::memory_order_relaxed);

      size_t buffer_size = sizeof(hazard_pointer_block) + hps * sizeof(hazard_pointer);
      void* buffer = hazard_pointer_block::operator new(buffer_size);
      auto block = ::new(buffer) hazard_pointer_block(hps);
      auto result = this->initialize_block(*block);
      block->next = hp_block.load(std::memory_order_relaxed);
      // (7) - this release-store synchronizes-with the acquire-load (6)
      hp_block.store(block, std::memory_order_release);
      return result;
    }

    size_t total_number_of_hps = Policy::K;
    std::atomic<hazard_pointer_block*> hp_block;
  };

  template <typename Policy>
  struct alignas(64) hazard_pointer<Policy>::thread_data : aligned_object<thread_data>
  {
    using HP = typename thread_control_block::hazard_pointer*;

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

    HP alloc_hazard_pointer()
    {
      ensure_has_control_block();
      return control_block->alloc_hazard_pointer(hint);
    }

    void release_hazard_pointer(HP& hp)
    {
      control_block->release_hazard_pointer(hp, hint);
    }

    std::size_t add_retired_node(detail::deletable_object* p)
    {
      p->next = retire_list;
      retire_list = p;
      return ++number_of_retired_nodes;
    }

    void scan()
    {
      std::vector<const detail::deletable_object*> protected_pointers;
      protected_pointers.reserve(Policy::number_of_active_hazard_pointers());

      // (8) - this seq_cst-fence enforces a total order with the seq_cst-fence (4)
      std::atomic_thread_fence(std::memory_order_seq_cst);

      auto adopted_nodes = global_thread_block_list.adopt_abandoned_retired_nodes();

      std::for_each(global_thread_block_list.begin(), global_thread_block_list.end(),
        [&protected_pointers](const auto& entry)
        {
          // TSan does not support explicit fences, so we cannot rely on the acquire-fence (9)
          // but have to perform an acquire-load here to avoid false positives.
          constexpr auto memory_order = TSAN_MEMORY_ORDER(std::memory_order_acquire, std::memory_order_relaxed);
          if (entry.is_active(memory_order))
            entry.gather_protected_pointers(protected_pointers);
        });

      // (9) - this acquire-fence synchronizes-with the release-store (3, 5)
      std::atomic_thread_fence(std::memory_order_acquire);

      std::sort(protected_pointers.begin(), protected_pointers.end());

      auto list = retire_list;
      retire_list = nullptr;
      number_of_retired_nodes = 0;
      reclaim_nodes(list, protected_pointers);
      reclaim_nodes(adopted_nodes, protected_pointers);
    }

  private:
    void ensure_has_control_block()
    {
      if (control_block == nullptr)
      {
        control_block = global_thread_block_list.acquire_entry();
        control_block->initialize(hint);
      }
    }

    void reclaim_nodes(detail::deletable_object* list,
      const std::vector<const detail::deletable_object*>& protected_pointers)
    {
      while (list != nullptr)
      {
        auto cur = list;
        list = list->next;

        if (std::binary_search(protected_pointers.begin(), protected_pointers.end(), cur))
          add_retired_node(cur);
        else
          cur->delete_self();
      }
    }
    detail::deletable_object* retire_list = nullptr;
    std::size_t number_of_retired_nodes = 0;
    typename thread_control_block::hint hint;

    thread_control_block* control_block = nullptr;

    friend class hazard_pointer;
    ALLOCATION_COUNTER(hazard_pointer);
  };

  template <size_t K, size_t A, size_t B, template <class> class ThreadControlBlock>
  std::atomic<size_t> generic_hazard_pointer_policy<K ,A, B, ThreadControlBlock>::number_of_active_hps;

  template <typename Policy>
  detail::thread_block_list<typename hazard_pointer<Policy>::thread_control_block>
    hazard_pointer<Policy>::global_thread_block_list;

  template <typename Policy>
  thread_local typename hazard_pointer<Policy>::thread_data hazard_pointer<Policy>::local_thread_data;

#ifdef TRACK_ALLOCATIONS
  template <typename Policy>
  detail::allocation_tracker hazard_pointer<Policy>::allocation_tracker;

  template <typename Policy>
  inline void hazard_pointer<Policy>::count_allocation()
  { local_thread_data.allocation_counter.count_allocation(); }

  template <typename Policy>
  inline void hazard_pointer<Policy>::count_reclamation()
  { local_thread_data.allocation_counter.count_reclamation(); }
#endif
}}
