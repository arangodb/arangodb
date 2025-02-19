////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <string>
#include <string_view>

#include <ProgramOptions/Parameters.h>
#include <Basics/ResultT.h>

namespace arangodb {
template<typename T>
class ResultT;
}
namespace arangodb::velocypack {
class Slice;
}

namespace arangodb::replication {

enum class Version { ONE = 1, TWO = 2 };

// We disable Replication2 in Production environments for now
// This we only allow to use Version one.
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
constexpr inline auto allowedVersions = {Version::ONE, Version::TWO};
#else
constexpr inline auto allowedVersions = {Version::ONE};
#endif

auto parseVersion(std::string_view version) -> ResultT<replication::Version>;
auto parseVersion(velocypack::Slice version) -> ResultT<replication::Version>;

auto versionToString(Version version) -> std::string_view;

}  // namespace arangodb::replication
