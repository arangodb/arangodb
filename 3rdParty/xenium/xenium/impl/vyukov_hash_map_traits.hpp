//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_VYUKOV_HASH_MAP_IMPL
#error "This is an impl file and must not be included directly!"
#endif

namespace xenium { namespace impl {
  namespace bits {
    template <class Key>
    struct vyukov_hash_map_common {
      using key_type = Key;

      template <class Accessor>
      static void reset(Accessor&& acc) { acc.reset(); }
    };

    template <class Key>
    struct vyukov_hash_map_trivial_key : vyukov_hash_map_common<Key> {
      template <class Cell>
      static bool compare_trivial_key(Cell& key_cell, const Key& key, hash_t hash) {
        return key_cell.load(std::memory_order_relaxed) == key;
      }

      template <class Accessor>
      static bool compare_nontrivial_key(const Accessor& acc, const Key& key) { return true; }

      template <class Hash>
      static hash_t rehash(const Key& k) { return Hash{}(k); }
    };

    template <class Key>
    struct vyukov_hash_map_nontrivial_key : vyukov_hash_map_common<Key> {
      template <class Cell>
      static bool compare_trivial_key(Cell& key_cell, const Key& key, hash_t hash) {
        return key_cell.load(std::memory_order_relaxed) == hash;
      }

      template <class Accessor>
      static bool compare_nontrivial_key(const Accessor& acc, const Key& key) {
        return acc.key() == key;
      }
      
      template <class Hash>
      static hash_t rehash(hash_t h) { return h; }
    };
  }
  template <class Key, class Value, class VReclaimer, class ValueReclaimer, class Reclaimer>
  struct vyukov_hash_map_traits<Key, managed_ptr<Value, VReclaimer>, ValueReclaimer, Reclaimer, true, true> :
    bits::vyukov_hash_map_trivial_key<Key>
  {
    static_assert(!parameter::is_set<ValueReclaimer>::value,
      "value_reclaimer policy can only be used with non-trivial key/value types");
    
    using value_type = Value*;
    using storage_key_type = std::atomic<Key>;
    using storage_value_type = typename VReclaimer::template concurrent_ptr<Value>;
    using iterator_value_type = std::pair<const Key, Value*>;
    using iterator_reference = iterator_value_type;

    class accessor {
    public:
      accessor() = default;
      Value* operator->() const noexcept { return guard.get(); }
      Value& operator*() const noexcept { return guard.get(); }
      void reset() { guard.reset(); }
      void reclaim() { guard.reclaim(); }
    private:
      accessor(storage_value_type& v, std::memory_order order):
        guard(acquire_guard(v, order))
      {}
      typename storage_value_type::guard_ptr guard;
      friend struct vyukov_hash_map_traits;
    };

    static accessor acquire(storage_value_type& v, std::memory_order order) {
      return accessor(v, order);
    }

    template <bool AcquireAccessor>
    static void store_item(storage_key_type& key_cell, storage_value_type& value_cell,
      hash_t hash, Key k, Value* v, std::memory_order order, accessor& acc)
    {
      key_cell.store(k, std::memory_order_relaxed);
      value_cell.store(v, order);
      if (AcquireAccessor)
        acc.guard = typename storage_value_type::guard_ptr(v);
    }

    template <bool AcquireAccessor>
    static bool compare_key(storage_key_type& key_cell, storage_value_type& value_cell, const Key& key,
      hash_t hash, accessor& acc)
    {
      if (key_cell.load(std::memory_order_relaxed) != key)
        return false;
      if (AcquireAccessor)
        acc.guard = typename storage_value_type::guard_ptr(value_cell.load(std::memory_order_relaxed));
      return true;
    }

    static iterator_reference deref_iterator(storage_key_type& k, storage_value_type& v) {
      return {k.load(std::memory_order_relaxed), v.load(std::memory_order_relaxed).get()};
    }

    static void reclaim(accessor& a) { a.guard.reclaim(); }
    static void reclaim_internal(accessor& a) {} // noop
  };

