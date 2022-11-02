////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include "Basics/debugging.h"

namespace arangodb {

namespace rest {
class RestHandler;
}

/// @brief LogContext allows to add thread-local contextual information which
/// will automatically be included in all log messages.
///
/// LogContext manages thread-local instances to capture per-thread values.
/// Values are usually added using ScopedValues which add a value to the context
/// for the current scope, i.e., once the ScopedValue goes out of scope the
/// value is again removed from the context. That way the LogContext can be
/// populated with data when we move up the callstack, and clean up upon
/// returning. This is simple enough for serial execution, but when we hand
/// execution over to some other thread (e.g., using futures), we also need to
/// transfer the context. The easiest way to achieve that is by using the
/// `withLogContext` helper function.
///
/// Internally the values are managed in an immutable linked list using ref
/// counts. I.e., values that have been added to some LogContext are never
/// copied, even if that LogContext is captured in some futures.
class LogContext {
 public:
  /// @brief Visitor pattern to visit the values of some LogContext.
  /// Values that are not strings or numbers are stringified using operator<<
  /// and visited as string.
  struct Visitor;

  /// @brief Helper class to allow simple visitors with a single templatized
  /// visit function.
  template<class Derived>
  struct GenericVisitor;

  /// @brief Helper class to define a visitor using any type that provides
  /// operator(). This is typically used with overloaded lambdas or a single
  /// lambda with auto params.
  template<class Overloads>
  struct OverloadVisitor;

  struct Entry;
  struct EntryPtr;

  struct Values;

  template<const char K[], class V>
  struct KeyValue {
    static constexpr const char* Key = K;
    using Value = V;
  };

  /// @brief helper class to create value tuples with the following syntax:
  ///   LogContext::makeValue().with<Key1>(value1).with<Key2>(value2)
  /// The keys must be compile time constant char pointers; the values can
  /// be strings, numbers, or anything else than supports operator<<.
  /// As shown, the inital ValueBuilder instance can be created via
  /// LogContext::makeValue(). The `ValueBuilder` can be passed directly to
  /// `ScopedValue`, or you can call `share()` to create a reusable
  /// `std::shared_ptr<Values>` which can be stored in a member and used for
  /// different `ScopedValues`.
  template<class KV = void, class Base = void, std::size_t Depth = 0>
  struct ValueBuilder;

  /// @brief sets the given LogContext as the thread's current context for the
  /// current scope; restores the previous LogContext upon destruction.
  struct ScopedContext;

  struct Accessor {
   private:
    /// @brief adds the provided value(s) to the LogContext for the current
    /// scope, i.e., upon destruction the value(s) are removed from the current
    /// LogContext.
    struct ScopedValue;

    // We intentionally use this Accessor class with RestHandler as friend to
    // restrict usage of ScopedValues to certian classes in order to prevent
    // ScopedValues to be created in some inner function where they might cause
    // significant performance overhead.
    friend class arangodb::rest::RestHandler;

    friend struct LogContextTest;
  };

  /// @brief Helper to create an empty ValueBuilder.
  static ValueBuilder<> makeValue() noexcept;

  LogContext() = default;
  ~LogContext();

  LogContext(LogContext const& r) noexcept;
  LogContext(LogContext&& r) noexcept;
  LogContext& operator=(LogContext const& r) noexcept;
  LogContext& operator=(LogContext&& r) noexcept;

  /// @brief iterates through the local LogContext's key/value pairs and calls
  /// the visitor for each of them.
  void visit(Visitor const&) const;

  bool empty() const noexcept { return _tail == nullptr; }

  /// @brief returns the local LogContext
  static LogContext& current() noexcept;

  /// @brief sets the given context as the current LogContext.
  static void setCurrent(LogContext ctx) noexcept;

  struct Current {
    /// @brief adds the provided values to the LogContext. The returned entry
    /// must later be removed from the LogContext using `popEntry`. You should
    /// only revert to this function if you cannot use `ScopedValue`.
    static EntryPtr pushValues(std::shared_ptr<Values>);

    template<class KV, class Base, std::size_t Depth>
    static EntryPtr pushValues(ValueBuilder<KV, Base, Depth>&&);

