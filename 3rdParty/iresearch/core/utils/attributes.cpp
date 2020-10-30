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

#include "shared.hpp"
#include "utils/register.hpp"
#include "attributes.hpp"

#include <cassert>

namespace {

class attribute_register
    : public irs::tagged_generic_register<irs::string_ref, irs::type_info,
                                          irs::string_ref, attribute_register> {
};


static const irs::flags EMPTY_INSTANCE;

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                        attributes
// -----------------------------------------------------------------------------

/*static*/ bool attributes::exists(
    const string_ref& name,
    bool load_library /*= true*/) {
  return static_cast<bool>(attribute_register::instance().get(name, load_library));
}

/*static*/ type_info attributes::get(
    const string_ref& name,
    bool load_library /*= true*/) noexcept {
  try {
    return attribute_register::instance().get(name, load_library);
  } catch (...) {
    IR_FRMT_ERROR("Caught exception while getting an attribute instance");
    IR_LOG_EXCEPTION();
  }

  return {}; // invalid type id
}

// -----------------------------------------------------------------------------
// --SECTION--                                                             flags 
// -----------------------------------------------------------------------------

const flags& flags::empty_instance() noexcept {
  return EMPTY_INSTANCE;
}

flags::flags(std::initializer_list<type_info> flags) {
  std::for_each( 
    flags.begin(), flags.end(), 
    [this](const type_info& type) {
      add(type.id());
  } );
}

flags& flags::operator=(std::initializer_list<type_info> flags) {
  map_.clear();
  std::for_each( 
    flags.begin(), flags.end(), 
    [this](const type_info& type) {
      add(type.id());
  });
  return *this;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            attribute registration
// -----------------------------------------------------------------------------

attribute_registrar::attribute_registrar(
    const type_info& type,
    const char* source /*= nullptr*/) {
  irs::string_ref source_ref(source);
  auto entry = attribute_register::instance().set(
    type.name(),
    type,
    source_ref.null() ? nullptr : &source_ref);

  registered_ = entry.second;

  if (!registered_ && type != entry.first) {
    auto* registered_source = attribute_register::instance().tag(type.name());

    if (source && registered_source) {
      IR_FRMT_WARN(
        "type name collision detected while registering attribute, ignoring: type '%s' from %s, previously from %s",
        type.name().c_str(),
        source,
        registered_source->c_str()
      );
    } else if (source) {
      IR_FRMT_WARN(
        "type name collision detected while registering attribute, ignoring: type '%s' from %s",
        type.name().c_str(),
        source
      );
    } else if (registered_source) {
      IR_FRMT_WARN(
        "type name collision detected while registering attribute, ignoring: type '%s', previously from %s",
        type.name().c_str(),
        registered_source->c_str()
      );
    } else {
      IR_FRMT_WARN(
        "type name collision detected while registering attribute, ignoring: type '%s'",
        type.name().c_str()
      );
    }
  }
}

attribute_registrar::operator bool() const noexcept {
  return registered_;
}

}
