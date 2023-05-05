////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <analysis/analyzer.hpp>
#include <analysis/token_attributes.hpp>
#include <utils/attributes.hpp>

struct TestAttribute : public irs::attribute {
  static constexpr std::string_view type_name() noexcept {
    return "TestAttribute";
  }
};

class TestAnalyzer final : public irs::analysis::TypedAnalyzer<TestAnalyzer> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "TestAnalyzer";
  }

  TestAnalyzer();

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final;

  static ptr make(std::string_view args);

  static bool normalize(std::string_view args, std::string& definition);

  bool next() final;

  bool reset(std::string_view data) final;

 private:
  irs::bytes_view _data;
  irs::increment _increment{};
  irs::term_attribute _term{};
  TestAttribute _attr;
};
