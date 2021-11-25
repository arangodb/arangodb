////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// for the current scope, i.e., once the ScopedValue goes out of scope the value
/// is again removed from the context. That way the LogContext can be populated
/// with data when we move up the callstack, and clean up upon returning.
/// This is simple enough for serial execution, but when we hand execution over
/// to some other thread (e.g., using futures), we also need to transfer the
/// context. The easiest way to achieve that is by using the `withLogContext`
/// helper function.
///
/// Internally the values are managed in an immutable linked list using ref counts.
/// I.e., values that have been added to some LogContext are never copied, even if
/// that LogContext is captured in some futures.
class LogContext {
 public:
  /// @brief Visitor pattern to visit the values of some LogContext.
  /// Values that are not strings or numbers are stringified using operator<< and visited as string.
  struct Visitor;

  /// @brief Helper class to allow simple visitors with a single templatized visit function.
  template <class Derived>
  struct GenericVisitor;

  /// @brief Helper class to define a visitor using any type that provides operator().
  /// This is typically used with overloaded lambdas or a single lambda with auto params.
  template <class Overloads>
  struct OverloadVisitor;

  struct Entry;

  struct Values;

  /// @brief helper class to create value tuples with the following syntax:
  ///   LogContext::makeValue().with<Key1>(value1).with<Key2>(value2)
  /// The keys must be compile time constant char pointers; the values can
  /// be strings, numbers, or anything else than supports operator<<.
  /// As shown, the inital ValueBuilder instance can be created via LogContext::makeValue().
  /// The `ValueBuilder` can be passed directly to `ScopedValue`, or you
  /// can call `share()` to create a reusable `std::shared_ptr<Values>`
  /// which can be stored in a member and used for different `ScopedValues`.
  template <class Vals = std::tuple<>, const char... Keys[]>
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

 private:
  struct ThreadControlBlock;

  static ThreadControlBlock& controlBlock() noexcept {
    return _threadControlBlock;
  }

  struct EntryCache;

  template <class Vals, const char... Keys[]>
  struct ValuesImpl;

  template <class Vals>
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
  virtual void visit(std::string_view const& key, std::string_view const& value) const = 0;
  virtual void visit(std::string_view const& key, double value) const = 0;
  virtual void visit(std::string_view const& key, std::int64_t value) const = 0;
  virtual void visit(std::string_view const& key, std::uint64_t value) const = 0;
};

