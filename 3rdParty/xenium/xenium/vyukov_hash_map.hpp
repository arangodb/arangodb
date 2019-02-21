//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_VYUKOV_HASH_MAP_HPP
#define XENIUM_VYUKOV_HASH_MAP_HPP

#include <xenium/acquire_guard.hpp>
#include <xenium/backoff.hpp>
#include <xenium/hash.hpp>
#include <xenium/parameter.hpp>
#include <xenium/policy.hpp>
#include <xenium/utils.hpp>

#include <atomic>
#include <cstdint>

namespace xenium {

namespace policy {
  /**
   * @brief Policy to configure the reclaimer used for internally alloced nodes in `vyukov_hash_map`.
   * 
   * If this policy is not specified, the implementation falls back to the reclaimer specified by
   * `xenium::policy::reclaimer`.
   * 
   * @tparam T the reclaimer to be used.
   */
  template <class T>
  struct value_reclaimer;
}

namespace impl {
  template <
    class Key,
    class Value,
    class ValueReclaimer,
    class Reclaimer,
    bool TrivialKey,
    bool TrivialValue>
  struct vyukov_hash_map_traits;
}

/**
 * @brief A helper struct to define that the lifetime of value objects of type `T`
 * has to be managed by the specified reclaimer. (only supported by `vyukov_hash_map`)
 * 
 * @tparam T
 * @tparam Reclaimer  
 */
template <class T, class Reclaimer>
struct managed_ptr;

namespace detail {
  template <class T>
  struct vyukov_supported_type {
    static constexpr bool value =
      std::is_trivial<T>::value && (sizeof(T) == 4 || sizeof(T) == 8);
  };
  template <class T, class Reclaimer>
  struct vyukov_supported_type<managed_ptr<T, Reclaimer>> {
    static_assert(
      std::is_base_of<typename Reclaimer::template enable_concurrent_ptr<T>, T>::value,
      "The type T specified in managed_ptr must derive from Reclaimer::enable_concurrent_ptr");
    static constexpr bool value = true;
  };
}

/**
 * @brief A concurrent hash-map that uses fine-grained locking.
 *
 * **This is a preliminary version; the interface will be subject to change.**
 *
 * This hash-map is heavily inspired by the hash-map presented by Vyukov
 * \[[Vyu08](index.html#ref-vyukov-2008)\].
 * It uses bucket-level locking for update operations (`emplace`/`erase`); however, read-only
 * operations (`try_get_value`) are lock-free. Buckets are cacheline aligned to reduce false
 * sharing and minimize cache thrashing.
 * 
 * There are two ways to access values of entries: via `iterator` or via `accessor`.
 * An `iterator` can be used to iterate the map, providing access to the current key/value
 * pair. An `iterator` keeps a lock on the currently iterated bucket, preventing concurrent
 * update operations on that bucket.
 * In contrast, an `accessor` provides safe access to a _single_ value, but without holding
 * any lock. The entry can safely be removed from the map even though some other thread
 * may have an `accessor` to its value. 
 * 
 * Some parts of the interface depend on whether the key/value types are trivially copyable
 * and have a size of 4 or 8 bytes (e.g., raw pointers, integers); such types are further
 * simply referred to as "trivial".
 * 
 * In addition to the distinction between trivial and non-trivial key/value types, it is
 * possible to specify a `managed_ptr` instantiation as value type.
 * 
 * Based on these possibilities the class behaves as follows:
 *   * trivial key, trivial value:
 *     * key/value are stored in separate atomics
 *     * `accessor` is a thin wrapper around a `Value` copy.
 *        Since the accessor contains a copy of the value it is limited to read-only access.
 *     * `iterator` dereferentiation returns a temporary `std::pair<const Key, Value>` object;
 *        the `operator->` is therefore not supported.
 *   * trivial key, managed_ptr with type `T` and reclaimer `R`:
 *     * `T` has to derive from `R::enable_concurrent_ptr<T>`
 *     * key is stored in an atomic, value is storend in a `concurrent_ptr<T>`
 *     * `accessor` is a thin wrapper around a `guard_ptr<T>`.
 *     * `iterator` dereferentiation returns a temporary `std::pair<const Key, T*>` object;
 *        the `operator->` is therefore not supported.
 *   * non-trivial key, managed_ptr with type `T` and reclaimer `R`:
 *     * `T` has to derive from `R::enable_concurrent_ptr<T>`
 *     * key and value are is stored in an internally allocated node
 *     * `accessor` is a thin wrapper containing a `guard_ptr` to the internal node, as
 *       well as a `guard_ptr<T>`.
 *     * `iterator` dereferentiation returns a temporary `std::pair<const Key&, T*>` object;
 *        the `operator->` is therefore not supported.
 *   * trivial key, non-trivial value
 *     * key is stored in an atomic, values are stored in internally allocated nodes.
 *       The lifetime of these nodes is managed via the specified the reclamation scheme
 *       (see `xenium::policy::value_reclaimer`).
 *     * `accessor` is a thin wrapper around a `guard_ptr` to the value's internal node
 *     * `iterator` dereferentiation returns a temporary `std::pair<const Key, Value&>` object;
 *        the `operator->` is therefore not supported.
 *   * non-trivial key, non-trivial value
 *     * key and value are stored in a `std::pair` in an internally allocated node
 *     * the key's hash value is memoized in an atomic in the bucket to ensure fast comparison
 *     * `accessor` is a thin wrapper around a `guard_ptr` to the internal node
 *     * `iterator` dereferentiation returns a reference to the node's `std::pair` object;
 *        this is the only configuration that supports the `operator->`.
 * 
 * Supported policies:
 *  * `xenium::policy::reclaimer`<br>
 *    Defines the reclamation scheme to be used for internal allocations. (**required**)
 *  * `xenium::policy::hash`<br>
 *    Defines the hash function. (*optional*; defaults to `xenium::hash<Key>`)
 *  * `xenium::policy::backoff`<br>
 *    Defines the backoff strategy. (*optional*; defaults to `xenium::no_backoff`)
 *
 * @tparam Key
 * @tparam Value
 * @tparam Policies
 */
template <class Key, class Value, class... Policies>
struct vyukov_hash_map {
  using reclaimer = parameter::type_param_t<policy::reclaimer, parameter::nil, Policies...>;
  using value_reclaimer = parameter::type_param_t<policy::value_reclaimer, parameter::nil, Policies...>;
  using hash = parameter::type_param_t<policy::hash, xenium::hash<Key>, Policies...>;
  using backoff = parameter::type_param_t<policy::backoff, no_backoff, Policies...>;

