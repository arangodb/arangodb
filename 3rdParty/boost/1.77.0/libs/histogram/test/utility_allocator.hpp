// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/typeinfo.hpp>
#include <boost/histogram/detail/type_name.hpp>
#include <boost/throw_exception.hpp>
#include <initializer_list>
#include <iostream>
#include <unordered_map>
#include <utility>

struct tracing_allocator_db : std::pair<int, int> {
  template <class T>
  auto& at() {
    return map_[&BOOST_CORE_TYPEID(T)];
  }

  void clear() {
    map_.clear();
    this->first = 0;
    this->second = 0;
  }

  int failure_countdown = -1;
  bool tracing = false;

  template <class... Ts>
  void log(Ts&&... ts) {
    if (!tracing) return;
    // fold trick
    (void)std::initializer_list<int>{(std::cerr << ts, 0)...};
    std::cerr << std::endl;
  }

  std::size_t size() const { return map_.size(); }

private:
  using map_t = std::unordered_map<const boost::core::typeinfo*, std::pair<int, int>>;
  map_t map_;
};

template <class T>
struct tracing_allocator {
  using value_type = T;

  tracing_allocator_db* db = nullptr;

  tracing_allocator() noexcept = default;
  tracing_allocator(const tracing_allocator&) noexcept = default;
  tracing_allocator(tracing_allocator&&) noexcept = default;

  tracing_allocator(tracing_allocator_db& x) noexcept : db(&x) {}
  template <class U>
  tracing_allocator(const tracing_allocator<U>& a) noexcept : db(a.db) {}
  template <class U>
  tracing_allocator& operator=(const tracing_allocator<U>& a) noexcept {
    db = a.db;
    return *this;
  }
  ~tracing_allocator() noexcept {}

  T* allocate(std::size_t n) {
    if (db) {
      if (db->failure_countdown >= 0) {
        const auto count = db->failure_countdown--;
        db->log("allocator +", n, " ", boost::histogram::detail::type_name<T>(),
                " [failure in ", count, "]");
        if (count == 0) BOOST_THROW_EXCEPTION(std::bad_alloc{});
      } else
        db->log("allocator +", n, " ", boost::histogram::detail::type_name<T>());
      auto& p = db->at<T>();
      p.first += static_cast<int>(n);
      p.second += static_cast<int>(n);
      db->first += static_cast<int>(n * sizeof(T));
      db->second += static_cast<int>(n * sizeof(T));
    }
    return static_cast<T*>(::operator new(n * sizeof(T)));
  }

  void deallocate(T* p, std::size_t n) {
    if (db) {
      db->at<T>().first -= static_cast<int>(n);
      db->first -= static_cast<int>(n * sizeof(T));
      db->log("allocator -", n, " ", boost::histogram::detail::type_name<T>());
    }
    ::operator delete((void*)p);
  }

  template <class... Ts>
  void construct(T* p, Ts&&... ts) {
    if (db) {
      if (db->failure_countdown >= 0) {
        const auto count = db->failure_countdown--;
        db->log("allocator construct ", boost::histogram::detail::type_name<T>(),
                "[ failure in ", count, "]");
        if (count == 0) BOOST_THROW_EXCEPTION(std::bad_alloc{});
      } else
        db->log("allocator construct ", boost::histogram::detail::type_name<T>());
    }
    ::new (static_cast<void*>(p)) T(std::forward<Ts>(ts)...);
  }

  void destroy(T* p) {
    if (db) db->log("allocator destroy ", boost::histogram::detail::type_name<T>());
    p->~T();
  }
};

template <class T, class U>
constexpr bool operator==(const tracing_allocator<T>&,
                          const tracing_allocator<U>&) noexcept {
  return true;
}

template <class T, class U>
constexpr bool operator!=(const tracing_allocator<T>& t,
                          const tracing_allocator<U>& u) noexcept {
  return !operator==(t, u);
}
