//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_RAMALHETE_QUEUE_HPP
#define XENIUM_RAMALHETE_QUEUE_HPP

#include <xenium/acquire_guard.hpp>
#include <xenium/backoff.hpp>
#include <xenium/reclamation/detail/marked_ptr.hpp>
#include <xenium/parameter.hpp>
#include <xenium/policy.hpp>

#include <algorithm>
#include <atomic>
#include <stdexcept>

namespace xenium {

namespace policy {
  /**
   * @brief Policy to configure the number of entries per allocated node in `ramalhete_queue`.
   * @tparam Value
   */
  template <unsigned Value>
  struct entries_per_node;

  /**
   * @brief Policy to configure the number of padding bytes to add to each entry in
   * `ramalhete_queue` to reduce false sharing.
   *
   * Note that this number of bytes is a lower bound. Depending on the size of the
   * queue's `value_type` the compiler may add some additional padding. The effective
   * size of a queue entry is provided in `ramalhete_queue::entry_size`.
   *
   * @tparam Value
   */
  template <unsigned Value>
  struct padding_bytes;

  /**
   * @brief Policy to configure the number of iterations to spin on a queue entry while waiting
   * for a pending push operation to finish.
   * @tparam Value
   */
  template <unsigned Value>
  struct pop_retries;
}

/**
 * @brief A fast unbounded lock-free multi-producer/multi-consumer FIFO queue.
 * 
 * This is an implementation of the `FAAArrayQueue` by Ramalhete and Correia \[[Ram16](index.html#ref-ramalhete-2016)\].
 *
 * It is faster and more efficient than the `michael_scott_queue`, but less generic as it can
 * only handle pointers (i.e., `T` must be a pointer type).
 *
 * A generic version that does not have this limitation is planned for a future version.
 *
 * Supported policies:
 *  * `xenium::policy::reclaimer`<br>
 *    Defines the reclamation scheme to be used for internal nodes. (**required**)
 *  * `xenium::policy::backoff`<br>
 *    Defines the backoff strategy. (*optional*; defaults to `xenium::no_backoff`)
 *  * `xenium::policy::entries_per_node`<br>
 *    Defines the number of entries for each internal node. (*optional*; defaults to 512)
 *  * `xenium::policy::padding slots`<br>
 *    Defines the number of padding slots for each entry. (*optional*; defaults to 1)
 *  * `xenium::policy::pop_retries slots`<br>
 *    Defines the number of iterations to spin on a queue entry while waiting for a pending
 *    push operation to finish. (*optional*; defaults to 10)
 *
 * @tparam T
 * @tparam Policies list of policies to customize the behaviour
 */
template <class T, class... Policies>
class ramalhete_queue {
public:
  static_assert(std::is_pointer<T>::value, "T must be a pointer type");
  using value_type = T;
  using reclaimer = parameter::type_param_t<policy::reclaimer, parameter::nil, Policies...>;
  using backoff = parameter::type_param_t<policy::backoff, no_backoff, Policies...>;
  static constexpr unsigned entries_per_node = parameter::value_param_t<unsigned, policy::entries_per_node, 512, Policies...>::value;
  static constexpr unsigned padding_bytes = parameter::value_param_t<unsigned, policy::padding_bytes, sizeof(T*), Policies...>::value;
  static constexpr unsigned pop_retries = parameter::value_param_t<unsigned, policy::pop_retries, 10, Policies...>::value;;

  static_assert(entries_per_node > 0, "entries_per_node must be greater than zero");
  static_assert(parameter::is_set<reclaimer>::value, "reclaimer policy must be specified");

  template <class... NewPolicies>
  using with = ramalhete_queue<T, NewPolicies..., Policies...>;

  ramalhete_queue();
  ~ramalhete_queue();

  /**
   * Pushes the given value to the queue.
   * This operation might have to allocate a new node.
   * Progress guarantees: lock-free (may perform a memory allocation)
   * @param value
   */
  void push(value_type value);

  /**
   * Tries to pop an object from the queue.
   * Progress guarantees: lock-free
   * @param result
   * @return `true` if the operation was successful, otherwise `false`
   */
  bool try_pop(value_type &result);

private:
  struct node;

  using concurrent_ptr = typename reclaimer::template concurrent_ptr<node, 0>;
  using marked_ptr = typename concurrent_ptr::marked_ptr;
  using guard_ptr = typename concurrent_ptr::guard_ptr;

  using marked_value = reclamation::detail::marked_ptr<std::remove_pointer_t<T>, 1>;

  struct padded_entry {
    std::atomic<marked_value> value;
    // we use max here to avoid arrays of size zero which are not allowed by Visual C++
    char padding[std::max(padding_bytes, 1u)];
  };

  struct unpadded_entry {
    std::atomic<marked_value> value;
  };

  using entry = std::conditional_t<padding_bytes == 0, unpadded_entry, padded_entry>;

public:
  /**
   * @brief Provides the effective size of a single queue entry (including padding).
   */
  static constexpr std::size_t entry_size = sizeof(entry);

private:
  struct node : reclaimer::template enable_concurrent_ptr<node> {
    std::atomic<unsigned>     pop_idx;
    entry entries[entries_per_node];
    std::atomic<unsigned>     push_idx;
    concurrent_ptr next;