    /// @brief removes the previously added entry from the LogContext.
    /// Note: it is important that entries are popped in the inverse order in
    /// which they were added.
    static void popEntry(EntryPtr&) noexcept;

   private:
    template<class T, class... Args>
    static EntryPtr appendEntry(Args&&... args);
  };

 private:
  struct ThreadControlBlock;

  template<const char... Ks[]>
  struct Keys {
    template<const char K[]>
    using With = Keys<Ks..., K>;
  };

  template<class... Vs>
  struct ValueTypes {
    template<class V>
    using With = ValueTypes<Vs..., V>;
    using Tuple = std::tuple<Vs...>;
  };

  static ThreadControlBlock& controlBlock() noexcept {
    return _threadControlBlock;
  }

  struct EntryCache;

  template<class Vals, class Keys>
  struct ValuesImpl;

  template<class Vals>
  struct EntryImpl;

  void doVisit(Visitor const&, Entry const*) const;

  Entry* pushEntry(std::unique_ptr<Entry>) noexcept;
  void popTail(EntryCache& cache) noexcept;
  void clear(EntryCache& cache);

  Entry* _tail{};

  static thread_local ThreadControlBlock _threadControlBlock;
};

struct LogContext::Visitor {
  virtual ~Visitor() = default;
  virtual void visit(std::string_view const& key,
                     std::string_view const& value) const = 0;
  virtual void visit(std::string_view const& key, double value) const = 0;
  virtual void visit(std::string_view const& key, std::int64_t value) const = 0;
  virtual void visit(std::string_view const& key,
                     std::uint64_t value) const = 0;
};

template<class Derived>
struct LogContext::GenericVisitor : Visitor {
  void visit(std::string_view const& key,
             std::string_view const& value) const override {
    self().visit(key, value);
  }
  void visit(std::string_view const& key, double value) const override {
    self().visit(key, value);
  }
  void visit(std::string_view const& key, std::int64_t value) const override {
    self().visit(key, value);
  }
  void visit(std::string_view const& key, std::uint64_t value) const override {
    self().visit(key, value);
  }

 private:
  Derived const& self() const noexcept {
    return static_cast<Derived const&>(*this);
  }
};

template<class Overloads>
struct LogContext::OverloadVisitor : GenericVisitor<OverloadVisitor<Overloads>>,
                                     Overloads {
  explicit OverloadVisitor(Overloads overloads)
      : GenericVisitor<OverloadVisitor>(), Overloads(std::move(overloads)) {}
  template<class T>
  void visit(std::string_view const& key, T&& value) const {
    this->operator()(key, std::forward<T>(value));
  }
};

template<>
struct LogContext::ValueBuilder<void, void, 0> {
  template<const char Key[], class Value>
  auto with(Value&& v) && {
    return ValueBuilder<KeyValue<Key, Value>, void, 1>(std::forward<Value>(v));
  }
};

template<const char K[], class V>
struct LogContext::ValueBuilder<LogContext::KeyValue<K, V>, void, 1> {
  template<const char Key[], class Value>
  auto with(Value&& v) && {
    return ValueBuilder<KeyValue<Key, Value>, ValueBuilder, 2>(
        *this, std::forward<Value>(v));
  }
  std::shared_ptr<Values> share() && {
    return std::make_shared<ValuesImpl<ValueTypesT, KeysT>>(std::move(_value));
  }

 private:
  using ValueTypesT = ValueTypes<std::decay_t<std::remove_reference_t<V>>>;
  using KeysT = Keys<K>;

  template<std::size_t Idx>
  V&& value() {
    static_assert(Idx == 0);
    return std::forward<V>(_value);
  }
  template<std::size_t Idx>
  static constexpr const char* key() {
    static_assert(Idx == 0);
    return K;
  }
  template<class F>
  auto passValues(F&& func) && {
    return std::forward<F>(func)(std::forward<V>(_value));
  }

