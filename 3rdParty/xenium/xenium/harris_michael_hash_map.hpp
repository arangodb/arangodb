//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_HARRIS_MICHAEL_HASH_MAP_HPP
#define XENIUM_HARRIS_MICHAEL_HASH_MAP_HPP

#include <xenium/acquire_guard.hpp>
#include <xenium/backoff.hpp>
#include <xenium/hash.hpp>
#include <xenium/parameter.hpp>
#include <xenium/policy.hpp>
#include <xenium/utils.hpp>

#include <atomic>

namespace xenium {

namespace policy {
  /**
   * @brief Policy to configure the number of buckets in `harris_michael_hash_map`.
   * @tparam Value
   */
  template <std::size_t Value>
  struct buckets;

  /**
   * @brief Policy to configure the function that maps the hash value to a bucket
   * in `harris_michael_hash_map`.
   *
   * This can be used to apply other multiplicative methods like fibonacci hashing.
   *
   * @tparam T
   */
  template <class T>
  struct map_to_bucket;

  /**
   * @brief Policy to configure whether the hash value should be stored and used
   * during lookup operations in `harris_michael_hash_map`.
   *
   * This can improve performance for complex types with expensive compare operations
   * like strings.
   *
   * @tparam T
   */
  template <bool Value>
  struct memoize_hash;
}

/**
 * @brief A generic lock-free hash-map.
 *
 * This hash-map consists of a fixed number of buckets were each bucket is essentially
 * a `harris_michael_list_based_set` instance. The number of buckets is fixed, so the
 * hash-map does not support dynamic resizing.
 * 
 * This hash-map is less efficient than many other available concurrent hash-maps, but it is
 * lock-free and fully generic, i.e., it supports arbitrary types for `Key` and `Value`.
 *
 * This data structure is based on the solution proposed by Michael \[[Mic02](index.html#ref-michael-2002)\]
 * which builds upon the original proposal by Harris \[[Har01](index.html#ref-harris-2001)\].
 *
 * Supported policies:
 *  * `xenium::policy::reclaimer`<br>
 *    Defines the reclamation scheme to be used for internal nodes. (**required**)
 *  * `xenium::policy::hash`<br>
 *    Defines the hash function. (*optional*; defaults to `xenium::hash<Key>`)
 *  * `xenium::policy::map_to_bucket`<br>
 *    Defines the function that is used to map the calculated hash to a bucket.
 *    (*optional*; defaults to `xenium::utils::modulo<std::size_t>`)
 *  * `xenium::policy::backoff`<br>
 *    Defines the backoff strategy. (*optional*; defaults to `xenium::no_backoff`)
 *  * `xenium::policy::buckets`<br>
 *    Defines the number of buckets. (*optional*; defaults to 512)
 *  * `xenium::policy::memoize_hash`<br>
 *    Defines whether the hash should be stored and used during lookup operations.
 *    (*optional*; defaults to false for scalar `Key` types; otherwise true)
 *
 * @tparam Key
 * @tparam Value
 * @tparam Policies list of policies to customize the behaviour
 */
template <class Key, class Value, class... Policies>
class harris_michael_hash_map
{
public:
  using value_type = std::pair<const Key, Value>;
  using reclaimer = parameter::type_param_t<policy::reclaimer, parameter::nil, Policies...>;
  using hash = parameter::type_param_t<policy::hash, xenium::hash<Key>, Policies...>;
  using map_to_bucket = parameter::type_param_t<policy::map_to_bucket, utils::modulo<std::size_t>, Policies...>;
  using backoff = parameter::type_param_t<policy::backoff, no_backoff, Policies...>;
  static constexpr std::size_t num_buckets = parameter::value_param_t<std::size_t, policy::buckets, 512, Policies...>::value;
  static constexpr bool memoize_hash =
    parameter::value_param_t<bool, policy::memoize_hash, !std::is_scalar<Key>::value, Policies...>::value;

  template <class... NewPolicies>
  using with = harris_michael_hash_map<Key, Value, NewPolicies..., Policies...>;

  static_assert(parameter::is_set<reclaimer>::value, "reclaimer policy must be specified");

  class iterator;
  class accessor;

