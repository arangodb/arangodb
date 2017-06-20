//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_OPTIMISER_H
#define IRESEARCH_OPTIMISER_H

#include "filter.hpp"
#include "noncopyable.hpp"

#include <functional>

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class optimiser 
/// @brief prepare query according to the optimisation rules 
//////////////////////////////////////////////////////////////////////////////
class optimiser : util::noncopyable {
 public:
  typedef std::function<
    const index_reader&, 
    const filter&, 
    const order&
  > rule_f;

  optimiser() = default;
  optimiser(optimiser&& rhs) NOEXCEPT;
  optimiser& operator=(optimiser&& rhs) NOEXCEPT;

  filter::prepared::ptr prepare(
    const index_reader& rdr, 
    const filter& filter, 
    const order& ord) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief inserts rule for optimising a particular query type 
  //////////////////////////////////////////////////////////////////////////////
  void insert(const type_id& type, const rule_f& rule);

  //////////////////////////////////////////////////////////////////////////////
  /// @returns rule for optimising the specified type of the query.
  //////////////////////////////////////////////////////////////////////////////
  const rule_f& find(const type_id& type) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes the optimisation rule for the specified type of the query
  //////////////////////////////////////////////////////////////////////////////
  void erase(const type_id& type);

 private:
  typedef std::unordered_map<
    const iresearch::type_id*, 
    rule_f
  > rules_map_t;

  rules_map_t rules_;
};

NS_END // ROOT

#endif // IRESEARCH_OPTIMISER_H