  friend class LogContext;
  template<class VV>
  ValueBuilder(VV&& v) : _value(std::forward<VV>(v)) {}
  V&& _value;
};

template<const char K[], class V, class Base, std::size_t Depth>
struct LogContext::ValueBuilder<LogContext::KeyValue<K, V>, Base, Depth> {
  template<const char Key[], class Value>
  auto with(Value&& v) && {
    return ValueBuilder<KeyValue<Key, Value>, ValueBuilder, Depth + 1>(
        *this, std::forward<Value>(v));
  }
  std::shared_ptr<Values> share() && {
    return std::move(*this).passValues([]<class... Args>(Args && ... args) {
      return std::make_shared<ValuesImpl<ValueTypesT, KeysT>>(
          std::forward<Args>(args)...);
    });
  }

 private:
  using ValueTypesT = typename Base::ValueTypesT::template With<
      std::decay_t<std::remove_reference_t<V>>>;
  using KeysT = typename Base::KeysT::template With<K>;

  template<class F>
  auto passValues(F&& func) && {
    return [ this, &func ]<std::size_t... I>(std::index_sequence<I...>) {
      return std::forward<F>(func)(this->template value<I>()...);
    }
    (std::make_index_sequence<Depth>{});
  }

  template<std::size_t Idx>
  auto&& value() {
    static_assert(Idx < Depth);
    if constexpr (Idx + 1 == Depth) {
      return std::forward<V>(_value);
    } else {
      return _base.template value<Idx>();
    }
  }
  template<std::size_t Idx>
  static constexpr const char* key() {
    static_assert(Idx < Depth);
    if constexpr (Idx + 1 == Depth) {
      return K;
    } else {
      return Base::template key<Idx>();
    }
  }
  friend class LogContext;
  template<class VV>
  // cppcheck-suppress uninitMemberVarPrivate
  ValueBuilder(Base& base, VV&& v) : _base(base), _value(std::forward<VV>(v)) {}
  Base& _base;
  V&& _value;
};

struct LogContext::Values {
  virtual ~Values() = default;
  virtual void visit(Visitor const&) const = 0;
};

template<class Vals, const char... KeyValues[]>
struct LogContext::ValuesImpl<Vals, LogContext::Keys<KeyValues...>> final
    : Values {
  template<class... V>
  explicit ValuesImpl(V&&... v) : _values(std::forward<V>(v)...) {}
  void visit(Visitor const& visitor) const override {
    doVisit<0, KeyValues...>(visitor);
  }
  ValuesImpl const* operator->() const { return this; }

 private:
  template<std::size_t Idx, const char K[], const char... Ks[]>
  void doVisit(Visitor const& visitor) const {
    visitor.visit(K, toVisitorType(std::get<Idx>(_values)));
    if constexpr (Idx + 1 < std::tuple_size<typename Vals::Tuple>{}()) {
      doVisit<Idx + 1, Ks...>(visitor);
    }
  }

  template<class T>
  static auto toVisitorType(T&& v) {
    using TT = std::remove_reference_t<T>;
    if constexpr (std::is_floating_point_v<TT>) {
      return static_cast<double>(v);
    } else if constexpr (std::is_unsigned_v<TT>) {
      return static_cast<std::uint64_t>(v);
    } else if constexpr (std::is_signed_v<TT>) {
      return static_cast<std::int64_t>(v);
    } else if constexpr (std::is_constructible_v<std::string_view, T>) {
      return std::string_view(std::forward<T>(v));
    } else if constexpr (std::is_constructible_v<std::string, T>) {
      return std::string(std::forward<T>(v));
    } else {
      std::ostringstream ss;
      ss << v;
      return std::move(ss).str();
    }
  }

  typename Vals::Tuple _values;
};

struct LogContext::Entry {
  virtual ~Entry() { TRI_ASSERT(_refCount.load() <= 1); }
  virtual void visit(Visitor const&) const = 0;

 protected:
  virtual void release(EntryCache& cache) noexcept {
    this->~Entry();
    ::operator delete(this);
  }

 private:
  void incRefCnt() noexcept {
    TRI_ASSERT(_refCount.load() > 0);
    // (1) - this acquire-fetch-add synchronizes-with the release-fetch-sub (2)
    _refCount.fetch_add(1, std::memory_order_acquire);
  }
  [[nodiscard]] std::size_t decRefCnt() noexcept {
    TRI_ASSERT(_refCount.load() > 0);
    // (2) - this release-fetch-sub synchronizes-with the acquire-fetch-add (1)
    //       and the acquire-load (3)
    return _refCount.fetch_sub(1, std::memory_order_release);
  }

