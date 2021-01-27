////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include <utils/compression.hpp>

namespace arangodb {
namespace iresearch {

irs::string_ref columnCompressionToString(irs::type_info::type_id type) noexcept;
irs::type_info::type_id columnCompressionFromString(irs::string_ref const& c) noexcept;
irs::type_info::type_id getDefaultCompression() noexcept;
} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_COMPRESSION_H
