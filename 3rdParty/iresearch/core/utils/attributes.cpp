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

NS_LOCAL

class attribute_register:
  public iresearch::generic_register<iresearch::string_ref, const iresearch::attribute::type_id*, attribute_register> {
};

const iresearch::attribute_store EMPTY_ATTRIBUTE_STORE(0);
const iresearch::attribute_view  EMPTY_ATTRIBUTE_VIEW(0);

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                attribute::type_id
// -----------------------------------------------------------------------------

/*static*/ bool attribute::type_id::exists(const string_ref& name) {
  return attribute_register::instance().get(name);
}

/*static*/ const attribute::type_id* attribute::type_id::get(
    const string_ref& name) {
  return attribute_register::instance().get(name);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                             flags 
// -----------------------------------------------------------------------------

const flags& flags::empty_instance() {
  static flags instance;
  return instance;
}

flags::flags() { }

flags::flags(flags&& rhs) NOEXCEPT
  : map_(std::move(rhs.map_)) {
}

flags& flags::operator=(flags&& rhs) NOEXCEPT {
  if (this != &rhs) {
    map_ = std::move(rhs.map_);
  }

  return *this;
}

flags::flags(std::initializer_list<const attribute::type_id*> flags) {
  std::for_each( 
    flags.begin(), flags.end(), 
    [this](const attribute::type_id* type) {
      add(*type);
  } );
}

flags& flags::operator=(std::initializer_list<const attribute::type_id*> flags) {
  map_.clear();
  std::for_each( 
    flags.begin(), flags.end(), 
    [this](const attribute::type_id* type) {
      add(*type);
  });
  return *this;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            attribute registration
// -----------------------------------------------------------------------------

attribute_registrar::attribute_registrar(
    const attribute::type_id& type,
    const char* source /*= nullptr*/
) {
  auto entry = attribute_register::instance().set(type.name(), &type);

  registered_ = entry.second;

  if (!registered_ && &type != entry.first) {
    if (source) {
      IR_FRMT_WARN(
        "type name collision detected while registering attribute, ignoring: type '%s' from %s",
        type.name().c_str(),
        source
      );
    } else {
      IR_FRMT_WARN(
        "type name collision detected while registering attribute, ignoring: type '%s'",
        type.name().c_str()
      );
    }

    IR_STACK_TRACE();
  }
}

attribute_registrar::operator bool() const NOEXCEPT {
  return registered_;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   attribute_store
// -----------------------------------------------------------------------------

attribute_store::attribute_store(size_t /*reserve = 0*/) {
}

/*static*/ const attribute_store& attribute_store::empty_instance() {  
  return EMPTY_ATTRIBUTE_STORE;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    attribute_view
// -----------------------------------------------------------------------------

attribute_view::attribute_view(size_t /*reserve = 0*/) {
}

/*static*/ const attribute_view& attribute_view::empty_instance() {  
  return EMPTY_ATTRIBUTE_VIEW;
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------