  friend class LogContext;
  std::atomic<std::size_t> _refCount{0};
  Entry* _prev{nullptr};
};

struct LogContext::EntryPtr {
  friend class LogContext;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  ~EntryPtr() {
    auto printValues = [](Entry* e) -> std::string {
      if (e == nullptr) {
        return {};
      }
      std::string out;
      LogContext::OverloadVisitor visitor([&out](std::string_view const& key,
                                                 auto&& value) {
        out.append(key).append(": ", 2);
        if constexpr (std::is_same_v<std::string_view,
                                     std::remove_cv_t<std::remove_reference_t<
                                         decltype(value)>>>) {
          out.append(value);
        } else {
          out.append(std::to_string(value));
        }
        out.append("; ", 2);
      });
      e->visit(visitor);
      return out;
    };
    TRI_ASSERT(_entry == nullptr)
        << "entry with the following values has not been removed: "
        << printValues(_entry);
  }
#endif
 private:
  Entry* _entry;

 public:
  constexpr EntryPtr() : _entry(nullptr) {}
  explicit EntryPtr(Entry* e) noexcept : _entry(e) {}

  EntryPtr(EntryPtr&& other) noexcept : _entry(other._entry) {
    other._entry = nullptr;
  }
  EntryPtr(EntryPtr const&) = delete;
  EntryPtr& operator=(EntryPtr&& other) noexcept {
    if (this != &other) {
      _entry = other._entry;
      other._entry = nullptr;
    }
    return *this;
  }
  EntryPtr& operator=(EntryPtr const&) = delete;
};

template<class Vals>
struct LogContext::EntryImpl final : Entry {
  // explicit EntryImpl(Vals&& v) : _values(std::move(v)) {}
  template<class... Args>
  explicit EntryImpl(Args&&... args) : _values(std::forward<Args>(args)...) {}
  void visit(Visitor const& visitor) const override { _values->visit(visitor); }
  void release(EntryCache& cache) noexcept override;

 private:
  Vals _values;
};

struct LogContext::EntryCache {
  struct CachedEntryAlloc {
    explicit CachedEntryAlloc(CachedEntryAlloc* n) noexcept : next(n) {}
    CachedEntryAlloc* const next;
  };

  ~EntryCache() {
    auto* e = _cachedEntries;
    while (e) {
      auto* next = e->next;
      e->~CachedEntryAlloc();
      ::operator delete(e);
      e = next;
    }
  }

  bool isFull() const noexcept { return _numCachedEntries >= MaxCachedEntries; }

  void addToCache(void* p) noexcept {
    auto* c = new (p) CachedEntryAlloc(_cachedEntries);
    _cachedEntries = c;
    ++_numCachedEntries;
  }

  void* popFromCache() {
    auto* p = _cachedEntries;
    if (p != nullptr) {
      _cachedEntries = p->next;
      p->~CachedEntryAlloc();
      --_numCachedEntries;
      return p;
    }
    return ::operator new(MinEntryCacheSize);
  }

  template<class T, class... Args>
  std::unique_ptr<Entry> makeEntry(Args&&... args) {
    void* p = sizeof(T) <= MinEntryCacheSize ? popFromCache()
                                             : ::operator new(sizeof(T));
    auto* e = new (p) T(std::forward<Args>(args)...);
    return std::unique_ptr<Entry>(e);
  }

  static inline constexpr char dummy[] =
      "dummy";  // only needed to calculate MinEntryCacheSize with MSVC
  static constexpr std::size_t MinEntryCacheSize =
      sizeof(EntryImpl<ValuesImpl<ValueTypes<std::string>, Keys<dummy>>>);

 private:
  static constexpr std::size_t MaxCachedEntries = 128;
  CachedEntryAlloc* _cachedEntries = nullptr;
  std::size_t _numCachedEntries = 0;
};

struct LogContext::ThreadControlBlock {
  ThreadControlBlock() = default;
  ThreadControlBlock(ThreadControlBlock const&) = delete;
  ThreadControlBlock(ThreadControlBlock&&) = delete;
  ThreadControlBlock& operator=(ThreadControlBlock const&) = delete;
  ThreadControlBlock& operator=(ThreadControlBlock&&) = delete;

