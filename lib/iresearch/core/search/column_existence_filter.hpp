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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "filter.hpp"
#include "utils/string.hpp"

namespace irs {

class by_column_existence;

using ColumnAcceptor = bool (*)(std::string_view prefix, std::string_view name);

// Options for column existence filter
struct by_column_existence_options {
  using filter_type = by_column_existence;

  // If set approves column matched the specified prefix
  ColumnAcceptor acceptor{};

  bool operator==(const by_column_existence_options& rhs) const noexcept {
    return acceptor == rhs.acceptor;
  }
};

// User-side column existence filter
class by_column_existence final
  : public FilterWithField<by_column_existence_options> {
 public:
  prepared::ptr prepare(const PrepareContext& ctx) const final;
};

}  // namespace irs