  harris_michael_hash_map() = default;
  ~harris_michael_hash_map();

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
   * @brief Inserts a new element into the container if the container doesn't already contain an
   * element with an equivalent key. The element is constructed as `value_type(std::piecewise_construct,
   * std::forward_as_tuple(k), std::forward_as_tuple(std::forward<Args>(args)...))`.
   *
   * The element may be constructed even if there already is an element with the key in the container,
   * in which case the newly constructed element will be destroyed immediately.
   *
   * No iterators or references are invalidated.
   * Progress guarantees: lock-free
   *
   * @param key the key of element to be inserted.
   * @param args arguments to forward to the constructor of the element
   * @return a pair consisting of an iterator to the inserted element, or the already-existing element
   * if no insertion happened, and a bool denoting whether the insertion took place;
   * `true` if an element was inserted, otherwise `false`
   */
  template <class... Args>
  std::pair<iterator, bool> get_or_emplace(Key key, Args&&... args);

  /**
   * @brief Inserts a new element into the container if the container doesn't already contain an
   * element with an equivalent key. The value for the newly constructed element is created by
   * calling `value_factory`.
   *
   * The element may be constructed even if there already is an element with the key in the container,
   * in which case the newly constructed element will be destroyed immediately.
   *
   * No iterators or references are invalidated.
   * Progress guarantees: lock-free
   *
   * @tparam Func
   * @param key the key of element to be inserted.
   * @param factory a functor that is used to create the `Value` instance when constructing
   * the new element to be inserted.
   * @return a pair consisting of an iterator to the inserted element, or the already-existing element
   * if no insertion happened, and a bool denoting whether the insertion took place;
   * `true` if an element was inserted, otherwise `false`
   */
  template <class Factory>
  std::pair<iterator, bool> get_or_emplace_lazy(Key key, Factory factory);

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
   * @brief
   *
   * The `accessor`
   * @param key
   * @return an accessor to the element's value
   */
  accessor operator[](const Key& key);

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
  using hash_t = std::size_t;
  using concurrent_ptr = typename reclaimer::template concurrent_ptr<node, 1>;
  using marked_ptr = typename concurrent_ptr::marked_ptr;
  using guard_ptr = typename concurrent_ptr::guard_ptr;

  template <typename Factory>
  std::pair<iterator, bool> do_get_or_emplace_lazy(Key key, Factory node_factory);

  struct construct_without_hash {};

  struct data_without_hash
  {
    value_type value;
    template <class... Args>
    data_without_hash(hash_t /*hash*/, Args&&... args) :
      value(std::forward<Args>(args)...)
    {}
    template <class... Args>
    data_without_hash(construct_without_hash, Args&&... args) :
      value(std::forward<Args>(args)...)
    {}
    hash_t get_hash() const { return hash{}(value.first); }
    bool greater_or_equal(hash_t /*h*/, const Key& key) const { return value.first >= key; }
  };

  struct data_with_hash
  {
    hash_t hash;
    value_type value;
    template <class... Args>
    data_with_hash(hash_t hash, Args&&... args) :
      hash(hash), value(std::forward<Args>(args)...)
    {}
    template <class... Args>
    data_with_hash(construct_without_hash, Args&&... args) :
      value(std::forward<Args>(args)...)
    {
      hash = harris_michael_hash_map::hash{}(value.first);
    }
    hash_t get_hash() const { return hash; }
    bool greater_or_equal(hash_t h, const Key& key) const { return hash >= h && value.first >= key; }
  };

  using data_t = std::conditional_t<memoize_hash, data_with_hash, data_without_hash>;


  struct node : reclaimer::template enable_concurrent_ptr<node, 1>
  {
    data_t data;
    concurrent_ptr next;
    template <class... Args>
    node(Args&&... args) :
      data(std::forward<Args>(args)...), next()
    {}
  };

  struct find_info
  {
    concurrent_ptr* prev;
    marked_ptr next;
    guard_ptr cur;
    guard_ptr save;
  };

  bool find(hash_t hash, const Key& key, std::size_t bucket, find_info& info, backoff& backoff);

  concurrent_ptr buckets[num_buckets];
};

/**
 * @brief A ForwardIterator to safely iterate the hash-map.
 * 
 * Iterators are not invalidated by concurrent insert/erase operations. However, conflicting erase
 * operations can have a negative impact on the performance when advancing the iterator, because it
 * may be necessary to rescan the bucket's list to find the next element.
 * 
 * *Note:* This iterator class does *not* provide multi-pass guarantee as `a == b` does not imply `++a == ++b`.
 * 
 * *Note:* Each iterator internally holds two `guard_ptr` instances. This has to be considered when using
 * a reclamation scheme that requires per-instance resources like `hazard_pointer` or `hazard_eras`.
 * It is therefore highly recommended to use prefix increments wherever possible.
 */
template <class Key, class Value, class... Policies>
class harris_michael_hash_map<Key, Value, Policies...>::iterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = harris_michael_hash_map::value_type;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;

