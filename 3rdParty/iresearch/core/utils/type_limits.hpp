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
struct term_id_t {};

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

template<> struct type_limits<type_t::field_id_t> {
  CONSTEXPR static field_id invalid() { 
    return integer_traits<field_id>::const_max;
  }
  CONSTEXPR static bool valid(field_id id) { return invalid() != id; }
};

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

template<> struct type_limits<type_t::term_id_t> {
  CONSTEXPR static bool valid(term_id id) { 
    return integer_traits<term_id>::const_max != id; 
  }
};

NS_END

#endif
