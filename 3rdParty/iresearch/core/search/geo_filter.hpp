////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_GEO_FILTER_H
#define IRESEARCH_GEO_FILTER_H

#include <s2/s2region_term_indexer.h>

#include "filter.hpp"

NS_ROOT

class by_geo_terms;

enum class GeoFilterType : uint32_t {
  INTERSECTS = 0,
  CONTAINS
};

////////////////////////////////////////////////////////////////////////////////
/// @struct by_term_options
/// @brief options for term filter
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_geo_terms_options {
 public:
  using filter_type = by_geo_terms;

  void reset(GeoFilterType type, const S2Region& region);
  void reset(const S2Point& point);

  const std::vector<std::string> terms() const noexcept {
    return terms_;
  }

  GeoFilterType type() const noexcept {
    return type_;
  }

  const std::string& prefix() const noexcept {
    return prefix_;
  }

  std::string* mutable_prefix() noexcept {
    return &prefix_;
  }

  const S2RegionTermIndexer::Options& options() const noexcept {
    return indexer_.options();
  }

  S2RegionTermIndexer::Options* mutable_options() noexcept {
    return indexer_.mutable_options();
  }

  bool operator==(const by_geo_terms_options& rhs) const noexcept {
    return type_ == rhs.type_ && terms_ == rhs.terms_;
  }

  size_t hash() const noexcept {
    size_t hash = std::hash<decltype(type_)>()(type_);
    for (auto& term : terms_) {
      hash = hash_combine(hash, hash_utils::hash(string_ref(term)));
    }
    return hash;
  }

 private:
  std::vector<std::string> terms_;
  std::string prefix_;
  mutable S2RegionTermIndexer indexer_;
  GeoFilterType type_{GeoFilterType::INTERSECTS};
}; // by_geo_terms_options

//////////////////////////////////////////////////////////////////////////////
/// @class by_geo_distance
/// @brief user-side geo distance filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_geo_terms final
  : public filter_base<by_geo_terms_options>{
 public:
  static constexpr string_ref type_name() noexcept {
    return "iresearch::by_geo_terms";
  }

  DECLARE_FACTORY();

  using filter::prepare;

  virtual prepared::ptr prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost,
      const attribute_provider* /*ctx*/) const;
}; // by_geo_terms

NS_END

#endif // IRESEARCH_GEO_FILTER_H
