////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_COLUMNSTORE_H
#define IRESEARCH_COLUMNSTORE_H

#include "shared.hpp"

#include "formats/formats.hpp"

namespace iresearch {
namespace columnstore {

enum class Version : int32_t {
  ////////////////////////////////////////////////////////////////////////////
  /// * no encryption support
  /// * no custom compression support
  ////////////////////////////////////////////////////////////////////////////
  MIN = 0,

  ////////////////////////////////////////////////////////////////////////////
  /// * encryption support
  /// * per column compression
  ////////////////////////////////////////////////////////////////////////////
  MAX = 1,
}; // Version

IRESEARCH_API irs::columnstore_writer::ptr make_writer(Version version);
IRESEARCH_API irs::columnstore_reader::ptr make_reader();

} // columnstore
} // iresearch

#endif // IRESEARCH_COLUMNSTORE_H
