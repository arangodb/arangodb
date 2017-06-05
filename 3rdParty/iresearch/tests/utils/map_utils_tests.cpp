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

#include <map>

#include "tests_shared.hpp"
#include "utils/hash_utils.hpp"
#include "utils/map_utils.hpp"

TEST(map_utils_tests, try_empalace) {
  static size_t key_count;
  struct value_t {
    value_t() { ++key_count; }
  };

  std::map<std::string, value_t> map;
  key_count = 0;

  // new element (move constructed)
  {
    irs::map_utils::try_emplace(map, "abc");
    ASSERT_EQ(1, key_count);
    ASSERT_EQ(1, map.size());
    irs::map_utils::try_emplace(map, "def");
    ASSERT_EQ(2, key_count);
    ASSERT_EQ(2, map.size());
  }

  // existing element (move constructed)
  {
    irs::map_utils::try_emplace(map, "abc");
    ASSERT_EQ(2, key_count);
    ASSERT_EQ(2, map.size());
  }

  map.clear();
  key_count = 0;

  // new element (copy constructed)
  {
    std::string key1 = "abc";
    irs::map_utils::try_emplace(map, key1);
    ASSERT_EQ(1, key_count);
    ASSERT_EQ(1, map.size());
    std::string key2 = "def";
    irs::map_utils::try_emplace(map, key2);
    ASSERT_EQ(2, key_count);
    ASSERT_EQ(2, map.size());
  }

  // existing element (copy constructed)
  {
    std::string key1 = "abc";
    irs::map_utils::try_emplace(map, key1);
    ASSERT_EQ(2, key_count);
    ASSERT_EQ(2, map.size());
  }
}

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