  template <class... NewPolicies>
  using with = vyukov_hash_map<Key, Value, NewPolicies..., Policies...>;

  static_assert(parameter::is_set<reclaimer>::value, "reclaimer policy must be specified");

private:
  using traits = typename impl::vyukov_hash_map_traits<Key, Value, value_reclaimer, reclaimer,
    detail::vyukov_supported_type<Key>::value, detail::vyukov_supported_type<Value>::value>;

public:
  vyukov_hash_map(std::size_t initial_capacity = 128);
  ~vyukov_hash_map();

  class iterator;
  using accessor = typename traits::accessor;
  
  using key_type = typename traits::key_type;
  using value_type = typename traits::value_type;

  /**
   * @brief Inserts a new element into the container if the container doesn't already contain an
   * element with an equivalent key.
   *
   * The element is only constructed if no element with the key exists in the container.
   * No iterators or accessors are invalidated.
   *
   * Progress guarantees: blocking
   * 
   * @param key the key of element to be inserted.
   * @param value the value of the element to be inserted
   * @return `true` if an element was inserted, otherwise `false`
   */
  bool emplace(key_type key, value_type value);

  /**
   * @brief Inserts a new element into the container if the container doesn't already contain an
   * element with an equivalent key.
   *
   * The element is only constructed if no element with the key exists in the container.
   * No iterators or accessors are invalidated.
   *
   * Progress guarantees: blocking
   * 
   * @param key the key of element to be inserted.
   * @param args arguments to forward to the constructor of the element
   * @return a pair consisting of an accessor to the inserted element, or the already-existing element
   * if no insertion happened, and a bool denoting whether the insertion took place;
   * `true` if an element was inserted, otherwise `false`
   */
  template <class... Args>
  std::pair<accessor, bool> get_or_emplace(key_type key, Args&&... args);

  /**
   * @brief Inserts a new element into the container if the container doesn't already contain an
   * element with an equivalent key. The value for the newly constructed element is created by
   * calling `value_factory`.
   *
   * The element is only constructed if no element with the key exists in the container.
   * No iterators or accessors are invalidated.
   *
   * Progress guarantees: blocking
   * 
   * @tparam Func
   * @param key the key of element to be inserted.
   * @param factory a functor that is used to create the `Value` instance when constructing
   * the new element to be inserted.
   * @return a pair consisting of an accessor to the inserted element, or the already-existing element
   * if no insertion happened, and a bool denoting whether the insertion took place;
   * `true` if an element was inserted, otherwise `false`
   */
  template <class Factory>
  std::pair<accessor, bool> get_or_emplace_lazy(key_type key, Factory&& factory);

  /**
   * @brief Removes the element with the key equivalent to key (if one exists), and provides an
   * accessor to the removed value.
   *
   * No iterators or accessors are invalidated.
   *
   * Progress guarantees: blocking
   *  
   * @param key key of the element to remove
   * @param accessor reference to an accessor to be set in case an element is removed.
   * @return `true` if an element was removed, otherwise `false`
   */
  bool extract(const key_type& key, accessor& value);

  /**
   * @brief Removes the element with the key equivalent to key (if one exists).
   *
   * No iterators or accessors are invalidated.
   *
   * Progress guarantees: blocking
   *  
   * @param key key of the element to remove
   * @return `true` if an element was removed, otherwise `false`
   */
  bool erase(const key_type& key);

  /**
   * @brief Removes the specified element from the container.
   *
   * No iterators or accessors are invalidated.
   *
   * Progress guarantees: blocking
   * 
   * @param pos the iterator identifying the element to remove;
   * pos is implicitly updated to refer to the next element.
   */
  void erase(iterator& pos);

