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

#include "analysis/token_stream.hpp"
#include "utils/type_info.hpp"

namespace irs::analysis {

class analyzer : public token_stream {
 public:
  using ptr = std::unique_ptr<analyzer>;

  virtual bool reset(std::string_view data) = 0;

  virtual irs::type_info::type_id type() const noexcept = 0;
};

template<typename Impl>
class TypedAnalyzer : public analyzer {
 public:
  irs::type_info::type_id type() const noexcept final {
    return irs::type<Impl>::id();
  }
};

class empty_analyzer final : public TypedAnalyzer<empty_analyzer> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "empty_analyzer";
  }

  attribute* get_mutable(irs::type_info::type_id) final { return nullptr; }

  bool next() final { return false; }

  bool reset(std::string_view) final { return false; }
};

}  // namespace irs::analysis
