////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/hash_table.hpp"
#include "analysis/token_attributes.hpp"

TEST(hash_set_test, insert_find) {
  iresearch::timer_utils::init_stats(true);
  std::vector<const irs::attribute::type_id*> source {
    irs::term_attribute::type(),
    irs::document::type(),
    irs::frequency::type(),
    irs::position::type(),
    irs::granularity_prefix::type(),
    irs::norm::type(),
    irs::increment::type(),
    irs::offset::type()
  };

  struct myhash {
    size_t operator()(const irs::attribute::type_id* t) const {
      return t->hash;
    }
  };


  for (auto i = 0; i < 100;  ++i) {


  std::sort(source.begin(), source.end());
  irs::hash_set<const irs::attribute::type_id*, myhash> set(17);
  std::set<const irs::attribute::type_id*> stdset;
  std::unordered_set<const irs::attribute::type_id*, myhash> stduset(source.size());
  irs::flags features;

  for (auto& e : source) {
    SCOPED_TIMER("std_set::insert");
    stdset.insert(*e);
  }

  for (auto& e : source) {
    SCOPED_TIMER("std_uset::insert");
    stdset.insert(*e);
  }

  for (auto& e : source) {
    SCOPED_TIMER("hash_set::insert");
    set.insert(e);
  }

  for (auto& e : source) {
    SCOPED_TIMER("features::insert");
    features.add(*e);
  }

  for (auto& e : source) {
    SCOPED_TIMER("std_set::find");
    ASSERT_TRUE(stdset.end() != stdset.find(*e));
  }

  for (auto& e : source) {
    SCOPED_TIMER("std_uset::find");
    ASSERT_TRUE(stdset.end() != stdset.find(*e));
  }

  for (auto& e : source) {
    SCOPED_TIMER("hash_set::find");
    ASSERT_TRUE(set.contains(*e));
  }

  for (auto& e : source) {
    SCOPED_TIMER("sorted_array::insert");
    ASSERT_TRUE(source.end() != std::lower_bound(source.begin(), source.end(), *e));
  }

  for (auto& e : source) {
    SCOPED_TIMER("sorted_array::linear_search");
    ASSERT_TRUE(source.end() != std::find(source.begin(), source.end(), *e));
  }

  for (auto& e : source) {
    SCOPED_TIMER("features::find");
    ASSERT_TRUE(features.check(*e));
  }

  }


  flush_timers(std::cerr);

}
