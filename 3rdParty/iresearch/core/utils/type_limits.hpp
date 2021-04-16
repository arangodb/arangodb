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

#ifndef IRESEARCH_TYPE_LIMITS_H
#define IRESEARCH_TYPE_LIMITS_H

#include "shared.hpp"

namespace iresearch {

// ----------------------------------------------------------------------------
// type identifiers for use with type_limits
// ----------------------------------------------------------------------------

namespace type_t {

struct address_t {};
struct doc_id_t {};
struct field_id_t {};
struct index_gen_t {};
struct pos_t {};

} // type_t

// ----------------------------------------------------------------------------
// type limits/boundaries
// ----------------------------------------------------------------------------

template<typename TYPE> struct type_limits;

template<> struct type_limits<type_t::address_t> {
  constexpr static uint64_t invalid() noexcept {
    return std::numeric_limits<uint64_t>::max();
  }
  constexpr static bool valid(uint64_t addr) noexcept {
    return invalid() != addr;
  }
};

template<> struct type_limits<type_t::doc_id_t> {
  constexpr static doc_id_t eof() noexcept {
    return std::numeric_limits<doc_id_t>::max();
  }
  constexpr static bool eof(doc_id_t id) noexcept {
    return eof() == id;
  }
  constexpr static doc_id_t invalid() noexcept { return 0; }
  constexpr static doc_id_t (min)() noexcept {
    return 1; // +1 because INVALID_DOC == 0
  }
  constexpr static bool valid(doc_id_t id) noexcept {
    return invalid() != id;
  }
};

typedef irs::type_limits<irs::type_t::doc_id_t> doc_limits;

template<> struct type_limits<type_t::field_id_t> {
  constexpr static field_id invalid() noexcept {
    return std::numeric_limits<field_id>::max();
  }
  constexpr static bool valid(field_id id) noexcept {
    return invalid() != id;
  }
};

typedef irs::type_limits<irs::type_t::field_id_t> field_limits;

template<> struct type_limits<type_t::index_gen_t> {
  constexpr static uint64_t invalid() noexcept {
    return std::numeric_limits<field_id>::max();
  }
  constexpr static bool valid(uint64_t id) noexcept {
    return invalid() != id;
  }
};

template<> struct type_limits<type_t::pos_t> {
  constexpr static uint32_t invalid() noexcept {
    return 0;
  }
  constexpr static bool valid(uint32_t pos) noexcept {
    return invalid() != pos;
  }
  constexpr static uint32_t eof() noexcept { return std::numeric_limits<uint32_t>::max(); }
  constexpr static bool eof(uint32_t pos) noexcept { return eof() == pos; }
  constexpr static uint32_t (min)() noexcept { return 1; }
};

typedef irs::type_limits<irs::type_t::pos_t> pos_limits;

}

#endif
