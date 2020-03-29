////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////
#ifndef ARANGOD_IRESEARCH__IRESEARCH_COMPRESSION_H
#define ARANGOD_IRESEARCH__IRESEARCH_COMPRESSION_H 1

#include "utils/string.hpp"

namespace arangodb {
namespace iresearch {

enum class ColumnCompression {
  INVALID = 0,
  NONE = 1,
  LZ4
#ifdef ARANGODB_USE_GOOGLE_TESTS
  , TEST = 999
#endif
};

irs::string_ref columnCompressionToString(ColumnCompression c);
ColumnCompression columnCompressionFromString(irs::string_ref const& c);
} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_COMPRESSION_H
