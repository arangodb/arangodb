////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Version.h"

#include <Basics/ResultT.h>
#include <Basics/Exceptions.h>
#include <Basics/StringUtils.h>
#include <Logger/LogMacros.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::basics;

auto arangodb::replication::parseVersion(std::string_view version)
    -> ResultT<replication::Version> {
  if (version.data() == nullptr) {
    version = "";
  }
  if (version == "1") {
    return replication::Version::ONE;
  } else if (version == "2") {
    return replication::Version::TWO;
  }
  return ResultT<replication::Version>::error(
      TRI_ERROR_BAD_PARAMETER,
      StringUtils::concatT(R"(Replication version must be "1" or "2", but is )",
                           version));
}

auto arangodb::replication::parseVersion(velocypack::Slice version)
    -> arangodb::ResultT<replication::Version> {
  if (version.isString()) {
    return parseVersion(version.stringView());
  } else {
    return ResultT<replication::Version>::error(
        TRI_ERROR_BAD_PARAMETER,
        StringUtils::concatT(R"(Replication version must be a string, but is )",
                             version.typeName()));
  }
}

auto replication::versionToString(replication::Version version)
    -> std::string_view {
  switch (version) {
    case Version::ONE:
      return "1";
    case Version::TWO:
      return "2";
  }
  abortOrThrow(TRI_ERROR_INTERNAL,
               StringUtils::concatT(
                   "Unhandled replication version: ",
                   static_cast<std::underlying_type_t<Version>>(version)),
               ADB_HERE);
}
