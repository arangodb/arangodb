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
/// @author Julia Casarin Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <frozen/string.h>
#include <frozen/unordered_set.h>

namespace arangodb {
namespace structuredParams {
// the parameters will be converted to lowercase when parsed, so the allow list
// is in lowercase too
static constexpr char DatabaseName[] = "database";
static constexpr char UrlName[] = "url";
static constexpr char UserName[] = "username";
constexpr auto allowList = frozen::make_unordered_set<frozen::string>({
    DatabaseName, UserName, UrlName});
}
}  // namespace arangodb

