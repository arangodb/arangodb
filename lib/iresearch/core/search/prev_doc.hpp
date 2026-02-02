////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include "utils/attribute_provider.hpp"

namespace irs {

// Provides an access to previous document before the current one.
// Undefined after iterator reached EOF.
class prev_doc : public attribute {
 public:
  using prev_doc_f = doc_id_t (*)(const void*);

  static constexpr std::string_view type_name() noexcept { return "prev_doc"; }

  constexpr explicit operator bool() const noexcept { return nullptr != func_; }

  constexpr bool operator==(std::nullptr_t) const noexcept {
    return !static_cast<bool>(*this);
  }

  doc_id_t operator()() const {
    IRS_ASSERT(static_cast<bool>(*this));
    return func_(ctx_);
  }

  void reset(prev_doc_f func, void* ctx) noexcept {
    func_ = func;
    ctx_ = ctx;
  }

 private:
  prev_doc_f func_{};
  void* ctx_{};
};

}  // namespace irs
