﻿////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_QUERY_H
#define IRESEARCH_QUERY_H

#include "sort.hpp"

#include <unordered_map>
#include <functional>

#include "shared.hpp"
#include "index/iterators.hpp"
#include "utils/hash_utils.hpp"

NS_ROOT

template<typename State>
class states_cache : private util::noncopyable {
 private:
  using states_map = std::unordered_map<const sub_reader*, State>;

 public:
  using state_type = State;

  explicit states_cache(size_t size) {
    states_.reserve(size);
  }

  states_cache(states_cache&& rhs)
      noexcept(std::is_nothrow_move_constructible_v<states_map>)
    : states_(std::move(rhs.states_)) {
  }

  states_cache& operator=(states_cache&& rhs)
      noexcept(std::is_nothrow_move_assignable_v<states_map>) {
    if (this != &rhs) {
      states_ = std::move(rhs.states_);
    }
    return *this;
  }

  State& insert(const sub_reader& rdr) {
    return states_[&rdr];
  }

  const State* find(const sub_reader& rdr) const noexcept {
    auto it = states_.find(&rdr);
    return states_.end() == it ? nullptr : &(it->second);
  }

  bool empty() const noexcept { return states_.empty(); }

private:
  // FIXME use vector instead?
  states_map states_;
}; // states_cache

struct index_reader;

////////////////////////////////////////////////////////////////////////////////
/// @class filter
/// @brief base class for all user-side filters
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API filter {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @class query
  /// @brief base class for all prepared(compiled) queries
  //////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API prepared {
   public:
    DECLARE_SHARED_PTR(const prepared);
    DEFINE_FACTORY_INLINE(prepared)

    static prepared::ptr empty();

    explicit prepared(boost_t boost = no_boost()) noexcept
      : boost_(boost) {
    }
    virtual ~prepared() = default;

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

    boost_t boost() const noexcept { return boost_; }

   protected:
    void boost(boost_t boost) noexcept { boost_ *= boost; }

   private:
    boost_t boost_;
  }; // prepared

  DECLARE_UNIQUE_PTR(filter);
  DEFINE_FACTORY_INLINE(filter)

  explicit filter(const type_id& type) noexcept;
  virtual ~filter() = default;

  virtual size_t hash() const noexcept {
    return std::hash<const type_id*>()(type_);
  }

  bool operator==(const filter& rhs) const noexcept {
    return equals(rhs);
  }

  bool operator!=(const filter& rhs) const noexcept {
    return !(*this == rhs);
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
    return prepare(rdr, ord, irs::no_boost(), ctx);
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
    return prepare(rdr, ord, irs::no_boost());
  }

  filter::prepared::ptr prepare(const index_reader& rdr) const {
    return prepare(rdr, order::prepared::unordered());
  }

  boost_t boost() const noexcept { return boost_; }

  filter& boost(boost_t boost) noexcept {
    boost_ = boost;
    return *this;
  }

  const type_id& type() const noexcept { return *type_; }

 protected:
  virtual bool equals(const filter& rhs) const noexcept {
    return type_ == rhs.type_;
  }

 private:
  boost_t boost_;
  const type_id* type_;
}; // filter

// boost::hash_combine support
inline size_t hash_value(const filter& q) noexcept {
  return q.hash();
}

////////////////////////////////////////////////////////////////////////////////
/// @class filter_with_options
/// @brief convenient base class filters with options
////////////////////////////////////////////////////////////////////////////////
template<typename Options>
class filter_with_options : public filter {
 public:
  using options_type = Options;
  using filter_type = typename options_type::filter_type;

  filter_with_options() : filter(filter_type::type()) { }

  const options_type& options() const noexcept { return options_; }
  options_type* mutable_options() noexcept { return &options_; }

  virtual size_t hash() const noexcept override {
    return hash_combine(filter::hash(), options_.hash());
  }

 protected:
  virtual bool equals(const filter& rhs) const noexcept override {
    return filter::equals(rhs) &&
      options_ ==
#ifdef IRESEARCH_DEBUG
        dynamic_cast<const filter_type&>(rhs).options_
#else
        static_cast<const filter_type&>(rhs).options_
#endif
      ;
  }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  options_type options_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // filter_with_options

////////////////////////////////////////////////////////////////////////////////
/// @class filter_base
/// @brief convenient base class for single field filters
////////////////////////////////////////////////////////////////////////////////
template<typename Options>
class filter_base : public filter_with_options<Options> {
 public:
  using options_type = typename filter_with_options<Options>::options_type;
  using filter_type = typename options_type::filter_type;

  const std::string& field() const noexcept { return field_; }
  std::string* mutable_field() noexcept { return &field_; }

  virtual size_t hash() const noexcept override {
    return hash_combine(hash_utils::hash(field_),
                        filter_with_options<options_type>::hash());
  }

 protected:
  virtual bool equals(const filter& rhs) const noexcept override {
    return filter_with_options<options_type>::equals(rhs) &&
      field_ ==
#ifdef IRESEARCH_DEBUG
        dynamic_cast<const filter_type&>(rhs).field_
#else
        static_cast<const filter_type&>(rhs).field_
#endif
      ;
  }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string field_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // filter_base

#define DECLARE_FILTER_TYPE() DECLARE_TYPE_ID(::iresearch::type_id)
#define DEFINE_FILTER_TYPE(class_name) DEFINE_TYPE_ID(class_name,::iresearch::type_id) { \
  static ::iresearch::type_id type; \
  return type; }

////////////////////////////////////////////////////////////////////////////////
/// @class empty
/// @brief filter which returns no documents
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
}; // empty

struct filter_visitor;
using field_visitor = std::function<void(const sub_reader&, const term_reader&, filter_visitor&)>;

NS_END

NS_BEGIN(std)

template<>
struct hash<iresearch::filter> {
  typedef iresearch::filter argument_type;
  typedef size_t result_type;

  result_type operator()(const argument_type& key) const {
    return key.hash();
  }
};

NS_END // std

#endif