  /**
   * @brief Provides an accessor to the value associated with the specified key,
   * if such an element exists in the map.
   *
   * No iterators or accessors are invalidated.
   *
   * Progress guarantees: lock-free
   * 
   * @param key key of the element to search for
   * @param result reference to an accessor to be set if a matching element is found
   * @return `true` if an element was found, otherwise `false`
   */
  bool try_get_value(const key_type& key, accessor& result) const;
  
  // TODO
  //bool contains(const key_type& key) const;

  /**
   * @brief Finds an element with key equivalent to key.
   * 
   * Progress guarantees: blocking
   * 
   * @param key key of the element to search for
   * @return iterator to an element with key equivalent to key if such element is found,
   * otherwise past-the-end iterator
   */
  iterator find(const key_type& key);
  
  /**
   * @brief Returns an iterator to the first element of the container. 
   * 
   * Progress guarantees: blocking
   * 
   * @return iterator to the first element 
   */
  iterator begin();

  /**
   * @brief Returns an iterator to the element following the last element of the container.
   * 
   * This element acts as a placeholder; attempting to access it results in undefined behavior. 
   * Progress guarantees: wait-free
   * 
   * @return iterator to the element following the last element.
   */
  iterator end() { return iterator(); }
private:
  struct unlocker;
  
  struct bucket_state;
  struct bucket;
  struct extension_item;
  struct extension_bucket;
  struct block;
  using block_ptr = typename reclaimer::template concurrent_ptr<block, 0>;  
  using guarded_block = typename block_ptr::guard_ptr;

  static constexpr std::uint32_t bucket_to_extension_ratio = 128;
  static constexpr std::uint32_t bucket_item_count = 3;
  static constexpr std::uint32_t extension_item_count = 10;
  
  static constexpr std::size_t item_counter_bits = utils::find_last_bit_set(bucket_item_count);
  static constexpr std::size_t lock_bit = 2 * item_counter_bits + 1;
  static constexpr std::size_t version_shift = lock_bit;

  static constexpr std::uint32_t lock = 1u << (lock_bit - 1);
  static constexpr std::size_t version_inc = 1ul << lock_bit;
  
  static constexpr std::uint32_t item_count_mask = (1u << item_counter_bits) - 1;
  static constexpr std::uint32_t delete_item_mask = item_count_mask << item_counter_bits;

  block_ptr data_block;
  std::atomic<int> resize_lock;

  block* allocate_block(std::uint32_t bucket_count);
  
  bucket& lock_bucket(hash_t hash, guarded_block& block, bucket_state& state);
  void grow(bucket& bucket, bucket_state state);

  template <bool AcquireAccessor, class Factory, class Callback>
  bool do_get_or_emplace(Key&& key, Factory&& factory, Callback&& callback);
  
  bool do_extract(const key_type& key, accessor& value);
  
  static extension_item* allocate_extension_item(block* b, hash_t hash);
  static void free_extension_item(extension_item* item);
};

/**
 * @brief A ForwardIterator to safely iterate `vyukov_hash_map`.
 * 
 * Iterators hold a lock on the currently iterated bucket, blocking concurrent
 * updates (emplace/erase) of that bucket, as well as grow operations or other
 * iterators trying to acquire a lock on that bucket.
 * In order to avoid deadlocks, consider the following guidelines:
 *   * `reset` an iterator once it is no longer required.
 *   * do not acquire more than one iterator on the same map.
 *   * do not call `emplace` while holding an iterator.
 *   * do not call `erase` with a key while holding an iterator.
 * 
 * Since the bucket locks are exclusive, it is not possible to copy iterators;
 * i.e., copy constructor, copy assignment and postfix increment are not available.
 * Instead, use move construction, move assignment and prefix increment.
 * 
 * Iterators are not invalidated by concurrent update operations.
 */
template <class Key, class Value, class... Policies>
class vyukov_hash_map<Key, Value, Policies...>::iterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = typename traits::iterator_value_type;  
  using reference = typename traits::iterator_reference;
  using pointer = value_type*;

  iterator();
  ~iterator();

  iterator(iterator&&) = default;
  iterator& operator=(iterator&&) = default;
  
  iterator(const iterator&) = delete;
  iterator& operator=(const iterator&) = delete;

  bool operator==(const iterator& r) const;
  bool operator!=(const iterator& r) const;
  iterator& operator++();

  reference operator*();
  pointer operator->();

  /**
   * @brief Releases the bucket lock and resets the iterator.
   * 
   * After calling `reset` the iterator equals `end()`.
   */
  void reset();
private:
  guarded_block block;
  bucket* current_bucket;
  bucket_state current_bucket_state;
  std::uint32_t index;
  extension_item* extension;
  std::atomic<extension_item*>* prev;
  friend class vyukov_hash_map;

  void move_to_next_bucket();
  Value* erase_current();
};

}

#define XENIUM_VYUKOV_HASH_MAP_IMPL
#include <xenium/impl/vyukov_hash_map_traits.hpp>
#include <xenium/impl/vyukov_hash_map.hpp>
#undef XENIUM_VYUKOV_HASH_MAP_IMPL

#endif