  void pop(LogContext::Entry* entry) noexcept;

 private:
  friend class LogContext;
  LogContext _logContext;
  LogContext::EntryCache _entryCache;
};

struct LogContext::Accessor::ScopedValue {
  explicit ScopedValue(std::shared_ptr<LogContext::Values> v) {
    appendEntry<std::shared_ptr<LogContext::Values>>(std::move(v));
  }

  template<class KV, class Base, std::size_t Depth>
  explicit ScopedValue(ValueBuilder<KV, Base, Depth>&& v) {
    std::move(v).passValues([this]<class... Args>(Args && ... args) {
      this->appendEntry<
          ValuesImpl<typename ValueBuilder<KV, Base, Depth>::ValueTypesT,
                     typename ValueBuilder<KV, Base, Depth>::KeysT>>(
          std::forward<Args>(args)...);
    });
  }

  ScopedValue(ScopedValue const&) = delete;
  ScopedValue& operator=(ScopedValue const&) = delete;

  ~ScopedValue();

  template<const char Key[], class Val>
  static ScopedValue with(Val&& v) {
    return ScopedValue(LogContext::makeValue().with<Key>(std::forward<Val>(v)));
  }

 private:
  template<class T, class... Args>
  void appendEntry(Args&&... args) {
    auto& local = LogContext::controlBlock();
    auto e =
        local._entryCache.makeEntry<EntryImpl<T>>(std::forward<Args>(args)...);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _oldTail = e.get();
#endif
    local._logContext.pushEntry(std::move(e));
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  Entry* _oldTail = nullptr;
  std::thread::id _owningThread = std::this_thread::get_id();
#endif
};

/// @brief sets the given LogContext as the thread's current context for the
/// current scope; restores the previous LogContext upon destruction.
struct LogContext::ScopedContext {
  explicit ScopedContext(LogContext ctx) noexcept;
  ~ScopedContext();

