//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_LOCK_FREE_REF_COUNT_HPP
#define XENIUM_LOCK_FREE_REF_COUNT_HPP

#include <xenium/reclamation/detail/concurrent_ptr.hpp>
#include <xenium/reclamation/detail/guard_ptr.hpp>
#include <xenium/reclamation/detail/allocation_tracker.hpp>

#include <xenium/acquire_guard.hpp>

#include <memory>

namespace xenium { namespace reclamation {

  /**
   * @brief Lock-free reference counting
   * @tparam InsertPadding
   * @tparam ThreadLocalFreeListSize
   */
  template <bool InsertPadding = false, size_t ThreadLocalFreeListSize = 0>
  class lock_free_ref_count
  {
    template <class T, class MarkedPtr>
    class guard_ptr;

  public:
    template <class T, std::size_t N = T::number_of_mark_bits>
    using concurrent_ptr = detail::concurrent_ptr<T, N, guard_ptr>;

    template <class T, std::size_t N = 0, class DeleterT = std::default_delete<T>>
    class enable_concurrent_ptr;

    class region_guard {};

    ALLOCATION_TRACKER
  private:
    static constexpr unsigned RefCountInc = 2;
    static constexpr unsigned RefCountClaimBit = 1;

    ALLOCATION_TRACKING_FUNCTIONS;
#ifdef TRACK_ALLOCATIONS
    static detail::allocation_counter& allocation_counter();
#endif
  };

  template <bool InsertPadding, size_t ThreadLocalFreeListSize>
  template <class T, std::size_t N, class DeleterT>
  class lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::enable_concurrent_ptr:
    private detail::tracked_object<lock_free_ref_count>
  {
  protected:
    enable_concurrent_ptr() noexcept
    {
      destroyed().store(false, std::memory_order_relaxed);
    }
    enable_concurrent_ptr(const enable_concurrent_ptr&) noexcept = delete;
    enable_concurrent_ptr(enable_concurrent_ptr&&) noexcept = delete;
    enable_concurrent_ptr& operator=(const enable_concurrent_ptr&) noexcept = delete;
    enable_concurrent_ptr& operator=(enable_concurrent_ptr&&) noexcept = delete;
    virtual ~enable_concurrent_ptr() noexcept
    {
      assert(!is_destroyed());
      destroyed().store(true, std::memory_order_relaxed);
    }
  public:
    using Deleter = DeleterT;
    static_assert(std::is_same<Deleter, std::default_delete<T>>::value,
                  "lock_free_ref_count reclamation can only be used with std::default_delete as Deleter.");

    static constexpr std::size_t number_of_mark_bits = N;
    unsigned refs() const { return getHeader()->ref_count.load(std::memory_order_relaxed) >> 1; }

    void* operator new(size_t sz);
    void operator delete(void* p);

  private:
    bool decrement_refcnt();
    bool is_destroyed() const { return getHeader()->destroyed.load(std::memory_order_relaxed); }
    void push_to_free_list() { global_free_list.push(static_cast<T*>(this)); }

    struct unpadded_header {
      std::atomic<unsigned> ref_count;
      std::atomic<bool> destroyed;
      concurrent_ptr<T, N> next_free;
    };
    struct padded_header : unpadded_header {
      char padding[64 - sizeof(unpadded_header)];
    };
    using header = std::conditional_t<InsertPadding, padded_header, unpadded_header>;
    header* getHeader() { return static_cast<header*>(static_cast<void*>(this)) - 1; }
    const header* getHeader() const { return static_cast<const header*>(static_cast<const void*>(this)) - 1; }

    std::atomic<unsigned>& ref_count() { return getHeader()->ref_count; }
    std::atomic<bool>& destroyed() { return getHeader()->destroyed; }
    concurrent_ptr<T, N>& next_free() { return getHeader()->next_free; }

    friend class lock_free_ref_count;

    using guard_ptr = typename concurrent_ptr<T, N>::guard_ptr;
    using marked_ptr = typename concurrent_ptr<T, N>::marked_ptr;

    class free_list;
    static free_list global_free_list;
  };

  template <bool InsertPadding, size_t ThreadLocalFreeListSize>
  template <class T, class MarkedPtr>
  class lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::guard_ptr :
      public detail::guard_ptr<T, MarkedPtr, guard_ptr<T, MarkedPtr>>
  {
    using base = detail::guard_ptr<T, MarkedPtr, guard_ptr>;
    using Deleter = typename T::Deleter;
  public:
    template <class, std::size_t, class>
    friend class enable_concurrent_ptr;

    // Guard a marked ptr.
    guard_ptr(const MarkedPtr& p = MarkedPtr()) noexcept;
    explicit guard_ptr(const guard_ptr& p) noexcept;
    guard_ptr(guard_ptr&& p) noexcept;

    guard_ptr& operator=(const guard_ptr& p);
    guard_ptr& operator=(guard_ptr&& p);

    // Atomically take snapshot of p, and *if* it points to unreclaimed object, acquire shared ownership of it.
    void acquire(const concurrent_ptr<T>& p, std::memory_order order = std::memory_order_seq_cst) noexcept;

    // Like acquire, but quit early if a snapshot != expected.
    bool acquire_if_equal(const concurrent_ptr<T>& p,
                          const MarkedPtr& expected,
                          std::memory_order order = std::memory_order_seq_cst) noexcept;

    // Release ownership. Postcondition: get() == nullptr.
    void reset() noexcept;

    // Reset. Deleter d will be applied some time after all owners release their ownership.
    void reclaim(Deleter d = Deleter()) noexcept;
  };
}}

#define LOCK_FREE_REF_COUNT_IMPL
#include <xenium/reclamation/impl/lock_free_ref_count.hpp>
#undef LOCK_FREE_REF_COUNT_IMPL

#endif
