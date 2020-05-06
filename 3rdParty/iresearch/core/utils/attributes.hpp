////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ATTRIBUTES_H
#define IRESEARCH_ATTRIBUTES_H

#include <map>
#include <set>

#include "attributes_provider.hpp"
#include "map_utils.hpp"
#include "noncopyable.hpp"
#include "memory.hpp"
#include "string.hpp"
#include "timer_utils.hpp"
#include "bit_utils.hpp"
#include "type_id.hpp"
#include "noncopyable.hpp"
#include "string.hpp"

NS_ROOT

struct IRESEARCH_API attributes {
  static bool exists(
    const string_ref& name,
    bool load_library = true);

  static type_info get(
    const string_ref& name,
    bool load_library = true) noexcept;

  attributes() = delete;
};

//////////////////////////////////////////////////////////////////////////////
/// @class attribute
/// @brief base class for all attributes that can be used with attribute_map
///        an empty struct tag type with no virtual methods
///        all derived classes must implement the following function:
///        static const attribute::type_id type() noexcept
///          via DECLARE_ATTRIBUTE_TYPE()/DEFINE_ATTRIBUTE_TYPE(...)
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API attribute { };

//////////////////////////////////////////////////////////////////////////////
/// @brief base class for all attributes that can be deallocated via a ptr of
///        this struct type using a virtual destructor
///        an empty struct tag type with a virtual destructor
///        all derived classes must implement the following functions:
///        static const attribute::type_id type() noexcept
///          via DECLARE_ATTRIBUTE_TYPE()/DEFINE_ATTRIBUTE_TYPE(...)
///        static ptr make(Args&&... args)
///          via DECLARE_FACTORY()/DECLARE_FACTORY()
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API stored_attribute : attribute {
  DECLARE_UNIQUE_PTR(stored_attribute);
  virtual ~stored_attribute() = default;
};

// -----------------------------------------------------------------------------
// --SECTION--                                            Attribute registration
// -----------------------------------------------------------------------------

class IRESEARCH_API attribute_registrar {
 public:
  explicit attribute_registrar(
    const type_info& type,
    const char* source = nullptr);
  operator bool() const noexcept;

 private:
  bool registered_;
};

#define REGISTER_ATTRIBUTE__(attribute_name, line, source) \
  static ::iresearch::attribute_registrar attribute_registrar ## _ ## line(::iresearch::type<attribute_name>::get(), source)
#define REGISTER_ATTRIBUTE_EXPANDER__(attribute_name, file, line) REGISTER_ATTRIBUTE__(attribute_name, line, file ":" TOSTRING(line))
#define REGISTER_ATTRIBUTE(attribute_name) REGISTER_ATTRIBUTE_EXPANDER__(attribute_name, __FILE__, __LINE__)