  iterator(iterator&&) = default;
  iterator(const iterator&) = default;

  iterator& operator=(iterator&&) = default;
  iterator& operator=(const iterator&) = default;

  iterator& operator++()
  {
    assert(info.cur.get() != nullptr);
    auto next = info.cur->next.load(std::memory_order_relaxed);
    guard_ptr tmp_guard;
    // (1) - this acquire-load synchronizes-with the release-CAS (8, 9, 10, 12, 14)
    if (next.mark() == 0 && tmp_guard.acquire_if_equal(info.cur->next, next, std::memory_order_acquire))
    {
      info.prev = &info.cur->next;
      info.save = std::move(info.cur);
      info.cur = std::move(tmp_guard);
    }
    else
    {
      // cur is marked for removal
      // -> use find to remove it and get to the next node with a key >= cur->key
      // Note: we have to copy key here!
      Key key = info.cur->data.value.first;
      hash_t h = info.cur->data.get_hash();
      backoff backoff;
      map->find(h, key, bucket, info, backoff);
    }
    assert(info.prev == &map->buckets[bucket] ||
           info.cur.get() == nullptr ||
           (info.save.get() != nullptr && &info.save->next == info.prev));

    if (!info.cur)
      move_to_next_bucket();

    return *this;
  }
  iterator operator++(int)
  {
    iterator retval = *this;
    ++(*this);
    return retval;
  }
  bool operator==(const iterator& other) const { return info.cur.get() == other.info.cur.get(); }
  bool operator!=(const iterator& other) const { return !(*this == other); }
  reference operator*() const noexcept { return info.cur->data.value; }
  pointer operator->() const noexcept { return &info.cur->data.value; }

  void reset() {
    bucket = num_buckets;
    info.prev = nullptr;
    info.cur.reset();
    info.save.reset();
  }

private:
  friend harris_michael_hash_map;

  explicit iterator(harris_michael_hash_map* map) :
    map(map),
    bucket(num_buckets)
  {}

  explicit iterator(harris_michael_hash_map* map, std::size_t bucket) :
    map(map),
    bucket(bucket)
  {
    info.prev = &map->buckets[bucket];
    // (2) - this acquire-load synchronizes-with the release-CAS (8, 9, 10, 12, 14)
    info.cur.acquire(*info.prev, std::memory_order_acquire);

    if (!info.cur)
      move_to_next_bucket();
  }

  explicit iterator(harris_michael_hash_map* map, std::size_t bucket, find_info&& info) :
    map(map),
    bucket(bucket),
    info(std::move(info))
  {}

  void move_to_next_bucket() {
    info.save.reset();
    while (!info.cur && bucket < num_buckets - 1) {
      ++bucket;
      info.prev = &map->buckets[bucket];
      // (3) - this acquire-load synchronizes-with the release-CAS (8, 9, 10, 12, 14)
      info.cur.acquire(*info.prev, std::memory_order_acquire);
    }
  }

  harris_michael_hash_map* map;
  std::size_t bucket;
  find_info info;
};

/**
 * @brief An accessor to safely access the value of an element.
 *
 * It is used as result type of `operator[]` to provide a similar behaviour as the
 * STL `map`/`unordered_map` classes. However, the accessor has to be dereferenced to
 * access the underlying value (`operator*`).
 *
 * Note: an `accessor` instance internally holds a `guard_ptr` to the value's element.
 */
template <class Key, class Value, class... Policies>
class harris_michael_hash_map<Key, Value, Policies...>::accessor {
public:
  Value* operator->() const noexcept { return &guard->data.value.second; }
  Value& operator*() const noexcept { return guard->data.value.second; }
  void reset() { guard.reset(); }
private:
  accessor(guard_ptr&& guard): guard(std::move(guard)) {}
  guard_ptr guard;
  friend harris_michael_hash_map;
};

template <class Key, class Value, class... Policies>
harris_michael_hash_map<Key, Value, Policies...>::~harris_michael_hash_map()
{
  for (std::size_t i = 0; i < num_buckets; ++i)
  {
    // (4) - this acquire-load synchronizes-with the release-CAS (8, 9, 10, 12, 14)
    auto p = buckets[i].load(std::memory_order_acquire);
    while (p)
    {
      // (5) - this acquire-load synchronizes-with the release-CAS (8, 9, 10, 12, 14)
      auto next = p->next.load(std::memory_order_acquire);
      delete p.get();
      p = next;
    }
  }
}

