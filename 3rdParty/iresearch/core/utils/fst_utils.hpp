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

#ifndef IRESEARCH_FST_UTILS_H
#define IRESEARCH_FST_UTILS_H

#include "store/data_output.hpp"
#include "store/data_input.hpp"

#include "utils/noncopyable.hpp"

#include "fstext/fst_decl.hpp"
#include "fstext/fst_string_weight.h"

NS_ROOT

struct byte_weight_output : data_output, private util::noncopyable {
  virtual void close() override {}

  virtual void write_byte(byte_type b) override final {
    weight.PushBack(b);
  }

  virtual void write_bytes(const byte_type* b, size_t len) override final {
    weight.PushBack(b, b + len);
  }

  byte_weight weight;
}; // byte_weight_output

NS_END // ROOT

#endif