  template <class Key, class Value, class VReclaimer, class ValueReclaimer, class Reclaimer>
  struct vyukov_hash_map_traits<Key, managed_ptr<Value, VReclaimer>, ValueReclaimer, Reclaimer, false, true> :
    bits::vyukov_hash_map_nontrivial_key<Key>
  {
    using reclaimer = std::conditional_t<parameter::is_set<ValueReclaimer>::value, ValueReclaimer, Reclaimer>;

    struct node : reclaimer::template enable_concurrent_ptr<node> {
      node(Key&& key, Value* value):
        key(std::move(key)),
        value(std::move(value))
      {}

      Key key;
      typename VReclaimer::template concurrent_ptr<Value> value;
    };

    using value_type = Value*;
    using storage_key_type = std::atomic<size_t>;
    using storage_value_type = typename reclaimer::template concurrent_ptr<node>;
    using iterator_value_type = std::pair<const Key&, value_type>;
    using iterator_reference = iterator_value_type;

    class accessor {
    public:
      accessor() = default;
      Value* operator->() const noexcept { return value_guard.get(); }
      Value& operator*() const noexcept { return *value_guard.get(); }
      void reset() {
        value_guard.reset();
        node_guard.reset();
      }
    private:
      accessor(storage_value_type& v, std::memory_order order):
        node_guard(acquire_guard(v, order)),
        value_guard(acquire_guard(node_guard->value, order))
      {}
      const Key& key() const { return node_guard->key; }
      //accessor(typename storage_value_type::marked_ptr v) : guard(v) {}
      typename storage_value_type::guard_ptr node_guard;
      typename VReclaimer::template concurrent_ptr<Value>::guard_ptr value_guard;
      friend struct vyukov_hash_map_traits;
      friend struct bits::vyukov_hash_map_nontrivial_key<Key>;
    };

    static accessor acquire(storage_value_type& v, std::memory_order order) {
      return accessor(v, order);
    }

    template <bool AcquireAccessor>
    static void store_item(storage_key_type& key_cell, storage_value_type& value_cell,
      hash_t hash, Key&& k, Value* v, std::memory_order order, accessor& acc)
    {
      auto n = new node(std::move(k), v);
      if (AcquireAccessor) {
        acc.node_guard = typename storage_value_type::guard_ptr(n); // TODO - is this necessary?
        acc.value_guard = typename VReclaimer::template concurrent_ptr<Value>::guard_ptr(v);
      }
      key_cell.store(hash, std::memory_order_relaxed);
      value_cell.store(n, order);
    }

    template <bool AcquireAccessor>    
    static bool compare_key(storage_key_type& key_cell, storage_value_type& value_cell,
      const Key& key, hash_t hash, accessor& acc)
    {
      if (key_cell.load(std::memory_order_relaxed) != hash)
        return false;
      acc.node_guard = typename storage_value_type::guard_ptr(value_cell.load(std::memory_order_relaxed));
      if (acc.node_guard->key == key) {
        if (AcquireAccessor)
          acc.value_guard = typename VReclaimer::template concurrent_ptr<Value>::guard_ptr(acc.node_guard->value.load(std::memory_order_relaxed));
        return true;
      }
      return false;
    }

    static iterator_reference deref_iterator(storage_key_type& k, storage_value_type& v) {
      auto node = v.load(std::memory_order_relaxed);
      return {node->key, node->value.load(std::memory_order_relaxed).get() };
    }

    static void reclaim(accessor& a) {
      a.value_guard.reclaim();
      a.node_guard.reclaim();
    }
    static void reclaim_internal(accessor& a) {
      // copy guard to avoid resetting the accessor's guard_ptr.
      // TODO - this could be simplified by avoiding reset of
      // guard_ptrs in reclaim().
      auto g = a.node_guard;
      g.reclaim();
    }
  };