template <class Key, class Value, class... Policies>
bool harris_michael_hash_map<Key, Value, Policies...>::find(hash_t hash, const Key& key, std::size_t bucket,
  find_info& info, backoff& backoff)
{
  auto& head = buckets[bucket];
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
    // (6) - this acquire-load synchronizes-with the release-CAS (8, 9, 10, 12, 14)
    if (!info.cur.acquire_if_equal(*info.prev, info.next, std::memory_order_acquire))
      goto retry;

    if (!info.cur)
      return false;

    info.next = info.cur->next.load(std::memory_order_relaxed);
    if (info.next.mark() != 0)
    {
      // Node *cur is marked for deletion -> update the link and retire the element

      // (7) - this acquire-load synchronizes-with the release-CAS (8, 9, 10, 12, 14)
      info.next = info.cur->next.load(std::memory_order_acquire).get();

      // Try to splice out node
      marked_ptr expected = info.cur.get();
      // (8) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6, 7)
      //       and the acquire-CAS (11, 13)
      //       it is the head of a potential release sequence containing (11, 13)
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
        goto retry; // cur might be cut from the hash_map.

      const auto& data = info.cur->data;
      if (data.greater_or_equal(hash, key))
        return data.value.first == key;

      info.prev = &info.cur->next;
      std::swap(info.save, info.cur);
    }
  }
}

template <class Key, class Value, class... Policies>
bool harris_michael_hash_map<Key, Value, Policies...>::contains(const Key& key)
{
  auto h = hash{}(key);
  auto bucket = map_to_bucket{}(h, num_buckets);
  find_info info{&buckets[bucket]};
  backoff backoff;
  return find(h, key, bucket, info, backoff);
}

template <class Key, class Value, class... Policies>
auto harris_michael_hash_map<Key, Value, Policies...>::find(const Key& key) -> iterator
{
  auto h = hash{}(key);
  auto bucket = map_to_bucket{}(h, num_buckets);
  find_info info{&buckets[bucket]};
  backoff backoff;
  if (find(h, key, bucket, info, backoff))
    return iterator(this, bucket, std::move(info));
  return end();
}

template <class Key, class Value, class... Policies>
template <class... Args>
bool harris_michael_hash_map<Key, Value, Policies...>::emplace(Args&&... args)
{
  auto result = emplace_or_get(std::forward<Args>(args)...);
  return result.second;
}

template <class Key, class Value, class... Policies>
template <class... Args>
auto harris_michael_hash_map<Key, Value, Policies...>::get_or_emplace(Key key, Args&&... args)
-> std::pair<iterator, bool>
{
  return do_get_or_emplace_lazy(std::move(key), [&args...](hash_t hash, Key key) {
    return new node(hash, std::piecewise_construct,
                    std::forward_as_tuple(std::move(key)),
                    std::forward_as_tuple(std::forward<Args>(args)...));
  });
}

template <class Key, class Value, class... Policies>
template <typename Factory>
auto harris_michael_hash_map<Key, Value, Policies...>::get_or_emplace_lazy(Key key, Factory value_factory)
  -> std::pair<iterator, bool>
{
  return do_get_or_emplace_lazy(std::move(key), [&value_factory](hash_t hash, Key key) {
    return new node(hash, std::move(key), value_factory());
  });
}

template <class Key, class Value, class... Policies>
template <typename Factory>
auto harris_michael_hash_map<Key, Value, Policies...>::do_get_or_emplace_lazy(Key key, Factory node_factory)
  -> std::pair<iterator, bool>
{
  node* n = nullptr;
  auto h = hash{}(key);
  auto bucket = map_to_bucket{}(h, num_buckets);

  const Key* pkey = &key;
  find_info info{&buckets[bucket]};
  backoff backoff;
  for (;;)
  {
    if (find(h, *pkey, bucket, info, backoff))
    {
      delete n;
      return {iterator(this, bucket, std::move(info)), false};
    }
    if (n == nullptr) {
      n = node_factory(h, std::move(key));
      pkey = &n->data.value.first;
    }

    // Try to install new node
    marked_ptr cur = info.cur.get();
    info.cur.reset();
    info.cur = guard_ptr(n);
    n->next.store(cur, std::memory_order_relaxed);

    // (9) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6, 7)
    //       and the acquire-CAS (11, 13)
    //       it is the head of a potential release sequence containing (11, 13)
    if (info.prev->compare_exchange_weak(cur, n,
                                         std::memory_order_release,
                                         std::memory_order_relaxed))
      return {iterator(this, bucket, std::move(info)), true};

    backoff();
  }
}

