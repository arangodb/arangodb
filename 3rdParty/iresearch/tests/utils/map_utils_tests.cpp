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

#include <map>

#include "tests_shared.hpp"
#include "utils/hash_utils.hpp"
#include "utils/map_utils.hpp"

TEST(map_utils_tests, try_emplace_update_key) {
  struct key_updater_t {
    irs::hashed_string_ref operator()(const irs::hashed_string_ref& key, const std::string& value) const {
      return irs::hashed_string_ref(key.hash(), value);
    }
  } updater;

  // move constructed
  {
    std::map<irs::hashed_string_ref, std::string> map;

    // new element
    {
      auto key1 = irs::make_hashed_ref(irs::string_ref("abc"), std::hash<irs::string_ref>());
      irs::map_utils::try_emplace_update_key(map, updater, std::move(key1), "abc");
      ASSERT_EQ(1, map.size());
      auto key2 = irs::make_hashed_ref(irs::string_ref("def"), std::hash<irs::string_ref>());
      irs::map_utils::try_emplace_update_key(map, updater, std::move(key2), "def");
      ASSERT_EQ(2, map.size());

      for (auto& entry: map) {
        ASSERT_EQ(entry.first.c_str(), entry.second.c_str());
      }
    }

    // existing element
    {
      auto key1 = irs::make_hashed_ref(irs::string_ref("abc"), std::hash<irs::string_ref>());
      irs::map_utils::try_emplace_update_key(map, updater, std::move(key1), "ghi");
      ASSERT_EQ(2, map.size());

      for (auto& entry: map) {
        ASSERT_EQ(entry.first.c_str(), entry.second.c_str());
      }
    }
  }
  
  // copy constructed
  {
    std::map<irs::hashed_string_ref, std::string> map;

    // new element
    {
      auto key1 = irs::make_hashed_ref(irs::string_ref("abc"), std::hash<irs::string_ref>());
      irs::map_utils::try_emplace_update_key(map, updater, key1, "abc");
      ASSERT_EQ(1, map.size());
      auto key2 = irs::make_hashed_ref(irs::string_ref("def"), std::hash<irs::string_ref>());
      irs::map_utils::try_emplace_update_key(map, updater, key2, "def");
      ASSERT_EQ(2, map.size());

      for (auto& entry: map) {
        ASSERT_EQ(entry.first.c_str(), entry.second.c_str());
      }
    }

    // existing element
    {
      auto key1 = irs::make_hashed_ref(irs::string_ref("abc"), std::hash<irs::string_ref>());
      irs::map_utils::try_emplace_update_key(map, updater, key1, "ghi");
      ASSERT_EQ(2, map.size());

      for (auto& entry: map) {
        ASSERT_EQ(entry.first.c_str(), entry.second.c_str());
      }
    }
  }
}