  template <class Key, class Value, class ValueReclaimer, class Reclaimer>
  struct vyukov_hash_map_traits<Key, Value, ValueReclaimer, Reclaimer, true, true> :
    bits::vyukov_hash_map_trivial_key<Key>
  {
    static_assert(!parameter::is_set<ValueReclaimer>::value,
      "value_reclaimer policy can only be used with non-trivial key/value types");

    using value_type = Value;
    using storage_key_type = std::atomic<Key>;
    using storage_value_type = std::atomic<Value>;
    using iterator_value_type = std::pair<const Key, Value>;
    using iterator_reference = iterator_value_type;

    class accessor {
    public:
      accessor() = default;
      const value_type& operator*() const noexcept { return v; }
    private:
      accessor(storage_value_type& v, std::memory_order order):
        v(v.load(order))
      {}
      value_type v;
      friend struct vyukov_hash_map_traits;
    };

    static void reset(accessor&& acc) {}
    static accessor acquire(storage_value_type& v, std::memory_order order) { return accessor(v, order); }

    template <bool AcquireAccessor>
    static void store_item(storage_key_type& key_cell, storage_value_type& value_cell,
      hash_t hash, Key k, Value v, std::memory_order order, accessor& acc)
    {
      key_cell.store(k, std::memory_order_relaxed);
      value_cell.store(v, order);
      if (AcquireAccessor)
        acc.v = v;
    }

    template <bool AcquireAccessor>    
    static bool compare_key(storage_key_type& key_cell, storage_value_type& value_cell,
      const Key& key, hash_t hash, accessor& acc)
    {
      if (key_cell.load(std::memory_order_relaxed) != key)
        return false;
      if (AcquireAccessor)
        acc.v = value_cell.load(std::memory_order_relaxed);
      return true;
    }

    static iterator_reference deref_iterator(storage_key_type& k, storage_value_type& v) {
      return {k.load(std::memory_order_relaxed), v.load(std::memory_order_relaxed)};
    }

    static void reclaim(accessor& a) {} // noop
    static void reclaim_internal(accessor& a) {} // noop
  };

  template <class Key, class Value, class ValueReclaimer, class Reclaimer>
  struct vyukov_hash_map_traits<Key, Value, ValueReclaimer, Reclaimer, true, false> :
    bits::vyukov_hash_map_trivial_key<Key>
  {
    using reclaimer = std::conditional_t<parameter::is_set<ValueReclaimer>::value, ValueReclaimer, Reclaimer>;

    struct node : reclaimer::template enable_concurrent_ptr<node> {
      node(Value&& value): value(std::move(value)) {}
      Value value;
    };

    using value_type = Value;
    using storage_key_type = std::atomic<Key>;
    using storage_value_type = typename reclaimer::template concurrent_ptr<node>;
    using iterator_value_type = std::pair<const Key, Value&>;
    using iterator_reference = iterator_value_type;

    class accessor {
    public:
      accessor() = default;
      Value* operator->() const noexcept { return &guard->value; }
      Value& operator*() const noexcept { return guard->value; }
      void reset() { guard.reset(); }
    private:
      accessor(storage_value_type& v, std::memory_order order):
        guard(acquire_guard(v, order))
      {}
      typename storage_value_type::guard_ptr guard;
      friend struct vyukov_hash_map_traits;
    };

    static accessor acquire(storage_value_type& v, std::memory_order order) {
      return accessor(v, order);
    }

    template <bool AcquireAccessor>    
    static bool compare_key(storage_key_type& key_cell, storage_value_type& value_cell,
      const Key& key, hash_t hash, accessor& acc)
    {
      if (key_cell.load(std::memory_order_relaxed) != key)
        return false;
      if (AcquireAccessor)
        acc.guard = typename storage_value_type::guard_ptr(value_cell.load(std::memory_order_relaxed));
      return true;
    }

    template <bool AcquireAccessor>
    static void store_item(storage_key_type& key_cell, storage_value_type& value_cell,
      hash_t hash, Key&& k, Value&& v, std::memory_order order, accessor& acc)
    {
      auto n = new node(std::move(v));
      if (AcquireAccessor)
        acc.guard = typename storage_value_type::guard_ptr(n);
      key_cell.store(k, std::memory_order_relaxed);
      value_cell.store(n, order);
    }

    static iterator_reference deref_iterator(storage_key_type& k, storage_value_type& v) {
      auto node = v.load(std::memory_order_relaxed);
      return {k.load(std::memory_order_relaxed), node->value};
    }

    static void reclaim(accessor& a) { a.guard.reclaim(); }
    static void reclaim_internal(accessor& a) {
      // copy guard to avoid resetting the accessor's guard_ptr.
      // TODO - this could be simplified by avoiding reset of
      // guard_ptrs in reclaim().
      auto g = a.guard;
      g.reclaim();
    }
  };

