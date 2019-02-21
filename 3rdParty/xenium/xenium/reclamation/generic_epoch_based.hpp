//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_GENERIC_EPOCH_BASED_HPP
#define XENIUM_GENERIC_EPOCH_BASED_HPP

#include <xenium/reclamation/detail/concurrent_ptr.hpp>
#include <xenium/reclamation/detail/guard_ptr.hpp>
#include <xenium/reclamation/detail/deletable_object.hpp>
#include <xenium/reclamation/detail/thread_block_list.hpp>
#include <xenium/reclamation/detail/retire_list.hpp>
#include <xenium/reclamation/detail/allocation_tracker.hpp>
#include <xenium/acquire_guard.hpp>
#include <xenium/parameter.hpp>

#include <algorithm>

namespace xenium { namespace reclamation {

  /**
   * @brief This namespace contains various scan strategies to be used
   * in the `generic_epoch_based` reclamation scheme.
   *
   * These strategies define how many threads should be scanned in an
   * attempt to update the global epoch.
   */
  namespace scan {
    struct all_threads;
    struct one_thread;
    template <unsigned N>
    struct n_threads;
  }

  /**
   * @brief This namespace contains various abandonment strategies to be used
   * in the `generic_epoch_based` reclamation scheme.
   *
   * These strategies define when a thread should abandon its retired nodes
   * (i.e., put them on the global list of retired nodes. A thread will always
   * abandon any remaining retired nodes when it terminates.
   *
   * @note The abandon strategy is applied for the retire list of each epoch
   * individually. This is important in case the `when_exceeds_threshold` policy
   * is used.
   */
  namespace abandon {
    struct never;
    struct always;
    template <size_t Threshold>
    struct when_exceeds_threshold;
  }

  /**
   * @brief Defines whether the size of a critical region can be extended using region_guards.
   */
  enum class region_extension {
    /**
     * @brief Critical regions are never extended, i.e., region_guards are effectively
     * no-ops.
     */
    none,

    /**
     * @brief The critical region is entered upon construction of the region_guard and
     * left once the region_guard gets destroyed.
     */
    eager,

    /**
     * @brief A critical region is only entered when a guard_ptr is created. But if this
     * is done inside the scope of a region_guard, the critical region is only left once
     * the region_guard gets destroyed.
     */
    lazy
  };

  namespace policy {
    template <std::size_t V> struct scan_frequency;
    template <class T> struct scan;
    template <class T> struct abandon;
    template <region_extension V> struct region_extension;
  }

  template <
    std::size_t ScanFrequency = 100,
    class ScanStrategy = scan::all_threads,
    class AbandonStrategy = abandon::never,
    region_extension RegionExtension = region_extension::eager
  >
  struct generic_epoch_based_traits {
    static constexpr std::size_t scan_frequency = ScanFrequency;
    template <class Reclaimer>
    using scan_strategy = typename ScanStrategy::template type<Reclaimer>;
    using abandon_strategy = AbandonStrategy;
    static constexpr region_extension region_extension_type = RegionExtension;

    template <class... Policies>
    using with = generic_epoch_based_traits<
      parameter::value_param_t<std::size_t, policy::scan_frequency, ScanFrequency, Policies...>::value,
      parameter::type_param_t<policy::scan, ScanStrategy, Policies...>,
      parameter::type_param_t<policy::abandon, AbandonStrategy, Policies...>,
      parameter::value_param_t<region_extension, policy::region_extension, RegionExtension, Policies...>::value
    >;
  };

  /**
   * @brief A generalized implementation of epoch based reclamation.
   *
   * @tparam Traits
   */
  template <class Traits = generic_epoch_based_traits<>>
  class generic_epoch_based
  {
    template <class T, class MarkedPtr>
    class guard_ptr;

    template <unsigned N>
    friend struct scan::n_threads;
    friend struct scan::all_threads;

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
    using concurrent_ptr = xenium::reclamation::detail::concurrent_ptr<T, N, guard_ptr>;

    ALLOCATION_TRACKER;
  private:
    using epoch_t = size_t;

    static constexpr epoch_t number_epochs = 3;

    struct thread_data;
    struct thread_control_block;

    static std::atomic<epoch_t> global_epoch;
    static detail::thread_block_list<thread_control_block> global_thread_block_list;
    static std::array<detail::orphan_list<>, number_epochs> orphans;
    static thread_data& local_thread_data();

    ALLOCATION_TRACKING_FUNCTIONS;
  };

  template <class Traits>
  template <class T, std::size_t N, class Deleter>
  class generic_epoch_based<Traits>::enable_concurrent_ptr :
    private detail::deletable_object_impl<T, Deleter>,
    private detail::tracked_object<generic_epoch_based>
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

  template <class Traits>
  template <class T, class MarkedPtr>
  class generic_epoch_based<Traits>::guard_ptr : public detail::guard_ptr<T, MarkedPtr, guard_ptr<T, MarkedPtr>>
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

#define GENERIC_EPOCH_BASED_IMPL
#include "impl/generic_epoch_based.hpp"
#undef GENERIC_EPOCH_BASED_IMPL

namespace xenium { namespace reclamation {
  // TODO - rename once old EBR/NEBR/DEBRA implementations have been removed
  template <class... Policies>
  using epoch_based2 = generic_epoch_based<generic_epoch_based_traits<>::with<
    policy::scan_frequency<100>,
    policy::scan<scan::all_threads>,
    policy::region_extension<region_extension::none>,
    Policies...
  >>;

  template <class... Policies>
  using new_epoch_based2 = generic_epoch_based<generic_epoch_based_traits<>::with<
    policy::scan_frequency<100>,
    policy::scan<scan::all_threads>,
    policy::region_extension<region_extension::eager>,
    Policies...
  >>;

  template <class... Policies>
  using debra2 = generic_epoch_based<generic_epoch_based_traits<>::with<
    policy::scan_frequency<20>,
    policy::scan<scan::one_thread>,
    policy::region_extension<region_extension::none>,
    Policies...
  >>;
}}

#endif