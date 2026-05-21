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

#include "common.hpp"

#include <frozen/string.h>
#include <frozen/unordered_map.h>

#include "shared.hpp"
#ifdef IRESEARCH_URING
#include "store/async_directory.hpp"
#endif
#include "store/fs_directory.hpp"
#include "store/mmap_directory.hpp"

namespace {

using factory_f = irs::directory::ptr (*)(std::string_view);

constexpr auto kFactories =
  frozen::make_unordered_map<frozen::string, factory_f>({
#ifdef IRESEARCH_URING
    {"async",
     [](std::string_view path) -> irs::directory::ptr {
       return std::make_unique<irs::AsyncDirectory>(path);
     }},
#endif
    {"mmap",
     [](std::string_view path) -> irs::directory::ptr {
       return std::make_unique<irs::MMapDirectory>(path);
     }},
    {"fs", [](std::string_view path) -> irs::directory::ptr {
       return std::make_unique<irs::FSDirectory>(path);
     }}});

}  // namespace

irs::directory::ptr create_directory(std::string_view type,
                                     std::string_view path) {
  const auto it = kFactories.find(type);

  return it == kFactories.end() ? nullptr : it->second(path);
}
