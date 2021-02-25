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

#include "shared.hpp"
#include "common.hpp"
#include "store/mmap_directory.hpp"
#include "store/fs_directory.hpp"

#include <unordered_map>
#include <functional>

namespace {

typedef std::function<irs::directory::ptr(const std::string&)> factory_f;

const std::unordered_map<std::string, factory_f> FACTORIES {
  { "mmap",
    [](const std::string& path) {
      return irs::memory::make_unique<irs::mmap_directory>(path); }
  },
  { "fs",
    [](const std::string& path) {
      return irs::memory::make_unique<irs::fs_directory>(path); }
  }
};

}

irs::directory::ptr create_directory(
    const std::string& type,
    const std::string& path) {
  const auto it = FACTORIES.find(type);

  return it == FACTORIES.end()
    ? nullptr
    : it->second(path);
}
