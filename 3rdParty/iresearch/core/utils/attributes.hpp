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

#ifndef IRESEARCH_ATTRIBUTES_H
#define IRESEARCH_ATTRIBUTES_H

#include <set>

#include "memory.hpp"
#include "timer_utils.hpp"
#include "bit_utils.hpp"
#include "type_id.hpp"
#include "noncopyable.hpp"
#include "string.hpp"

namespace iresearch {

struct IRESEARCH_API attributes {
  static bool exists(
    const string_ref& name,
    bool load_library = true);

  static type_info get(
    const string_ref& name,
    bool load_library = true) noexcept;

  attributes() = delete;
};

//////////////////////////////////////////////////////////////////////////////
/// @class attribute
/// @brief base class for all attributes that can be used with attribute_map
///        an empty struct tag type with no virtual methods
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API attribute { };

//////////////////////////////////////////////////////////////////////////////
/// @brief base class for all attributes that can be deallocated via a ptr of
///        this struct type using a virtual destructor
///        an empty struct tag type with a virtual destructor
///        all derived classes must implement the following functions:
///        static const attribute::type_id type() noexcept
///          via DECLARE_ATTRIBUTE_TYPE()/DEFINE_ATTRIBUTE_TYPE(...)
///        static ptr make(Args&&... args)
///          via DECLARE_FACTORY()/DECLARE_FACTORY()
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API stored_attribute : attribute {
  using ptr = std::unique_ptr<stored_attribute>;

  stored_attribute() = default;
  stored_attribute(const stored_attribute&) = default;
  stored_attribute(stored_attribute&&) = default;
  stored_attribute& operator=(const stored_attribute&) = default;
  stored_attribute& operator=(stored_attribute&&) = default;

  virtual ~stored_attribute() = default;
};

static_assert(std::is_nothrow_move_constructible_v<stored_attribute>);
static_assert(std::is_nothrow_move_assignable_v<stored_attribute>);

// -----------------------------------------------------------------------------
// --SECTION--                                            Attribute registration
// -----------------------------------------------------------------------------

class IRESEARCH_API attribute_registrar {
 public:
  explicit attribute_registrar(
    const type_info& type,
    const char* source = nullptr);
  operator bool() const noexcept;

 private:
  bool registered_;
};

#define REGISTER_ATTRIBUTE__(attribute_name, line, source) \
  static ::iresearch::attribute_registrar attribute_registrar ## _ ## line(::iresearch::type<attribute_name>::get(), source)
#define REGISTER_ATTRIBUTE_EXPANDER__(attribute_name, file, line) REGISTER_ATTRIBUTE__(attribute_name, line, file ":" TOSTRING(line))
#define REGISTER_ATTRIBUTE(attribute_name) REGISTER_ATTRIBUTE_EXPANDER__(attribute_name, __FILE__, __LINE__)

}

#endif // IRESEARCH_ATTRIBUTES_H
