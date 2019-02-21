//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_STAMP_IT_HPP
#define XENIUM_STAMP_IT_HPP

#include <xenium/reclamation/detail/concurrent_ptr.hpp>
#include <xenium/reclamation/detail/guard_ptr.hpp>
#include <xenium/reclamation/detail/deletable_object.hpp>
#include <xenium/reclamation/detail/thread_block_list.hpp>
#include <xenium/reclamation/detail/allocation_tracker.hpp>

#include <xenium/acquire_guard.hpp>

namespace xenium { namespace reclamation {

  /**
   * @brief Stamp-it
   */
  class stamp_it
  {
    template <class T, class MarkedPtr>
    class guard_ptr;

  public:
    template <class T, std::size_t N = 0, class Deleter = std::default_delete<T>>
    class enable_concurrent_ptr;

    struct region_guard
    {
      region_guard() noexcept;
      ~region_guard();

      region_guard(const region_guard&) = delete;
      region_guard(region_guard&&) = delete;
      region_guard& operator=(const region_guard&) = delete;
      region_guard& operator=(region_guard&&) = delete;
    };

    template <class T, std::size_t N = T::number_of_mark_bits>
    using concurrent_ptr = detail::concurrent_ptr<T, N, guard_ptr>;

#ifdef WITH_PERF_COUNTER
    struct performance_counters
    {
      size_t push_calls = 0;
      size_t push_iterations = 0;
      size_t remove_calls = 0;
      size_t remove_next_iterations = 0;
      size_t remove_prev_iterations = 0;
    };
    static performance_counters get_performance_counters();
#endif

    ALLOCATION_TRACKER;
  private:
    static constexpr size_t MarkBits = 18;

    using stamp_t = size_t;

    struct deletable_object_with_stamp;
    struct thread_control_block;
    struct thread_data;

    class thread_order_queue;

    static constexpr stamp_t NotInList = 1;
    static constexpr stamp_t PendingPush = 2;
    static constexpr stamp_t StampInc = 4;

    static thread_data& local_thread_data();
    static thread_order_queue queue;

    ALLOCATION_TRACKING_FUNCTIONS;
  };

  struct stamp_it::deletable_object_with_stamp
  {
    virtual void delete_self() = 0;
    deletable_object_with_stamp* next = nullptr;
    deletable_object_with_stamp* next_chunk = nullptr;
  protected:
    virtual ~deletable_object_with_stamp() = default;
  private:
    stamp_t stamp;
    friend class stamp_it;
  };

  template <class T, std::size_t N, class Deleter>
  class stamp_it::enable_concurrent_ptr :
    private detail::deletable_object_impl<T, Deleter, deletable_object_with_stamp>,
    private detail::tracked_object<stamp_it>
  {
  public:
    static constexpr std::size_t number_of_mark_bits = N;
  protected:
    enable_concurrent_ptr() noexcept = default;
    enable_concurrent_ptr(const enable_concurrent_ptr&) noexcept = default;
    enable_concurrent_ptr(enable_concurrent_ptr&&) noexcept = default;
    enable_concurrent_ptr& operator=(const enable_concurrent_ptr&) noexcept = default;
    enable_concurrent_ptr& operator=(enable_concurrent_ptr&&) noexcept = default;
    ~enable_concurrent_ptr() noexcept = default;
  private:
    friend detail::deletable_object_impl<T, Deleter, deletable_object_with_stamp>;

    template <class, class>
    friend class guard_ptr;
  };

  template <class T, class MarkedPtr>
  class stamp_it::guard_ptr : public detail::guard_ptr<T, MarkedPtr, guard_ptr<T, MarkedPtr>>
  {
    using base = detail::guard_ptr<T, MarkedPtr, guard_ptr>;
    using Deleter = typename T::Deleter;
  public:
    // Guard a marked ptr.
    guard_ptr(const MarkedPtr& p = MarkedPtr()) noexcept;
    explicit guard_ptr(const guard_ptr& p) noexcept;
    guard_ptr(guard_ptr&& p) noexcept;

    guard_ptr& operator=(const guard_ptr& p) noexcept;
    guard_ptr& operator=(guard_ptr&& p) noexcept;

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

#define STAMP_IT_IMPL
#include <xenium/reclamation/impl/stamp_it.hpp>
#undef STAMP_IT_IMPL

#endif
