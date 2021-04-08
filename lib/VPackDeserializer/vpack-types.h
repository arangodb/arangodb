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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef DESERIALIZE_VPACK_TYPES_H
#define DESERIALIZE_VPACK_TYPES_H

#ifndef DESERIALIZER_NO_VPACK_TYPES
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"
#include "velocypack/Builder.h"

namespace arangodb {
namespace velocypack {
namespace deserializer {
using slice_type = arangodb::velocypack::Slice;
using object_iterator = arangodb::velocypack::ObjectIterator;
using array_iterator = arangodb::velocypack::ArrayIterator;
using builder_type = arangodb::velocypack::Builder;
}  // namespace deserializer
}  // namespace velocypack
}  // namespace arangodb
#endif  // DESERIALIZER_NO_VPACK_TYPES

#endif  // DESERIALIZE_VPACK_TYPES_H
