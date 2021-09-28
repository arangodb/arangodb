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

#include "shared.hpp"
#include "token_attributes.hpp"

namespace {

struct empty_position final : irs::position {
  virtual void reset() override { }
  virtual bool next() override { return false; }
  virtual attribute* get_mutable(irs::type_info::type_id) noexcept override {
    return nullptr;
  }
};

empty_position NO_POSITION;

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                          position
// -----------------------------------------------------------------------------

/*static*/ irs::position* position::empty() noexcept { return &NO_POSITION; }

// -----------------------------------------------------------------------------
// --SECTION--                                            attribute registration
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// !!! DO NOT MODIFY value in DEFINE_ATTRIBUTE_TYPE(...) as it may break
/// already created indexes !!!
////////////////////////////////////////////////////////////////////////////////

REGISTER_ATTRIBUTE(frequency);
REGISTER_ATTRIBUTE(position);
REGISTER_ATTRIBUTE(offset);
REGISTER_ATTRIBUTE(payload);
REGISTER_ATTRIBUTE(iresearch::granularity_prefix);

}
