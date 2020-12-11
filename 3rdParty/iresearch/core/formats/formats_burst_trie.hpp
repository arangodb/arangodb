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

#ifndef IRESEARCH_FORMAT_BURST_TRIE_H
#define IRESEARCH_FORMAT_BURST_TRIE_H

#include "formats.hpp"

namespace iresearch {
namespace burst_trie {

enum class Version : int32_t {
  ////////////////////////////////////////////////////////////////////////////
  /// * no encryption support
  /// * term dictionary stored on disk as fst::VectorFst<...>
  ////////////////////////////////////////////////////////////////////////////
  MIN = 0,

  ////////////////////////////////////////////////////////////////////////////
  /// * encryption support
  /// * term dictionary stored on disk as fst::VectorFst<...>
  ////////////////////////////////////////////////////////////////////////////
  ENCRYPTION_MIN = 1,

  ////////////////////////////////////////////////////////////////////////////
  /// * encryption support
  /// * term dictionary stored on disk as fst::fstext::ImmutableFst<...>
  ////////////////////////////////////////////////////////////////////////////
  MAX = 2
};

irs::field_writer::ptr make_writer(
  Version version,
  irs::postings_writer::ptr&& writer,
  bool volatile_state);

irs::field_reader::ptr make_reader(
  irs::postings_reader::ptr&& reader);

} // burst_trie
} // ROOT

#endif
