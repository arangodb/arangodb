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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "search/all_docs_provider.hpp"
#include "search/filter.hpp"
#include "utils/iterator.hpp"

namespace irs {

// Represents user-side boolean filter as the container for other filters
class boolean_filter : public FilterWithBoost, public AllDocsProvider {
 public:
  auto begin() const { return filters_.begin(); }
  auto end() const { return filters_.end(); }

  ScoreMergeType merge_type() const noexcept { return merge_type_; }

  void merge_type(ScoreMergeType merge_type) noexcept {
    merge_type_ = merge_type;
  }

  template<typename T, typename... Args>
  T& add(Args&&... args) {
    return add(std::make_unique<T>(std::forward<Args>(args)...));
  }

  template<typename T>
  T& add(std::unique_ptr<T>&& filter) {
    IRS_ASSERT(filter);
    return DownCast<T>(*filters_.emplace_back(std::move(filter)));
  }

  void clear() { filters_.clear(); }
  bool empty() const { return filters_.empty(); }
  size_t size() const { return filters_.size(); }

  prepared::ptr prepare(const PrepareContext& ctx) const override;

 protected:
  bool equals(const filter& rhs) const noexcept final;

  virtual prepared::ptr PrepareBoolean(std::vector<const filter*>& incl,
                                       std::vector<const filter*>& excl,
                                       const PrepareContext& ctx) const = 0;

 private:
  void group_filters(AllDocsProvider::Ptr& all_docs_zero_boost,
                     std::vector<const filter*>& incl,
                     std::vector<const filter*>& excl) const;

  std::vector<filter::ptr> filters_;
  ScoreMergeType merge_type_{ScoreMergeType::kSum};
};

// Represents conjunction
class And final : public boolean_filter {
 public:
  type_info::type_id type() const noexcept final {
    return irs::type<And>::id();
  }

 protected:
  prepared::ptr PrepareBoolean(std::vector<const filter*>& incl,
                               std::vector<const filter*>& excl,
                               const PrepareContext& ctx) const final;
};

// Represents disjunction
class Or final : public boolean_filter {
 public:
  // Return minimum number of subqueries which must be satisfied
  size_t min_match_count() const { return min_match_count_; }

  // Sets minimum number of subqueries which must be satisfied
  Or& min_match_count(size_t count) {
    min_match_count_ = count;
    return *this;
  }

  prepared::ptr prepare(const PrepareContext& ctx) const final;

  type_info::type_id type() const noexcept final { return irs::type<Or>::id(); }

 protected:
  prepared::ptr PrepareBoolean(std::vector<const filter*>& incl,
                               std::vector<const filter*>& excl,
                               const PrepareContext& ctx) const final;

 private:
  size_t min_match_count_{1};
};

// Represents negation
class Not : public FilterWithType<Not>, public AllDocsProvider {
 public:
  const filter* filter() const { return filter_.get(); }

  template<typename T>
  const T* filter() const {
    return DownCast<T>(filter_.get());
  }

  template<typename T, typename... Args>
  T& filter(Args&&... args) {
    static_assert(std::is_base_of_v<irs::filter, T>);
    filter_ = std::make_unique<T>(std::forward<Args>(args)...);
    return DownCast<T>(*filter_);
  }

  void clear() { filter_.reset(); }
  bool empty() const { return nullptr == filter_; }

  prepared::ptr prepare(const PrepareContext& ctx) const final;

 protected:
  bool equals(const irs::filter& rhs) const noexcept final;

 private:
  filter::ptr filter_;
};

}  // namespace irs