  template <class Key, class Value, class ValueReclaimer, class Reclaimer, bool TrivialValue>
  struct vyukov_hash_map_traits<Key, Value, ValueReclaimer, Reclaimer, false, TrivialValue> :
    bits::vyukov_hash_map_nontrivial_key<Key>
  {
    using reclaimer = std::conditional_t<parameter::is_set<ValueReclaimer>::value, ValueReclaimer, Reclaimer>;

    struct node : reclaimer::template enable_concurrent_ptr<node> {
      node(Key&& key, Value&& value):
        data(std::move(key), std::move(value))
      {}

      std::pair<const Key, Value> data;
    };

    using value_type = Value;
    using storage_key_type = std::atomic<hash_t>;
    using storage_value_type = typename reclaimer::template concurrent_ptr<node>;
    using iterator_value_type = std::pair<const Key, Value>;
    using iterator_reference = iterator_value_type&;

    class accessor {
    public:
      accessor() = default;
      Value* operator->() const noexcept { return &guard->data.second; }
      Value& operator*() const noexcept { return guard->data.second; }
      void reset() { guard.reset(); }
    private:
      accessor(storage_value_type& v, std::memory_order order):
        guard(acquire_guard(v, order))
      {}
      const Key& key() const { return guard->data.first; }
      typename storage_value_type::guard_ptr guard;
      friend struct vyukov_hash_map_traits;
      friend struct bits::vyukov_hash_map_nontrivial_key<Key>;
    };

    static accessor acquire(storage_value_type& v, std::memory_order order) {
      return accessor(v, order);
    }

    template <bool AcquireAccessor>
    static void store_item(storage_key_type& key_cell, storage_value_type& value_cell,
      hash_t hash, Key&& k, Value&& v, std::memory_order order, accessor& acc)
    {
      auto n = new node(std::move(k), std::move(v));
      if (AcquireAccessor)
        acc.guard = typename storage_value_type::guard_ptr(n);
      key_cell.store(hash, std::memory_order_relaxed);
      value_cell.store(n, order);
    }

    template <bool AcquireAccessor>    
    static bool compare_key(storage_key_type& key_cell, storage_value_type& value_cell,
      const Key& key, hash_t hash, accessor& acc)
    {
      if (key_cell.load(std::memory_order_relaxed) != hash)
        return false;
      acc.guard = typename storage_value_type::guard_ptr(value_cell.load(std::memory_order_relaxed));
      return acc.guard->data.first == key;
    }

    static iterator_reference deref_iterator(storage_key_type& k, storage_value_type& v) {
      auto node = v.load(std::memory_order_relaxed);
      return node->data;
    }

    static void reclaim(accessor& a) { a.guard.reclaim(); }
    static void reclaim_internal(accessor& a) {
      // copy guard to avoid resetting the accessor's guard_ptr.
      // TODO - this could be simplified by avoiding reset of
      // guard_ptrs in reclaim().
      auto g = a.guard;
      g.reclaim();
    }
  };
}}