template <class Derived>
struct LogContext::GenericVisitor : Visitor {
  void visit(std::string_view const& key, std::string_view const& value) const override {
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

template <class Overloads>
struct LogContext::OverloadVisitor : GenericVisitor<OverloadVisitor<Overloads>>,
                                     Overloads {
  explicit OverloadVisitor(Overloads overloads)
      : GenericVisitor<OverloadVisitor>(), Overloads(std::move(overloads)) {}
  template <class T>
  void visit(std::string_view const& key, T&& value) const {
    this->operator()(key, std::forward<T>(value));
  }
};

template <class Vals, const char... Keys[]>
struct LogContext::ValueBuilder {
  template <const char Key[], class Value>
  auto with(Value v) && {
    auto vals =
        std::tuple_cat(std::move(_vals), std::make_tuple(std::forward<Value>(v)));
    return ValueBuilder<decltype(vals), Keys..., Key>(std::move(vals));
  }
  std::shared_ptr<Values> share() && {
    return std::make_shared<ValuesImpl<Vals, Keys...>>(std::move(_vals));
  }

 private:
  friend class LogContext;
  ValueBuilder(Vals vals) : _vals(std::move(vals)) {}
  Vals _vals;
};

struct LogContext::Values {
  virtual ~Values() = default;
  virtual void visit(Visitor const&) const = 0;
};

template <class Vals, const char... Keys[]>
struct LogContext::ValuesImpl final : Values {
  explicit ValuesImpl(Vals vals) : _values(std::move(vals)) {}
  void visit(Visitor const& visitor) const override {
    doVisit<0, Keys...>(visitor);
  }
  ValuesImpl const* operator->() const { return this; }

 private:
  template <std::size_t Idx, const char K[], const char... Ks[]>
  void doVisit(Visitor const& visitor) const {
    visitor.visit(K, toVisitorType(std::get<Idx>(_values)));
    if constexpr (Idx + 1 < std::tuple_size<Vals>{}()) {
      doVisit<Idx + 1, Ks...>(visitor);
    }
  }
  
  template <class T>
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

  Vals _values;
};

struct LogContext::Entry {
  virtual ~Entry() {
    TRI_ASSERT(_refCount.load() <= 1);
  }
  virtual void visit(Visitor const&) const = 0;
  
 protected:
  virtual void release(EntryCache& cache) noexcept {
    this->~Entry();
    ::operator delete(this);
  }

 private:
  void incRefCnt() noexcept {
    TRI_ASSERT(_refCount.load() > 0);
    _refCount.fetch_add(1, std::memory_order_acquire);
  }
  [[nodiscard]] std::size_t decRefCnt() noexcept {
    TRI_ASSERT(_refCount.load() > 0);
    return _refCount.fetch_sub(1, std::memory_order_relaxed);
  }

  friend class LogContext;
  std::atomic<std::size_t> _refCount{0};
  Entry* _prev;
};

template <class Vals>
struct LogContext::EntryImpl final : Entry {
  explicit EntryImpl(Vals&& v) : _values(std::move(v)) {}
  void visit(Visitor const& visitor) const override { _values->visit(visitor); }
  void release(EntryCache& cache) noexcept override;

 private:
  Vals _values;
};

struct LogContext::EntryCache {
  struct CachedEntryAlloc {
    CachedEntryAlloc* next;
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
    auto* c = new (p) CachedEntryAlloc;
    c->next = _cachedEntries;
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

  template <class T, class Arg>
  std::unique_ptr<Entry> makeEntry(Arg&& arg) {
    void* p = sizeof(T) <= MinEntryCacheSize ? popFromCache()
                                             : ::operator new(sizeof(T));
    auto* e = new (p) T(std::forward<Arg>(arg));
    return std::unique_ptr<Entry>(e);
  }

  static inline constexpr char dummy[] = "dummy"; // only needed to calculate MinEntryCacheSize with MSVC
  static constexpr std::size_t MinEntryCacheSize =
      sizeof(EntryImpl<ValuesImpl<std::tuple<std::string>, dummy>>);

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

  template <class Vals, const char... Keys[]>
  LogContext::Entry* push(LogContext::ValueBuilder<Vals, Keys...>&& v);
  LogContext::Entry* push(std::shared_ptr<LogContext::Values> v);

  void pop(LogContext::Entry* entry) noexcept;

 private:
  friend class LogContext;
  LogContext _logContext;
  LogContext::EntryCache _entryCache;
};

struct LogContext::Accessor::ScopedValue {
  explicit ScopedValue(std::shared_ptr<LogContext::Values> v) {
    appendEntry(std::move(v));
  }

  template <class Vals, const char... Keys[]>
  explicit ScopedValue(ValueBuilder<Vals, Keys...>&& v) {
    using V = ValuesImpl<Vals, Keys...>;
    appendEntry(V(std::move(v._vals)));
  }

  ScopedValue(ScopedValue const&) = delete;
  ScopedValue& operator=(ScopedValue const&) = delete;
  
  ~ScopedValue();
  
  template <const char Key[], class Val>
  static ScopedValue with(Val&& v) { return ScopedValue(LogContext::makeValue().with<Key>(std::forward<Val>(v))); }

 private:
  template <class V>
  void appendEntry(V&& v) {
    auto& local = LogContext::controlBlock();
    auto e = local._entryCache.makeEntry<EntryImpl<V>>(std::forward<V>(v));
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
// more aggressive inlining for reduced overhead, even when compiling without IPO.

// LogContext::EntryImpl

template <class Vals>
inline void LogContext::EntryImpl<Vals>::release(EntryCache& cache) noexcept {
  if (sizeof(*this) <= LogContext::EntryCache::MinEntryCacheSize && !cache.isFull()) {
    this->~EntryImpl();
    cache.addToCache(this);
  } else {
    this->~EntryImpl();
    ::operator delete(this);
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

template <class Vals, const char... Keys[]>
inline LogContext::Entry* LogContext::ThreadControlBlock::push(ValueBuilder<Vals, Keys...>&& v) {
  using V = ValuesImpl<Vals, Keys...>;
  return _logContext.pushEntry(_entryCache.makeEntry<EntryImpl<V>>(V(std::move(v._vals))));
}

inline LogContext::Entry* LogContext::ThreadControlBlock::push(std::shared_ptr<Values> v) {
  return _logContext.pushEntry(
      _entryCache.makeEntry<EntryImpl<std::shared_ptr<Values>>>(std::move(v)));
}

// LogContext

inline LogContext::~LogContext() {
  auto& cache = controlBlock()._entryCache;
  auto* t = _tail;
  while (t != nullptr) {
    auto prev = t->_prev;
    if (t->_refCount == 1) {
      // we have the only reference to this Entry, so we can "reuse" t's reference
      // to prev and therefore do not need to update any refCount.
      t->release(cache);
    } else {
      if (prev) {
        prev->incRefCnt();
      }
      if (t->decRefCnt() == 1) {
        t->release(cache);
      }
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
  return ValueBuilder<std::tuple<>>({});
}

inline LogContext& LogContext::current() noexcept {
  return _threadControlBlock._logContext;
}

inline LogContext::Entry* LogContext::pushEntry(std::unique_ptr<Entry> entry) noexcept {
  entry->_prev = _tail;
  entry->_refCount.store(1, std::memory_order_relaxed);
  _tail = entry.release();
  return _tail;
}

inline void LogContext::popTail(EntryCache& cache) noexcept {
  TRI_ASSERT(_tail != nullptr);
  auto prev = _tail->_prev;
  if (_tail->_refCount == 1) {
    // we have the only reference to this Entry, so we can "reuse" _tail's reference
    // to prev and therefore do not need to update any refCount.
    _tail->release(cache);
  } else {
    if (prev) {
      prev->incRefCnt();
    }
    if (_tail->decRefCnt() == 1) {
      _tail->release(cache);
    }
  }
  _tail = prev;
}

/// @brief Captures the current LogContext and returns a new function that applies
/// the captures LogContext before calling the given function.
/// This is helpful in cases where a function is handed to some other thread but we
/// want to retain the current LogContext (e.g., when using futures).
template <typename Func>
auto withLogContext(Func&& func) {
  return [func = std::forward<Func>(func), ctx = LogContext::current()](auto&&... args) mutable {
    LogContext::ScopedContext ctxGuard(ctx);
    return std::forward<Func>(func)(std::forward<decltype(args)>(args)...);
  };
}

}  // namespace arangodb
