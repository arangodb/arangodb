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

#include "shared.hpp"
#include "utils/register.hpp"
#include "attributes.hpp"

#include <cassert>

NS_LOCAL

class attribute_register:
  public iresearch::generic_register<iresearch::string_ref, const iresearch::attribute::type_id*, attribute_register> {
};

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                attribute::type_id
// -----------------------------------------------------------------------------

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
// --SECTION--                                                     attribute_map
// -----------------------------------------------------------------------------

#if defined(_MSC_VER) && defined(IRESEARCH_DLL)

  template class IRESEARCH_API attribute_map<attribute*>;
  template class IRESEARCH_API attribute_map<std::shared_ptr<stored_attribute>>;

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                   attribute_store
// -----------------------------------------------------------------------------

attribute_store::attribute_store(size_t /*reserve = 0*/) {
}

/*static*/ const attribute_store& attribute_store::empty_instance() {
  static attribute_store instance(0);
  return instance;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    attribute_view
// -----------------------------------------------------------------------------

attribute_view::attribute_view(size_t /*reserve = 0*/) {
}

/*static*/ const attribute_view& attribute_view::empty_instance() {
  static attribute_view instance(0);
  return instance;
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------