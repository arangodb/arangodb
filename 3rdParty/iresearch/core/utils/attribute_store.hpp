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

#ifndef IRESEARCH_ATTRIBUTE_STORE_H
#define IRESEARCH_ATTRIBUTE_STORE_H

#include <map>

#include "shared.hpp"
#include "utils/attributes.hpp"

namespace iresearch {

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

static_assert(std::is_move_constructible<attribute_store>::value,
              "default move constructor expected");

}

#endif // IRESEARCH_ATTRIBUTE_STORE_H
