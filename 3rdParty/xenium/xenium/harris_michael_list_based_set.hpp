//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_HARRIS_MICHAEL_LIST_BASED_SET_HPP
#define XENIUM_HARRIS_MICHAEL_LIST_BASED_SET_HPP

#include <xenium/acquire_guard.hpp>
#include <xenium/backoff.hpp>
#include <xenium/parameter.hpp>
#include <xenium/policy.hpp>

#include <functional>

namespace xenium {
/**
 * @brief A lock-free container that contains a sorted set of unique objects of type `Key`.
 *
 * This container is implemented as a sorted singly linked list. All operations have
 * a runtime complexity linear in the size of the list (in the absence of conflicting
 * operations).
 * 
 * This data structure is based on the solution proposed by Michael \[[Mic02](index.html#ref-michael-2002)\]
 * which builds upon the original proposal by Harris \[[Har01](index.html#ref-harris-2001)\].
 *
 * * Supported policies:
 *  * `xenium::policy::reclaimer`<br>
 *    Defines the reclamation scheme to be used for internal nodes. (**required**)
 *  * `xenium::policy::compare`<br>
 *    Defines the comparison function that is used to order the list. (*optional*; defaults to `std::less<Key>`)
 *  * `xenium::policy::backoff`<br>
 *    Defines the backoff strategy. (*optional*; defaults to `xenium::no_backoff`)
 *
 * @tparam Key type of the stored elements.
 * @tparam Policies list of policies to customize the behaviour
 */
template <class Key, class... Policies>
class harris_michael_list_based_set
{
public:
  using value_type = Key;
  using reclaimer = parameter::type_param_t<policy::reclaimer, parameter::nil, Policies...>;
  using backoff = parameter::type_param_t<policy::backoff, no_backoff, Policies...>;
  using compare = parameter::type_param_t<policy::compare, std::less<Key>, Policies...>;

  template <class... NewPolicies>
  using with = harris_michael_list_based_set<Key, NewPolicies..., Policies...>;

  static_assert(parameter::is_set<reclaimer>::value, "reclaimer policy must be specified");

  harris_michael_list_based_set() = default;
  ~harris_michael_list_based_set();

  class iterator;

  /**
   * @brief Inserts a new element into the container if the container doesn't already contain an
   * element with an equivalent key. The element is constructed in-place with the given `args`.
   *
   * The element is always constructed. If there already is an element with the key in the container,
   * the newly constructed element will be destroyed immediately.
   *
   * No iterators or references are invalidated.
   * 
   * Progress guarantees: lock-free
   *
   * @param args arguments to forward to the constructor of the element
   * @return `true` if an element was inserted, otherwise `false`
   */
  template <class... Args>
  bool emplace(Args&&... args);

  /**
   * @brief Inserts a new element into the container if the container doesn't already contain an
   * element with an equivalent key. The element is constructed in-place with the given `args`.
   *
   * The element is always constructed. If there already is an element with the key in the container,
   * the newly constructed element will be destroyed immediately.
   *
   * No iterators or references are invalidated.
   * 
   * Progress guarantees: lock-free
   * 
   * @param args arguments to forward to the constructor of the element
   * @return a pair consisting of an iterator to the inserted element, or the already-existing element
   * if no insertion happened, and a bool denoting whether the insertion took place;
   * `true` if an element was inserted, otherwise `false`
   */
  template <class... Args>
  std::pair<iterator, bool> emplace_or_get(Args&&... args);

  /**
   * @brief Removes the element with the key equivalent to key (if one exists).
   *
   * No iterators or references are invalidated.
   * 
   * Progress guarantees: lock-free
   * 
   * @param key key of the element to remove
   * @return `true` if an element was removed, otherwise `false`
   */
  bool erase(const Key& key);

  /**
   * @brief Removes the specified element from the container.
   *
   * No iterators or references are invalidated.
   * 
   * Progress guarantees: lock-free
   * 
   * @param pos the iterator identifying the element to remove
   * @return iterator following the last removed element
   */
  iterator erase(iterator pos);

