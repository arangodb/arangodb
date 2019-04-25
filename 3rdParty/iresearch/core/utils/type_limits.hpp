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

#include "integer.hpp"
#include "shared.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
// type identifiers for use with type_limits
// ----------------------------------------------------------------------------

NS_BEGIN(type_t)

struct address_t {};
struct doc_id_t {};
struct field_id_t {};
struct index_gen_t {};
struct pos_t {};

NS_END // type_t

// ----------------------------------------------------------------------------
// type limits/boundaries
// ----------------------------------------------------------------------------

template<typename TYPE> struct type_limits;

template<> struct type_limits<type_t::address_t> {
  CONSTEXPR static uint64_t invalid() {
    return integer_traits<uint64_t>::const_max;
  }
  CONSTEXPR static bool valid(uint64_t addr) { return invalid() != addr; }
};

template<> struct type_limits<type_t::doc_id_t> {
  CONSTEXPR static doc_id_t eof() {
    return integer_traits<doc_id_t>::const_max;
  }
  CONSTEXPR static bool eof(doc_id_t id) { return eof() == id; }
  CONSTEXPR static doc_id_t invalid() { return 0; }
  CONSTEXPR static doc_id_t (min)() {
    return 1; // +1 because INVALID_DOC == 0
  }
  CONSTEXPR static bool valid(doc_id_t id) { return invalid() != id; }
};

typedef irs::type_limits<irs::type_t::doc_id_t> doc_limits;

template<> struct type_limits<type_t::field_id_t> {
  CONSTEXPR static field_id invalid() { 
    return integer_traits<field_id>::const_max;
  }
  CONSTEXPR static bool valid(field_id id) { return invalid() != id; }
};

typedef irs::type_limits<irs::type_t::field_id_t> field_limits;

template<> struct type_limits<type_t::index_gen_t> {
  CONSTEXPR static uint64_t invalid() {
    return integer_traits<field_id>::const_max;
  }
  CONSTEXPR static bool valid(uint64_t id) { return invalid() != id; }
};

template<> struct type_limits<type_t::pos_t> {
  CONSTEXPR static uint32_t invalid() { 
    return integer_traits<uint32_t>::const_max; 
  }
  CONSTEXPR static bool valid(uint32_t pos) { return invalid() != pos; }
  CONSTEXPR static uint32_t eof() { return invalid() - 1; }
  CONSTEXPR static bool eof(uint32_t pos) { return eof() == pos; }
  CONSTEXPR static uint32_t (min)() { return 0; }
};

typedef irs::type_limits<irs::type_t::pos_t> pos_limits;

NS_END

#endif
