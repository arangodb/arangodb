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

#include <set>

#include "memory.hpp"
#include "timer_utils.hpp"
#include "bit_utils.hpp"
#include "type_id.hpp"
#include "noncopyable.hpp"
#include "string.hpp"

namespace iresearch {

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
  using ptr = std::unique_ptr<stored_attribute>;

  stored_attribute() = default;
  stored_attribute(const stored_attribute&) = default;
  stored_attribute(stored_attribute&&) = default;
  stored_attribute& operator=(const stored_attribute&) = default;
  stored_attribute& operator=(stored_attribute&&) = default;

  virtual ~stored_attribute() = default;
};

static_assert(std::is_nothrow_move_constructible_v<stored_attribute>);
static_assert(std::is_nothrow_move_assignable_v<stored_attribute>);

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

  static const flags& empty_instance() noexcept;

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

static_assert(std::is_move_constructible_v<flags>);
static_assert(std::is_move_assignable_v<flags>);

}

#endif // IRESEARCH_ATTRIBUTES_H
