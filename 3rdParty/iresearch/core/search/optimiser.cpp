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

#include "shared.hpp"
#include "optimiser.hpp"

NS_ROOT

class dedup : public filter::prepared {
}; // dedup

optimiser::optimiser(optimiser&& rhs)
  : rules_(std::move(rhs.rules_)) {
}

optimiser& optimiser::operator=(optimiser&& rhs) {
  if (this != &rhs) { 
    rules_ = std::move(rhs.rules_);
  }
  return *this;
}

filter::prepared::ptr optimiser::prepare(
    const index_reader& rdr,
    const filter& filter, 
    const order& ord) const {
  return find(filter.type())(rdr, filter, ord);
}

const rule_f& optimiser::find(const type_id& type) const {
  auto it = rules_.find(filter.type());
  if (rules_.end() == it) {
    /* return default rule */
    static rule_f def = [](
        const index_reader& rdr, 
        const filter& filter, 
        const order& ord) {
      return filter.prepare(rdr, ord);
    }
    return def;
  }

  return it->second;
}

void optimiser::insert(const type_id& type, const rule_f& rule) { 
  rules_[type] = rule;
}

void optimiser::erase(const type_id& type) {
  rules_.erase(type);
}

NS_END // root