template <class Key, class Value, class... Policies>
template <class... Args>
auto harris_michael_hash_map<Key, Value, Policies...>::emplace_or_get(Args&&... args)
  -> std::pair<iterator, bool>
{
  node* n = new node(construct_without_hash{}, std::forward<Args>(args)...);

  auto h = n->data.get_hash();
  auto bucket = map_to_bucket{}(h, num_buckets);

  find_info info{&buckets[bucket]};
  backoff backoff;
  for (;;)
  {
    if (find(h, n->data.value.first, bucket, info, backoff))
    {
      delete n;
      return {iterator(this, bucket, std::move(info)), false};
    }
    // Try to install new node
    marked_ptr cur = info.cur.get();
    info.cur.reset();
    info.cur = guard_ptr(n);
    n->next.store(cur, std::memory_order_relaxed);

    // (10) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6, 7)
    //        and the acquire-CAS (11, 13)
    //        it is the head of a potential release sequence containing (11, 13)
    if (info.prev->compare_exchange_weak(cur, n,
                                         std::memory_order_release,
                                         std::memory_order_relaxed))
      return {iterator(this, bucket, std::move(info)), true};

    backoff();
  }
}

template <class Key, class Value, class... Policies>
bool harris_michael_hash_map<Key, Value, Policies...>::erase(const Key& key)
{
  auto h = hash{}(key);
  auto bucket = map_to_bucket{}(h, num_buckets);
  backoff backoff;
  find_info info{&buckets[bucket]};
  // Find node in hash_map with matching key and mark it for erasure.
  do
  {
    if (!find(h, key, bucket, info, backoff))
      return false; // No such node in the hash_map
    // (11) - this acquire-CAS synchronizes with the release-CAS (8, 9, 10, 12, 14)
    //        and is part of a release sequence headed by those operations
  } while (!info.cur->next.compare_exchange_weak(info.next,
                                                 marked_ptr(info.next.get(), 1),
                                                 std::memory_order_acquire,
                                                 std::memory_order_relaxed));

  assert(info.next.mark() == 0);
  assert(info.cur.mark() == 0);

  // Try to splice out node
  marked_ptr expected = info.cur;
  // (12) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6, 7)
  //        and the acquire-CAS (11, 13)
  //        it is the head of a potential release sequence containing (11, 13)
  if (info.prev->compare_exchange_weak(expected, info.next.get(),
                                       std::memory_order_release,
                                       std::memory_order_relaxed))
    info.cur.reclaim();
  else
    // Another thread interfered -> rewalk the bucket's list to ensure
    // reclamation of marked node before returning.
    find(h, key, bucket, info, backoff);
   
  return true;
}

template <class Key, class Value, class... Policies>
auto harris_michael_hash_map<Key, Value, Policies...>::erase(iterator pos) -> iterator
{
  backoff backoff;
  auto next = pos.info.cur->next.load(std::memory_order_relaxed);
  while (next.mark() == 0)
  {
    // (13) - this acquire-CAS synchronizes with the release-CAS (8, 9, 10, 12, 14)
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
  // (14) - this release-CAS synchronizes with the acquire-load (1, 2, 3, 4, 5, 6, 7)
  //        and the acquire-CAS (11, 13)
  //        it is the head of a potential release sequence containing (11, 13)
  if (pos.info.prev->compare_exchange_weak(expected, next_guard,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
    pos.info.cur.reclaim();
    pos.info.cur = std::move(next_guard);
  } else {
    next_guard.reset();
    Key key = pos.info.cur->data.value.first;
    hash_t h = pos.info.cur->data.get_hash();

    // Another thread interfered -> rewalk the list to ensure reclamation of marked node before returning.
    find(h, key, pos.bucket, pos.info, backoff);
  }

  if (!pos.info.cur)
    pos.move_to_next_bucket();

  return pos;
}

template <class Key, class Value, class... Policies>
auto harris_michael_hash_map<Key, Value, Policies...>::operator[](const Key& key) -> accessor
{
  auto result = get_or_emplace_lazy(key, []() { return Value{}; });
  return accessor(std::move(result.first.info.cur));
}

template <class Key, class Value, class... Policies>
auto harris_michael_hash_map<Key, Value, Policies...>::begin() -> iterator
{
  return iterator(this, 0);
}

template <class Key, class Value, class... Policies>
auto harris_michael_hash_map<Key, Value, Policies...>::end() -> iterator
{
  return iterator(this);
}
}

#endif