  /**
   * @brief Finds an element with key equivalent to key.
   * 
   * Progress guarantees: lock-free
   * 
   * @param key key of the element to search for
   * @return iterator to an element with key equivalent to key if such element is found,
   * otherwise past-the-end iterator
   */
  iterator find(const Key& key);

  /**
   * @brief Checks if there is an element with key equivalent to key in the container.
   *
   * Progress guarantees: lock-free
   * 
   * @param key key of the element to search for
   * @return `true` if there is such an element, otherwise `false`
   */
  bool contains(const Key& key);

  /**
   * @brief Returns an iterator to the first element of the container. 
   * @return iterator to the first element 
   */
  iterator begin();

  /**
   * @brief Returns an iterator to the element following the last element of the container.
   * 
   * This element acts as a placeholder; attempting to access it results in undefined behavior. 
   * @return iterator to the element following the last element.
   */
  iterator end();

private:
  struct node;

  using concurrent_ptr = typename reclaimer::template concurrent_ptr<node, 1>;
  using marked_ptr = typename concurrent_ptr::marked_ptr;
  using guard_ptr = typename concurrent_ptr::guard_ptr;
  
  struct find_info
  {
    concurrent_ptr* prev;
    marked_ptr next;
    guard_ptr cur;
    guard_ptr save;
  };
  bool find(const Key& key, find_info& info, backoff& backoff);

  concurrent_ptr head;
};

/**
 * @brief A ForwardIterator to safely iterate the list.
 * 
 * Iterators are not invalidated by concurrent insert/erase operations. However, conflicting erase
 * operations can have a negative impact on the performance when advancing the iterator, because it
 * may be necessary to rescan the list to find the next element.
 * 
 * *Note:* This iterator class does *not* provide multi-pass guarantee as `a == b` does not imply `++a == ++b`.
 * 
 * *Note:* Each iterator internally holds two `guard_ptr` instances. This has to be considered when using
 * a reclamation scheme that requires per-instance resources like `hazard_pointer` or `hazard_eras`.
 * It is therefore highly recommended to use prefix increments wherever possible.
 */
template <class Key, class... Policies>
class harris_michael_list_based_set<Key, Policies...>::iterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = Key;
  using difference_type = std::ptrdiff_t;
  using pointer = const Key*;
  using reference = const Key&;

  iterator(iterator&&) = default;
  iterator(const iterator&) = default;

  iterator& operator=(iterator&&) = default;
  iterator& operator=(const iterator&) = default;

  /**
   * @brief Moves the iterator to the next element.
   * In the absence of conflicting operations, this operation has constant runtime complexity.
   * However, in case of conflicting erase operations we might have to rescan the list to help
   * remove the node and find the next element.
   *
   * Progress guarantess: lock-free
   */
  iterator& operator++();
  iterator operator++(int);

  bool operator==(const iterator& other) const { return info.cur.get() == other.info.cur.get(); }
  bool operator!=(const iterator& other) const { return !(*this == other); }
  reference operator*() const noexcept { return info.cur->key; }
  pointer operator->() const noexcept { return &info.cur->key; }

  /**
   * @brief Resets the iterator; this is equivalent to assigning to `end()` it.
   * This operation can be handy in situations where an iterator is no longer needed and you want
   * to ensure that the internal `guard_ptr` instances are reset.
   */
  void reset() {
    info.cur.reset();
    info.save.reset();
  }
private:
  friend harris_michael_list_based_set;

  explicit iterator(harris_michael_list_based_set& list, concurrent_ptr* start) : list(&list)
  {
    info.prev = start;
    if (start) {
      // (2) - this acquire-load synchronizes-with the release-CAS (7, 8, 10, 12)
      info.cur.acquire(*start, std::memory_order_acquire);
    }
  }

  explicit iterator(harris_michael_list_based_set& list, find_info&& info) :
    list(&list),
    info(std::move(info))
  {}

  harris_michael_list_based_set* list;
  find_info info;
};

template <class Key, class... Policies>
struct harris_michael_list_based_set<Key, Policies...>::node : reclaimer::template enable_concurrent_ptr<node, 1>
{
  const Key key;
  concurrent_ptr next;
  template< class... Args >
  node(Args&&... args) : key(std::forward<Args>(args)...), next() {}
};

