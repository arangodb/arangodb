////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "index-put.hpp"
#include "index-search.hpp"

#include <unordered_map>
#include <functional>

typedef std::unordered_map<
  std::string,
  std::function<int(int argc, char* argv[])>
> handlers_t;

const std::string MODE_PUT = "put";
const std::string MODE_SEARCH = "search";

bool init_handlers(handlers_t& handlers) {
  handlers.emplace(MODE_PUT, &put);
  handlers.emplace(MODE_SEARCH, &search);
  return true;
}