    // Start with the first entry pre-filled
    node(value_type item) :
      pop_idx{0},
      push_idx{1},
      next{nullptr}
    {
      entries[0].value.store(item, std::memory_order_relaxed);
      for (unsigned i = 1; i < entries_per_node; i++)
        entries[i].value.store(nullptr, std::memory_order_relaxed);
    }
  };

  alignas(64) concurrent_ptr head;
  alignas(64) concurrent_ptr tail;
};

template <class T, class... Policies>
ramalhete_queue<T, Policies...>::ramalhete_queue()
{
  auto n = new node(nullptr);
  n->push_idx.store(0, std::memory_order_relaxed);
  head.store(n, std::memory_order_relaxed);
  tail.store(n, std::memory_order_relaxed);
}

template <class T, class... Policies>
ramalhete_queue<T, Policies...>::~ramalhete_queue()
{
  // (1) - this acquire-load synchronizes-with the release-CAS (11)
  auto n = head.load(std::memory_order_acquire);
  while (n)
  {
    // (2) - this acquire-load synchronizes-with the release-CAS (4)
    auto next = n->next.load(std::memory_order_acquire);
    delete n.get();
    n = next;
  }
}

template <class T, class... Policies>
void ramalhete_queue<T, Policies...>::push(value_type value)
{
  if (value == nullptr)
    throw std::invalid_argument("value can not be nullptr");

  backoff backoff;

  guard_ptr t;
  for (;;) {
    // (3) - this acquire-load synchronizes-with the release-CAS (5, 7)
    t.acquire(tail, std::memory_order_acquire);

    const int idx = t->push_idx.fetch_add(1, std::memory_order_relaxed);
    if (idx > entries_per_node - 1)
    {
      // This node is full
      if (t != tail.load(std::memory_order_relaxed))
        continue; // some other thread already added a new node.

      auto next = t->next.load(std::memory_order_relaxed);
      if (next == nullptr)
      {
        node* new_node = new node(value);
        marked_ptr expected = nullptr;
        // (4) - this release-CAS synchronizes-with the acquire-load (2, 6, 10)
        if (t->next.compare_exchange_strong(expected, new_node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
        {
          expected = t;
          // (5) - this release-CAS synchronizes-with the acquire-load (3)
          tail.compare_exchange_strong(expected, new_node, std::memory_order_release, std::memory_order_relaxed);
          return;
        }
        // some other node already added a new node
        delete new_node;
      } else {
        // (6) - this acquire-load synchronizes-with the release-CAS (4)
        next = t->next.load(std::memory_order_acquire);
        marked_ptr expected = t;
        // (7) - this release-CAS synchronizes-with the acquire-load (3)
        tail.compare_exchange_strong(expected, next, std::memory_order_release, std::memory_order_relaxed);
      }
      continue;
    }

    marked_value expected = nullptr;
    // (8) - this release-CAS synchronizes-with the acquire-exchange (12)
    if (t->entries[idx].value.compare_exchange_strong(expected, value, std::memory_order_release, std::memory_order_relaxed))
      return;

    backoff();
  }
}

template <class T, class... Policies>
bool ramalhete_queue<T, Policies...>::try_pop(value_type &result)
{
  backoff backoff;

  guard_ptr h;
  for (;;) {
    // (9) - this acquire-load synchronizes-with the release-CAS (11)
    h.acquire(head, std::memory_order_acquire);

    if (h->pop_idx.load(std::memory_order_relaxed) >= h->push_idx.load(std::memory_order_relaxed) &&
        h->next.load(std::memory_order_relaxed) == nullptr)
      break;

    const int idx = h->pop_idx.fetch_add(1, std::memory_order_relaxed);
    if (idx > entries_per_node - 1)
    {
      // This node has been drained, check if there is another one
      // (10) - this acquire-load synchronizes-with the release-CAS (4)
      auto next = h->next.load(std::memory_order_acquire);
      if (next == nullptr)
        break;  // No more nodes in the queue

      marked_ptr expected = h;
      // (11) - this release-CAS synchronizes-with the acquire-load (1, 9)
      if (head.compare_exchange_strong(expected, next, std::memory_order_release, std::memory_order_relaxed))
        h.reclaim(); // The old node has been unlinked -> reclaim it.

      continue;
    }

    if (pop_retries > 0) {
      unsigned cnt = 0;
      ramalhete_queue::backoff backoff;
      while (h->entries[idx].value.load(std::memory_order_relaxed) == nullptr && ++cnt <= pop_retries)
        backoff(); // TODO - use a backoff tpye that can be configured separately
    }

    // (12) - this acquire-exchange synchronizes-with the release-CAS (8)
    auto value = h->entries[idx].value.exchange(marked_value(nullptr, 1), std::memory_order_acquire);
    if (value != nullptr) {
      result = value.get();
      return true;
    }

    backoff();
  }

  return false;
}
}

#endif