template <class Key, class... Policies>
auto harris_michael_list_based_set<Key, Policies...>::iterator::operator++() -> iterator&
{
  assert(info.cur.get() != nullptr);
  auto next = info.cur->next.load(std::memory_order_relaxed);
  guard_ptr tmp_guard;
  // (1) - this acquire-load synchronizes-with the release-CAS (7, 8, 10, 12)
  if (next.mark() == 0 && tmp_guard.acquire_if_equal(info.cur->next, next, std::memory_order_acquire))
  {
    info.prev = &info.cur->next;
    info.save = std::move(info.cur);
    info.cur = std::move(tmp_guard);
  }
  else
  {
    // cur is marked for removal
    // -> use find to remove it and get to the next node with a compare(key, cur->key) == false
    auto key = info.cur->key;
    backoff backoff;
    list->find(key, info, backoff);
  }
  assert(info.prev == &list->head ||
          info.cur.get() == nullptr ||
          (info.save.get() != nullptr && &info.save->next == info.prev));
  return *this;
}

template <class Key, class... Policies>
auto harris_michael_list_based_set<Key, Policies...>::iterator::operator++(int) -> iterator
{
  iterator retval = *this;
  ++(*this);
  return retval;
}

template <class Key, class... Policies>
harris_michael_list_based_set<Key, Policies...>::~harris_michael_list_based_set()
{
  // delete all remaining nodes
  // (3) - this acquire-load synchronizes-with the release-CAS (7, 8, 10, 12)
  auto p = head.load(std::memory_order_acquire);
  while (p)
  {
    // (4) - this acquire-load synchronizes-with the release-CAS (7, 8, 10, 12)
    auto next = p->next.load(std::memory_order_acquire);
    delete p.get();
    p = next;
  }
}

