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

#include "token_attributes.hpp"

#include "shared.hpp"

namespace irs {
namespace {

struct EmptyPosition final : position {
  attribute* get_mutable(type_info::type_id /*type*/) noexcept final {
    return nullptr;
  }

  bool next() final { return false; }
};

EmptyPosition kNoPosition;

}  // namespace

position& position::empty() noexcept { return kNoPosition; }

REGISTER_ATTRIBUTE(frequency);
REGISTER_ATTRIBUTE(position);
REGISTER_ATTRIBUTE(offset);
REGISTER_ATTRIBUTE(payload);
REGISTER_ATTRIBUTE(irs::granularity_prefix);

}  // namespace irs
