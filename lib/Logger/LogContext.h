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

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace arangodb {

class LogContext {
 private:
  struct Entry;

  template <class Vals>
  struct EntryImpl;

  template <class Vals = std::tuple<>, const char... Keys[]>
  struct ValuesImpl;

 public:
  struct Visitor {
    virtual ~Visitor() = default;
    virtual void visit(std::string_view const& key, std::string_view const& value) const = 0;
    virtual void visit(std::string_view const& key, double value) const = 0;
    virtual void visit(std::string_view const& key, std::int64_t value) const = 0;
    virtual void visit(std::string_view const& key, std::uint64_t value) const = 0;
  };

  template <class Derived>
  struct GenericVisitor : Visitor {
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
  struct OverloadVisitor : GenericVisitor<OverloadVisitor<Overloads>>, Overloads {
    explicit OverloadVisitor(Overloads overloads) : GenericVisitor<OverloadVisitor>(), Overloads(std::move(overloads)) {}
    template <class T>
    void visit(std::string_view const& key, T&& value) const {
      this->operator()(key, std::forward<T>(value));
    }
  };

  struct Values {
    virtual ~Values() = default;
    virtual void visit(Visitor const&) = 0;
  };

  /// @brief helper class to create value tuples with the following syntax:
  ///   LogContext::makeValue().with<Key1>(value1).with<Key2>(value2)
  /// The keys must be compile time constant char pointers; the values can
  /// be strings or numbers.
  ///   TODO: support for arbitrary types that support operator<<
  /// As shown, the inital ValueBuilder instance can be created via LogContext::makeValue().
  /// The `ValueBuilder` can be passed directly to `ScopedValue`, or you
  /// can call `share()` to create a reusable `std::shared_ptr<Values>`
  /// which can be stored in a member and used for different `ScopedValues`.
  template <class Vals = std::tuple<>, const char... Keys[]>
  struct ValueBuilder {
    template <const char Key[], class Value>
    auto with(Value v) && {
      auto vals = std::tuple_cat(std::move(_vals), std::make_tuple(std::forward<Value>(v)));
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

  static ValueBuilder<> makeValue() { return ValueBuilder<std::tuple<>>({}); }

  /// @brief adds the provided value(s) to the LogContext for the current scope,
  /// i.e., upon destruction the value(s) are removed from the current LogContext.
  struct ScopedValue {
    template <class Vals, const char... Keys[]>
    explicit ScopedValue(ValueBuilder<Vals, Keys...>&& v) {
      using V = ValuesImpl<Vals, Keys...>;
      auto entry = std::make_shared<EntryImpl<V>>(V(std::move(v._vals)));
      appendEntry(entry);
    }

    explicit ScopedValue(std::shared_ptr<Values> v) {
      appendEntry(std::make_shared<EntryImpl<std::shared_ptr<Values>>>(std::move(v)));
    }

    ~ScopedValue();
  private:
    void appendEntry(std::shared_ptr<Entry> e) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _oldTail = e.get();
#endif
      LogContext::current().pushEntry(std::move(e));
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    Entry* _oldTail = nullptr;
#endif
  };

  /// @brief sets the given LogContext as the thread's current context for the
  /// current scope; restores the previous LogContext upon destruction.
  struct ScopedContext {
    explicit ScopedContext(LogContext ctx);
    ~ScopedContext();
   private:
    std::shared_ptr<Entry> _oldTail;
    bool _mustRestore = false;
  };

  LogContext() = default;

  /// @brief iterates through the local LogContext's key/value pairs and calls
  /// the visitor for each of them.
  void visit(Visitor const&) const;

  /// @brief returns the local LogContext
  static LogContext& current();

  /// @brief sets the given context as the current LogContext.
  static void setCurrent(LogContext ctx);

 private:
  template <class Vals, const char... Keys[]>
  struct ValuesImpl : Values {
    explicit ValuesImpl(Vals vals) : _values(std::move(vals)) {}
    void visit(Visitor const& visitor) override {
      doVisit<0, Keys...>(visitor);
    }
    ValuesImpl* operator->() { return this; }
   private:
    template <std::size_t Idx, const char K[], const char... Ks[]>
    void doVisit(Visitor const& visitor) {
      visitor.visit(K, toVisitorType(std::get<Idx>(_values)));
      if constexpr (Idx + 1 < std::tuple_size<Vals>()) {
        doVisit<Idx + 1, Ks...>(visitor);
      }
    }

    template <class T>
    auto toVisitorType(T&& v) {
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

  struct Entry : Values {
   private:
    friend class LogContext;
    std::shared_ptr<Entry> _prev; // TODO - make const?
  };

  template <class Vals>
  struct EntryImpl : Entry {
    explicit EntryImpl(Vals&& v) : _values(std::move(v)) {}
    void visit(Visitor const& visitor) override {
      _values->visit(visitor);
    };
   private:
    Vals _values;
  };

  LogContext(std::shared_ptr<Entry> tail) : _tail(std::move(tail)) {}

  void doVisit(Visitor const&, std::shared_ptr<Entry> const&) const;

  void pushEntry(std::shared_ptr<Entry>);
  void popTail() noexcept ;

  std::shared_ptr<Entry> _tail{};
};

template <typename Func>
auto withLogContext(Func&& func) {
  return [func = std::forward<Func>(func), ctx = LogContext::current()](auto&&... args) mutable {
    LogContext::ScopedContext ctxGuard(ctx);
    return std::forward<Func>(func)(std::forward<decltype(args)>(args)...);
  };
}

}  // namespace arangodb

