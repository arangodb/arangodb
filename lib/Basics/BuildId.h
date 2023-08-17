////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <optional>
#include <string>

#include "Basics/StringUtils.h"

// The code contained in this file and BuildId.cpp is supposed to be a mininum
// viable, robust way of reading our own build-id from the ElfHeader
//
// The build-id is a unique identifier for the built executable and as such
// a valuable way of identifying which executable was involved in a support
// situation.
//
// For better visibility this code reads the build-id of its own executable so
// that it can be included in log messages in the CrashHandler and detailed
// version information obtainable via the rest version endpoint
//
// This is not an attempt at making the most complete or pretty implementation
// of an ELF parser.
namespace arangodb::build_id {
struct BuildId {
  explicit BuildId(const char* s, size_t len) : id{s, len} {}
  std::string id;

  std::string toHexString() { return basics::StringUtils::encodeHex(id); }
};

std::optional<BuildId> getBuildId();
}  // namespace arangodb::build_id
