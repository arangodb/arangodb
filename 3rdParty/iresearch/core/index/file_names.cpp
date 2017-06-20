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
#include "file_names.hpp"
#include <cinttypes>

#if defined(_MSC_VER)
  #pragma warning (disable : 4996)
#endif

NS_ROOT

std::string file_name(uint64_t gen) {
  char buf[22] = "_"; // can hold : -2^63 .. 2^64-1, plus '_' prefix, plus 0
  return std::string(buf, 1 + std::sprintf(buf + 1, "%" PRIu64, gen));
}

std::string file_name(const string_ref& prefix, uint64_t gen) {
  char buf[21]; // can hold : -2^63 .. 2^64-1, plus 0
  auto buf_size = sprintf(buf, "%" PRIu64, gen);

  std::string str;
  str.reserve(buf_size + prefix.size());
  str.append(prefix.c_str(), prefix.size());
  str.append(buf, sprintf(buf, "%" PRIu64, gen));

  return str;
}

std::string file_name(const string_ref& name, const string_ref& ext) {
  std::string out;
  out.reserve(1 + name.size() + ext.size());
  out.append(name.c_str(), name.size());
  out += '.';
  out.append(ext.c_str(), ext.size());

  return out;
}

void file_name(std::string& out, const string_ref& name, const string_ref& ext) {
  out.clear();
  out.reserve(1 + name.size() + ext.size());
  out.append(name.c_str(), name.size());
  out += '.';
  out.append(ext.c_str(), ext.size());
}

std::string file_name(const string_ref& name, uint64_t gen, const string_ref& ext) {
  char buf[21]; // can hold : -2^63 .. 2^64-1, plus 0
  auto buf_size = sprintf(buf, "%" PRIu64, gen);

  std::string out;
  out.reserve(2 + name.size() + buf_size + ext.size());
  out.append(name.c_str(), name.size());
  out += '.';
  out.append(buf, buf_size);
  out += '.';
  out.append(ext.c_str(), ext.size());

  return out;
}

NS_END

#if defined(_MSC_VER)
  #pragma warning (default : 4996)
#endif
