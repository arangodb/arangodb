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

#ifndef IRESEARCH_QUERY_H
#define IRESEARCH_QUERY_H

#include "shared.hpp"
#include "sort.hpp"
#include "index/iterators.hpp"

#include <unordered_map>
#include <functional>

NS_ROOT

template<typename State>
class states_cache {
public:
  explicit states_cache(size_t size) {
    states_.reserve(size);
  }

  states_cache(states_cache&& rhs) NOEXCEPT
    : states_(std::move(rhs.states_)) {
  }

  states_cache& operator=(states_cache&& rhs) NOEXCEPT {
    if (this != &rhs) {
      states_ = std::move(rhs.states_);
    }
    return *this;
  }

  State& insert(const sub_reader& rdr) {
    auto it = states_.emplace(&rdr, State()).first;
    return it->second;    
  }

  const State* find(const sub_reader& rdr) const {
    auto it = states_.find(&rdr);
    return states_.end() == it ? nullptr : &(it->second);
  }

  bool empty() const { return states_.empty(); }

private:
  typedef std::unordered_map<
    const sub_reader*, State
  > states_map_t;

  states_map_t states_;
}; // states_cache

struct index_reader;

////////////////////////////////////////////////////////////////////////////////
/// @class filter
/// @brief base class for all user-side filters
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API filter {
 public:
  typedef iresearch::boost::boost_t boost_t;

  //////////////////////////////////////////////////////////////////////////////
  /// @class query
  /// @brief base class for all prepared(compiled) queries
  //////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API prepared: public util::attribute_store_provider {
   public:
    DECLARE_SHARED_PTR(const prepared);
    DEFINE_FACTORY_INLINE(prepared);

    static prepared::ptr empty();

    prepared() = default;
    explicit prepared(attribute_store&& attrs);
    virtual ~prepared();

    using util::attribute_store_provider::attributes;
    virtual attribute_store& attributes() NOEXCEPT override final {
      return attrs_;
    }

    doc_iterator::ptr execute(
        const sub_reader& rdr) const {
      return execute(rdr, order::prepared::unordered());
    }

    doc_iterator::ptr execute(
        const sub_reader& rdr,
        const order::prepared& ord) const {
      return execute(rdr, ord, attribute_view::empty_instance());
    }

    virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_view& ctx
    ) const = 0;

   protected:
    friend class filter;

   private:
    attribute_store attrs_;
  }; // prepared

  DECLARE_UNIQUE_PTR(filter);
  DEFINE_FACTORY_INLINE(filter);

  filter(const type_id& type) NOEXCEPT;
  virtual ~filter();

  virtual size_t hash() const NOEXCEPT {
    return std::hash<const type_id*>()(type_);
  }

  bool operator==(const filter& rhs) const NOEXCEPT {
    return equals(rhs);
  }

  bool operator!=(const filter& rhs) const NOEXCEPT {
    return !(*this == rhs);
  }

  // boost::hash_combile support
  friend size_t hash_value(const filter& q) NOEXCEPT {
    return q.hash();
  }

  // boost - external boost
  virtual filter::prepared::ptr prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost,
      const attribute_view& ctx
  ) const = 0;

  filter::prepared::ptr prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      const attribute_view& ctx) const {
    return prepare(rdr, ord, irs::boost::no_boost(), ctx);
  }

  filter::prepared::ptr prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost) const {
    return prepare(rdr, ord, boost, attribute_view::empty_instance());
  }

  filter::prepared::ptr prepare(
      const index_reader& rdr,
      const order::prepared& ord) const {
    return prepare(rdr, ord, boost::no_boost());
  }

  filter::prepared::ptr prepare(const index_reader& rdr) const {
    return prepare(rdr, order::prepared::unordered());
  }

  boost_t boost() const NOEXCEPT { return boost_; }

  filter& boost(boost_t boost) NOEXCEPT {
    boost_ = boost;
    return *this;
  }

  const type_id& type() const NOEXCEPT { return *type_; }

 protected:
  virtual bool equals(const filter& rhs) const NOEXCEPT {
    return type_ == rhs.type_;
  }

 private:
  boost_t boost_;
  const type_id* type_;
}; // filter

#define DECLARE_FILTER_TYPE() DECLARE_TYPE_ID(::iresearch::type_id)
#define DEFINE_FILTER_TYPE(class_name) DEFINE_TYPE_ID(class_name,::iresearch::type_id) { \
  static ::iresearch::type_id type; \
  return type; }

////////////////////////////////////////////////////////////////////////////////
/// @class empty
/// @brief filter that returns no documents
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API empty: public filter {
 public:
  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY();

  empty();

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& ctx
  ) const override;
};

NS_END

NS_BEGIN( std )

template<>
struct hash<iresearch::filter> {
  typedef iresearch::filter argument_type;
  typedef size_t result_type;

  result_type operator()( const argument_type& key) const {
    return key.hash();
  }
};

NS_END // std

#endif