 private:
  std::optional<LogContext> _oldContext;
};

// These functions are deliberately defined inline in this header file to allow
// more aggressive inlining for reduced overhead, even when compiling without
// IPO.

// LogContext::EntryImpl

template<class Vals>
inline void LogContext::EntryImpl<Vals>::release(EntryCache& cache) noexcept {
  // (3) - this acquire-load synchronizes-with the release-fetch-sub (2)
  std::ignore = this->_refCount.load(std::memory_order_acquire);
  void* p = this;
  this->~EntryImpl();
  if (sizeof(*this) <= LogContext::EntryCache::MinEntryCacheSize &&
      !cache.isFull()) {
    cache.addToCache(p);
  } else {
    ::operator delete(p);
  }
}

// LogContext::ScopedValue

inline LogContext::Accessor::ScopedValue::~ScopedValue() {
  auto& local = LogContext::controlBlock();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_oldTail == local._logContext._tail);
  TRI_ASSERT(_owningThread == std::this_thread::get_id());
#endif
  local._logContext.popTail(local._entryCache);
}

// LogContext::ThreadControlBlock

inline void LogContext::ThreadControlBlock::pop(Entry* entry) noexcept {
  TRI_ASSERT(_logContext._tail == entry);
  _logContext.popTail(_entryCache);
}

// LogContext

inline LogContext::~LogContext() {
  auto& cache = controlBlock()._entryCache;
  auto* t = _tail;
  while (t != nullptr) {
    auto prev = t->_prev;
    if (t->_refCount.load(std::memory_order_relaxed) == 1 ||
        t->decRefCnt() == 1) {
      // we have/had the only reference to this Entry, so we can "reuse" t's
      // reference to prev and therefore do not need to update any refCount.
      t->release(cache);
    } else {
      // we have decremented the refcnt, but some other LogContext still holds
      // a reference to this entry, so there is nothing left for us to do!
      break;
    }
    t = prev;
  }
}

inline LogContext::LogContext(LogContext const& r) noexcept : _tail(r._tail) {
  if (_tail) {
    _tail->incRefCnt();
  }
}

inline LogContext::LogContext(LogContext&& r) noexcept : _tail(r._tail) {
  r._tail = nullptr;
}

inline LogContext& LogContext::operator=(LogContext const& r) noexcept {
  TRI_ASSERT(&r != this);
  TRI_ASSERT(_tail == nullptr);
  _tail = r._tail;
  _tail->incRefCnt();
  return *this;
}

inline LogContext& LogContext::operator=(LogContext&& r) noexcept {
  TRI_ASSERT(&r != this);
  TRI_ASSERT(_tail == nullptr);
  _tail = r._tail;
  r._tail = nullptr;
  return *this;
}

inline LogContext::ValueBuilder<> LogContext::makeValue() noexcept {
  return ValueBuilder<>();
}

// the following attribute suppresses an UBSan false positive that reports
// a nullptr access to the LogContext object here. it seems UBSan has issues
// with thread-locals
#ifndef _MSC_VER
__attribute__((no_sanitize("null")))
#endif
inline LogContext&
LogContext::current() noexcept {
  return _threadControlBlock._logContext;
}

inline LogContext::EntryPtr LogContext::Current::pushValues(
    std::shared_ptr<Values> values) {
  return appendEntry<std::shared_ptr<LogContext::Values>>(std::move(values));
}

template<class KV, class Base, std::size_t Depth>
inline LogContext::EntryPtr LogContext::Current::pushValues(
    ValueBuilder<KV, Base, Depth>&& v) {
  return std::move(v).passValues([]<class... Args>(Args && ... args) {
    return Current::appendEntry<
        ValuesImpl<typename ValueBuilder<KV, Base, Depth>::ValueTypesT,
                   typename ValueBuilder<KV, Base, Depth>::KeysT>>(
        std::forward<Args>(args)...);
  });
}

template<class T, class... Args>
inline LogContext::EntryPtr LogContext::Current::appendEntry(Args&&... args) {
  auto& local = LogContext::controlBlock();
  auto e =
      local._entryCache.makeEntry<EntryImpl<T>>(std::forward<Args>(args)...);
  EntryPtr result(e.get());
  local._logContext.pushEntry(std::move(e));
  return result;
}

inline void LogContext::Current::popEntry(EntryPtr& entry) noexcept {
  if (entry._entry != nullptr) {
    auto& local = LogContext::controlBlock();
    TRI_ASSERT(entry._entry == local._logContext._tail);
    local._logContext.popTail(local._entryCache);
    entry._entry = nullptr;
  }
}

inline LogContext::Entry* LogContext::pushEntry(
    std::unique_ptr<Entry> entry) noexcept {
  TRI_ASSERT(entry->_refCount.load() == 0);
  entry->_prev = _tail;
  entry->_refCount.store(1, std::memory_order_relaxed);
  _tail = entry.release();
  return _tail;
}

inline void LogContext::popTail(EntryCache& cache) noexcept {
  TRI_ASSERT(_tail != nullptr);
  auto prev = _tail->_prev;
  if (_tail->_refCount.load(std::memory_order_relaxed) == 1) {
    // we have the only reference to this Entry, so we can "reuse" _tail's
    // reference to prev and therefore do not need to update any refCount.
    _tail->release(cache);
  } else if (prev) {
    prev->incRefCnt();
    if (_tail->decRefCnt() == 1) {
      TRI_ASSERT(prev->_refCount.load() > 1);
      std::ignore = prev->decRefCnt();
      _tail->release(cache);
    }
  } else if (_tail->decRefCnt() == 1) {
    _tail->release(cache);
  }
  _tail = prev;
}

/// @brief Captures the current LogContext and returns a new function that
/// applies the captures LogContext before calling the given function. This is
/// helpful in cases where a function is handed to some other thread but we want
/// to retain the current LogContext (e.g., when using futures).
template<typename Func>
auto withLogContext(Func&& func) {
  return [
    func = std::forward<Func>(func), ctx = LogContext::current()
  ]<typename... Args,
    typename = std::enable_if_t<std::is_invocable_v<Func, Args...>>>(
      Args && ... args) mutable {
    LogContext::ScopedContext ctxGuard(ctx);
    return std::forward<Func>(func)(std::forward<Args>(args)...);
  };
}

}  // namespace arangodb
