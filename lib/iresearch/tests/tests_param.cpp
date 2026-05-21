////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "tests_param.hpp"

#include "tests_shared.hpp"
#ifdef IRESEARCH_URING
#include "store/async_directory.hpp"
#endif
#include "store/fs_directory.hpp"
#include "store/memory_directory.hpp"
#include "store/mmap_directory.hpp"
#include "utils/file_utils.hpp"

namespace tests {

std::string to_string(dir_generator_f generator) {
  if (generator == &memory_directory) {
    return "memory";
  }

  if (generator == &fs_directory) {
    return "fs";
  }

  if (generator == &mmap_directory) {
    return "mmap";
  }

#ifdef IRESEARCH_URING
  if (generator == &async_directory) {
    return "async";
  }
#endif

  return "unknown";
}

std::shared_ptr<irs::directory> memory_directory(
  const test_base* /*test*/, irs::directory_attributes attrs) {
  return std::make_shared<irs::memory_directory>(std::move(attrs));
}

std::shared_ptr<irs::directory> fs_directory(const test_base* test,
                                             irs::directory_attributes attrs) {
  return MakePhysicalDirectory<irs::FSDirectory>(test, std::move(attrs));
}

std::shared_ptr<irs::directory> mmap_directory(
  const test_base* test, irs::directory_attributes attrs) {
  return MakePhysicalDirectory<irs::MMapDirectory>(test, std::move(attrs));
}

#ifdef IRESEARCH_URING
std::shared_ptr<irs::directory> async_directory(
  const test_base* test, irs::directory_attributes attrs) {
  return MakePhysicalDirectory<irs::AsyncDirectory>(test, std::move(attrs));
}
#endif

}  // namespace tests