//////////////////////////////////////////////////////////////////////////////
/// @class flags
/// @brief represents a set of features enabled for the particular field
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API flags {
 public:
  // std::set<...> is 25% faster than std::unordered_set<...> as per profile_bulk_index test
  typedef std::set<type_info::type_id> type_map;

  static const flags& empty_instance();

  flags() = default;
  flags(const flags&) = default;
  flags(flags&& rhs) = default;
  flags(std::initializer_list<type_info> flags);
  flags& operator=(std::initializer_list<type_info> flags);
  flags& operator=(flags&& rhs) = default;
  flags& operator=(const flags&) = default;

  type_map::const_iterator begin() const noexcept { return map_.begin(); }
  type_map::const_iterator end() const noexcept { return map_.end(); }

  template< typename T >
  flags& add() {
    typedef typename std::enable_if<
      std::is_base_of< attribute, T >::value, T
    >::type attribute_t;

    return add(type<attribute_t>::id());
  }

  flags& add(type_info::type_id type) {
    map_.insert(type);
    return *this;
  }

  template<typename T>
  flags& remove() {
    typedef typename std::enable_if<
      std::is_base_of<attribute, T>::value, T
    >::type attribute_t;

    return remove(type<attribute_t>::id());
  }

  flags& remove(type_info::type_id type) {
    map_.erase(type);
    return *this;
  }

  bool empty() const noexcept { return map_.empty(); }
  size_t size() const noexcept { return map_.size(); }
  void clear() noexcept { map_.clear(); }
  void reserve(size_t /*capacity*/) {
    // NOOP for std::set
  }

  template<typename T>
  bool check() const noexcept {
    typedef typename std::enable_if<
      std::is_base_of< attribute, T >::value, T
    >::type attribute_t;

    return check(type<attribute_t>::id());
  }

  bool check(type_info::type_id type) const noexcept {
    return map_.end() != map_.find(type);
  }

  bool operator==(const flags& rhs) const {
    return map_ == rhs.map_;
  }

  bool operator!=(const flags& rhs) const {
    return !(*this == rhs);
  }

  flags& operator|=(const flags& rhs) {
    std::for_each(
      rhs.map_.begin(), rhs.map_.end() ,
      [this] (type_info::type_id type) { map_.insert(type); });
    return *this;
  }

  flags operator&(const flags& rhs) const {
    const type_map* lhs_map = &map_;
    const type_map* rhs_map = &rhs.map_;
    if (lhs_map->size() > rhs_map->size()) {
      std::swap(lhs_map, rhs_map);
    }

    flags out;
    out.reserve(lhs_map->size());

    for (auto lhs_it = lhs_map->begin(), lhs_end = lhs_map->end(); lhs_it != lhs_end; ++lhs_it) {
      auto rhs_it = rhs_map->find(*lhs_it);
      if (rhs_map->end() != rhs_it) {
        out.map_.insert(*rhs_it);
      }
    }

    return out;
  }

  flags operator|(const flags& rhs) const {
    flags out(*this);
    out.reserve(rhs.size());
    for (auto it = rhs.map_.begin(), end = rhs.map_.end(); it != end; ++it) {
      out.map_.insert(*it);
    }
    return out;
  }

  bool is_subset_of(const flags& rhs) const {
    auto& rhs_map = rhs.map_;
    for (auto entry : map_) {
      if (rhs_map.end() == rhs_map.find(entry)) {
        return false;
      }
    }
    return true;
  }

 private:
  template<
    typename T,
    template <typename, typename...> class Ref,
    typename... Args>
  friend class attribute_map;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  type_map map_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

//////////////////////////////////////////////////////////////////////////////
/// @brief common interface for attribute storage implementations
//////////////////////////////////////////////////////////////////////////////
template<
  typename T,
  template <typename, typename...> class Ref,
  typename... Args
> class IRESEARCH_API_TEMPLATE attribute_map {
 public:
  template<typename U>
  struct ref {
    typedef Ref<U, Args...> type;

    IRESEARCH_HELPER_DLL_LOCAL static const type NIL;

    static_assert(
      sizeof(typename ref<T>::type) == sizeof(type),
      "sizeof(typename ref<T>::type) != sizeof(type)"
    );
  };

  attribute_map() = default;

  attribute_map(const attribute_map& other) = default;

  attribute_map(attribute_map&& other) noexcept {
    *this = std::move(other);
  }

  attribute_map& operator=(const attribute_map& other) = default;

  attribute_map& operator=(attribute_map&& other) noexcept {
    if (this != &other) {
      map_ = std::move(other.map_);
    }

    return *this;
  }

  void clear() {
    map_.clear();
  }

  bool contains(type_info::type_id type) const noexcept {
    return map_.find(type) != map_.end();
  }

  template<typename A>
  inline bool contains() const noexcept {
    typedef typename std::enable_if<
      std::is_base_of<attribute, A >::value, A
    >::type type;

    return contains(irs::type<type>::id());
  }

  flags features() const {
    flags features;

    for (auto& entry: map_) {
      features.map_.insert(entry.first);
    }

    return features;
  }

  template<typename A>
  inline typename ref<A>::type* get() noexcept {
    typedef typename std::enable_if<
      std::is_base_of<attribute, A>::value, A
    >::type type;

    auto* value = get(irs::type<type>::id());

    // safe to reinterpret because layout/size is identical
    return reinterpret_cast<typename ref<A>::type*>(value);
  }

  template<typename A>
  inline typename ref<A>::type& get(
      typename ref<A>::type& fallback) noexcept {
    typedef typename std::enable_if<
      std::is_base_of<attribute, A>::value, A
    >::type type;

    auto& value = get(irs::type<type>::id(), reinterpret_cast<typename ref<T>::type&>(fallback));

    // safe to reinterpret because layout/size is identical
    return reinterpret_cast<typename ref<A>::type&>(value);
  }

  template<typename A>
  inline const typename ref<A>::type& get(
      const typename ref<A>::type& fallback = ref<A>::NIL) const noexcept {
    typedef typename std::enable_if<
      std::is_base_of<attribute, A>::value, A
    >::type type;

    auto& value = get(
      irs::type<type>::id(),
      reinterpret_cast<const typename ref<T>::type&>(fallback));

    // safe to reinterpret because layout/size is identical
    return reinterpret_cast<const typename ref<A>::type&>(value);
  }

  typename ref<T>::type& get(
      type_info::type_id type,
      typename ref<T>::type& fallback) noexcept {
    const auto itr = map_.find(type);

    return map_.end() == itr ? fallback : itr->second;
  }

  typename ref<T>::type* get(type_info::type_id type) noexcept {
    const auto itr = map_.find(type);

    return map_.end() == itr ? nullptr : &(itr->second);
  }

  const typename ref<T>::type& get(
      type_info::type_id type,
      const typename ref<T>::type& fallback = ref<T>::NIL) const noexcept {
    return const_cast<attribute_map*>(this)->get(type, const_cast<typename ref<T>::type&>(fallback));
  }

  bool remove(type_info::type_id type) {
    return map_.erase(type) > 0;
  }

  template<typename A>
  inline bool remove() {
    typedef typename std::enable_if<std::is_base_of<attribute, A>::value, A>::type type;

    return remove(irs::type<type>::id());
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor) {
    return visit(*this, visitor);
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor) const {
    return visit(*this, visitor);
  }

  size_t size() const noexcept { return map_.size(); }

  bool empty() const noexcept { return map_.empty(); }

 protected:
  typename ref<T>::type& emplace(bool& inserted, type_info::type_id type) {
    const auto res = map_.try_emplace(type);

    inserted = res.second;

    return res.first->second;
  }

 private:
  // std::map<...> is 25% faster than std::unordered_map<...> as per profile_bulk_index test
  typedef std::map<type_info::type_id, typename ref<T>::type> map_t;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  map_t map_;
  IRESEARCH_API_PRIVATE_VARIABLES_END

  template<typename Attributes, typename Visitor>
  static bool visit(Attributes& attrs, const Visitor& visitor) {
    for (auto& entry : attrs.map_) {
      if (!visitor(entry.first, entry.second)) {
        return false;
      }
    }
    return true;
  }
}; // attribute_map

// Prevent using MSVS lett than 16.3 due to: `fatal error C1001: An internal
// error has occurred in the compiler (compiler file 'msc1.cpp', line 1527)`
#if defined _MSC_VER
  static_assert(_MSC_VER < 1920 || _MSC_VER >= 1923, "_MSC_VER < 1920 || _MSC_VER >= 1923");
#endif
template<typename T, template <typename, typename...> class Ref, typename... Args>
template<typename U>
typename attribute_map<T, Ref, Args...>::template ref<U>::type
const attribute_map<T, Ref, Args...>::ref<U>::NIL;

//////////////////////////////////////////////////////////////////////////////
/// @brief storage of shared_ptr to attributes
//////////////////////////////////////////////////////////////////////////////

MSVC_ONLY(template class IRESEARCH_API irs::attribute_map<stored_attribute, std::shared_ptr>;)

class IRESEARCH_API attribute_store
    : public attribute_map<stored_attribute, std::shared_ptr> {
 public:
  typedef attribute_map<stored_attribute, std::shared_ptr> base_t;

  static const attribute_store& empty_instance();

  explicit attribute_store(size_t reserve = 0);

  attribute_store(const attribute_store&) = default;

  attribute_store(attribute_store&& rhs) noexcept
    : base_t(std::move(rhs)) {
  }

  attribute_store& operator=(const attribute_store&) = default;

  attribute_store& operator=(attribute_store&& rhs) noexcept {
    base_t::operator=(std::move(rhs));
    return *this;
  }

  template<typename T, typename... Args>
  typename ref<T>::type& try_emplace(bool& inserted, Args&&... args) {
    REGISTER_TIMER_DETAILED();

    typedef typename std::enable_if<
      std::is_base_of<stored_attribute, T>::value, T
    >::type type;

    auto& attr = attribute_map::emplace(inserted, irs::type<type>::id());

    if (inserted) {
      attr = type::make(std::forward<Args>(args)...);
    }

    return reinterpret_cast<typename ref<T>::type&>(attr);
  }

  template<typename T, typename... Args>
  typename ref<T>::type& emplace(Args&&... args) {
    bool inserted;
    return try_emplace<T>(inserted, std::forward<Args>(args)...);
  }
}; // attribute_store

//////////////////////////////////////////////////////////////////////////////
/// @brief Contains a pointer to an object of type `T`.
/// An adaptor for `attribute_map` container.
///
/// Can't use `std::unique_ptr<T, memory::noop_deleter>` becuase of
/// the bugs in MSVC2013-2015 related to move semantic in std::map
//////////////////////////////////////////////////////////////////////////////
template<typename T>
class pointer_wrapper {
 public:
  FORCE_INLINE pointer_wrapper(T* p = nullptr) noexcept : p_(p) { }
  FORCE_INLINE T* get() const noexcept { return p_; }
  FORCE_INLINE T* operator->() const noexcept { return get(); }
  FORCE_INLINE T& operator*() const noexcept { return *get(); }
  FORCE_INLINE pointer_wrapper& operator=(T* p) noexcept {
    p_ = p;
    return *this;
  }
  FORCE_INLINE operator bool() const noexcept {
    return nullptr != p_;
  }
  FORCE_INLINE bool operator==(std::nullptr_t) const noexcept {
    return nullptr == p_;
  }
  FORCE_INLINE bool operator!=(std::nullptr_t) const noexcept {
    return !(*this == nullptr);
  }

 private:
  T* p_;
}; // pointer_wrapper

template<typename T>
FORCE_INLINE bool operator==(
    std::nullptr_t,
    const pointer_wrapper<T>& rhs
) noexcept {
  return rhs == nullptr;
}

template<typename T>
FORCE_INLINE bool operator!=(
    std::nullptr_t,
    const pointer_wrapper<T>& rhs
) noexcept {
  return rhs != nullptr;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief storage of data pointers to attributes
//////////////////////////////////////////////////////////////////////////////

MSVC_ONLY(template class IRESEARCH_API irs::attribute_map<attribute, pointer_wrapper>;)

class IRESEARCH_API attribute_view
    : public attribute_map<attribute, pointer_wrapper> {
 public:
  typedef attribute_map<attribute, pointer_wrapper> base_t;

  static const attribute_view& empty_instance();

  explicit attribute_view(size_t reserve = 0);

  attribute_view(attribute_view&& rhs) noexcept
    : base_t(std::move(rhs)) {
  }

  attribute_view& operator=(attribute_view&& rhs) noexcept {
    base_t::operator=(std::move(rhs));
    return *this;
  }

  template<typename T>
  inline typename ref<T>::type& emplace() {
    return emplace_internal<T>();
  }

  template<typename T>
  inline typename ref<T>::type& emplace(T& value) {
    return emplace_internal(&value);
  }

 private:
  template<typename T>
  typename ref<T>::type& emplace_internal(T* value = nullptr) {
    REGISTER_TIMER_DETAILED();

    typedef typename std::enable_if<
      std::is_base_of<attribute, T>::value, T
    >::type type;

    bool inserted;
    auto& attr = attribute_map::emplace(inserted, irs::type<type>::id());

    if (inserted) {
      // reinterpret_cast to avoid layout related problems
      attr = reinterpret_cast<attribute*>(value);
    }

    return reinterpret_cast<typename ref<T>::type&>(attr);
  }
}; // attribute_view

NS_END

#endif
