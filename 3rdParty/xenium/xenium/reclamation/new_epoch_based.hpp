//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_NEW_EPOCH_BASED_HPP
#define XENIUM_NEW_EPOCH_BASED_HPP

#include <xenium/reclamation/detail/concurrent_ptr.hpp>
#include <xenium/reclamation/detail/guard_ptr.hpp>
#include <xenium/reclamation/detail/deletable_object.hpp>
#include <xenium/reclamation/detail/thread_block_list.hpp>
#include <xenium/reclamation/detail/allocation_tracker.hpp>

#include <xenium/acquire_guard.hpp>

namespace xenium { namespace reclamation {

  template <std::size_t UpdateThreshold>
  class new_epoch_based
  {
    template <class T, class MarkedPtr>
    class guard_ptr;

  public:
    template <class T, std::size_t N = 0, class Deleter = std::default_delete<T>>
    class enable_concurrent_ptr;

    struct region_guard
    {
      region_guard() noexcept;
      ~region_guard() noexcept;

      region_guard(const region_guard&) = delete;
      region_guard(region_guard&&) = delete;
      region_guard& operator=(const region_guard&) = delete;
      region_guard& operator=(region_guard&&) = delete;
    };

    template <class T, std::size_t N = T::number_of_mark_bits>
    using concurrent_ptr = detail::concurrent_ptr<T, N, guard_ptr>;

    ALLOCATION_TRACKER;
  private:
    static constexpr unsigned number_epochs = 3;

    struct thread_data;
    struct thread_control_block;

    static std::atomic<unsigned> global_epoch;
    static detail::thread_block_list<thread_control_block> global_thread_block_list;
    static thread_data& local_thread_data();

    ALLOCATION_TRACKING_FUNCTIONS;
  };

  template <std::size_t UpdateThreshold>
  template <class T, std::size_t N, class Deleter>
  class new_epoch_based<UpdateThreshold>::enable_concurrent_ptr :
    private detail::deletable_object_impl<T, Deleter>,
    private detail::tracked_object<new_epoch_based>
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
    friend detail::deletable_object_impl<T, Deleter>;

    template <class, class>
    friend class guard_ptr;
  };

  template <std::size_t UpdateThreshold>
  template <class T, class MarkedPtr>
  class new_epoch_based<UpdateThreshold>::guard_ptr : public detail::guard_ptr<T, MarkedPtr, guard_ptr<T, MarkedPtr>>
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

#define NEW_EPOCH_BASED_IMPL
#include <xenium/reclamation/impl/new_epoch_based.hpp>
#undef NEW_EPOCH_BASED_IMPL

#endif