template <class Key, class... Policies>
bool harris_michael_list_based_set<Key, Policies...>::find(const Key& key, find_info& info, backoff& backoff)
{
  assert((info.save == nullptr && info.prev == &head) || &info.save->next == info.prev);
  concurrent_ptr* start = info.prev;
  guard_ptr start_guard = info.save; // we have to keep a guard_ptr to prevent start's node from getting reclaimed.
retry:
  info.prev = start;
  info.save = start_guard;
  info.next = info.prev->load(std::memory_order_relaxed);
  if (info.next.mark() != 0) {
    // our start node is marked for removal -> we have to restart from head
    start = &head;
    start_guard.reset();
    goto retry;
  }

  for (;;)
  {
    // (5) - this acquire-load synchronizes-with the release-CAS (7, 8, 10, 12)
    if (!info.cur.acquire_if_equal(*info.prev, info.next, std::memory_order_acquire))
      goto retry;

    if (!info.cur)
      return false;

    info.next = info.cur->next.load(std::memory_order_relaxed);
    if (info.next.mark() != 0)
    {
      // Node *cur is marked for deletion -> update the link and retire the element

      // (6) - this acquire-load synchronizes-with the release-CAS (7, 8, 10, 12)
      info.next = info.cur->next.load(std::memory_order_acquire).get();

      // Try to splice out node
      marked_ptr expected = info.cur.get();
      // (7) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6)
      //       and the require-CAS (9, 11)
      //       it is the head of a potential release sequence containing (9, 11)
      if (!info.prev->compare_exchange_weak(expected, info.next,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
      {
        backoff();
        goto retry;
      }
      info.cur.reclaim();
    }
    else
    {
      if (info.prev->load(std::memory_order_relaxed) != info.cur.get())
        goto retry; // cur might be cut from list.

      Key ckey = info.cur->key;
      compare compare;
      if (compare(ckey, key) == false)
        return compare(key, ckey) == false;

      info.prev = &info.cur->next;
      std::swap(info.save, info.cur);
    }
  }
}

template <class Key, class... Policies>
bool harris_michael_list_based_set<Key, Policies...>::contains(const Key& key)
{
  find_info info{&head};
  backoff backoff;
  return find(key, info, backoff);
}

template <class Key, class... Policies>
auto harris_michael_list_based_set<Key, Policies...>::find(const Key& key) -> iterator
{
  find_info info{&head};
  backoff backoff;
  if (find(key, info, backoff))
    return iterator(*this, std::move(info));
  return end();
}

template <class Key, class... Policies>
template <class... Args>
bool harris_michael_list_based_set<Key, Policies...>::emplace(Args&&... args)
{
  auto result = emplace_or_get(std::forward<Args>(args)...);
  return result.second;
}

template <class Key, class... Policies>
template <class... Args>
auto harris_michael_list_based_set<Key, Policies...>::emplace_or_get(Args&&... args) -> std::pair<iterator, bool>
{
  node* n = new node(std::forward<Args>(args)...);
  find_info info{&head};
  backoff backoff;
  for (;;)
  {
    if (find(n->key, info, backoff))
    {
      delete n;
      return {iterator(*this, std::move(info)), false};
    }
    // Try to install new node
    marked_ptr cur = info.cur.get();
    info.cur.reset();
    info.cur = guard_ptr(n);
    n->next.store(cur, std::memory_order_relaxed);

    // (8) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6)
    //       and the acquire-CAS (9, 11)
    //       it is the head of a potential release sequence containing (9, 11)
    if (info.prev->compare_exchange_weak(cur, n,
                                         std::memory_order_release,
                                         std::memory_order_relaxed))
      return {iterator(*this, std::move(info)), true};

    backoff();
  }
}

template <class Key, class... Policies>
bool harris_michael_list_based_set<Key, Policies...>::erase(const Key& key)
{
  backoff backoff;
  find_info info{&head};
  // Find node in list with matching key and mark it for reclamation.
  for (;;)
  {
    if (!find(key, info, backoff))
      return false; // No such node in the list

    // (9) - this acquire-CAS synchronizes with the release-CAS (7, 8, 10, 12)
    //       and is part of a release sequence headed by those operations
    if (info.cur->next.compare_exchange_weak(info.next,
                                             marked_ptr(info.next.get(), 1),
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed))
      break;

    backoff();
  }

  assert(info.next.mark() == 0);
  assert(info.cur.mark() == 0);

  // Try to splice out node
  marked_ptr expected = info.cur;
  // (10) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6)
  //        and the acquire-CAS (9, 11)
  //        it is the head of a potential release sequence containing (9, 11)
  if (info.prev->compare_exchange_weak(expected, info.next,
                                       std::memory_order_release,
                                       std::memory_order_relaxed))
    info.cur.reclaim();
  else
    // Another thread interfered -> rewalk the list to ensure reclamation of marked node before returning.
    find(key, info, backoff);

  return true;
}

template <class Key, class... Policies>
auto harris_michael_list_based_set<Key, Policies...>::erase(iterator pos) -> iterator
{
  backoff backoff;
  auto next = pos.info.cur->next.load(std::memory_order_relaxed);
  while (next.mark() == 0)
  {
    // (11) - this acquire-CAS synchronizes-with the release-CAS (7, 8, 10, 12)
    //        and is part of a release sequence headed by those operations
    if (pos.info.cur->next.compare_exchange_weak(next,
                                            marked_ptr(next.get(), 1),
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed))
      break;

    backoff();
  }

  guard_ptr next_guard(next.get());
  assert(pos.info.cur.mark() == 0);

  // Try to splice out node
  marked_ptr expected = pos.info.cur;
  // (12) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6)
  //        and the acquire-CAS (9, 11)
  //        it is the head of a potential release sequence containing (9, 11)
  if (pos.info.prev->compare_exchange_weak(expected, next_guard,
                                      std::memory_order_release,
                                      std::memory_order_relaxed)) {
    pos.info.cur.reclaim();
    pos.info.cur = std::move(next_guard);
  } else {
    next_guard.reset();
    Key key = pos.info.cur->key;

    // Another thread interfered -> rewalk the list to ensure reclamation of marked node before returning.
    find(key, pos.info, backoff);
  }

  return pos;
}

template <class Key, class... Policies>
auto harris_michael_list_based_set<Key, Policies...>::begin() -> iterator
{
  return iterator(*this, &head);
}

template <class Key, class... Policies>
auto harris_michael_list_based_set<Key, Policies...>::end() -> iterator
{
  return iterator(*this, nullptr);
}
}

#endif
