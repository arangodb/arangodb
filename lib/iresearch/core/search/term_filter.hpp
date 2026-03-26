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

#include "search/filter.hpp"
#include "utils/string.hpp"

namespace irs {

class by_term;
struct filter_visitor;

// Options for term filter
struct by_term_options {
  using filter_type = by_term;

  bstring term;

  bool operator==(const by_term_options& rhs) const noexcept {
    return term == rhs.term;
  }
};

// User-side term filter
class by_term : public FilterWithField<by_term_options> {
 public:
  static prepared::ptr prepare(const PrepareContext& ctx,
                               std::string_view field, bytes_view term);

  static void visit(const SubReader& segment, const term_reader& field,
                    bytes_view term, filter_visitor& visitor);

  prepared::ptr prepare(const PrepareContext& ctx) const final {
    auto sub_ctx = ctx;
    sub_ctx.boost *= boost();
    return prepare(sub_ctx, field(), options().term);
  }
};

}  // namespace irs
