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

#include "tests_shared.hpp"
#include "tests_param.hpp"
#include "store/fs_directory.hpp"
#include "store/mmap_directory.hpp"
#include "store/memory_directory.hpp"

namespace tests {

std::shared_ptr<irs::directory> memory_directory(
    const test_base* /*test*/,
    irs::directory_attributes attrs) {
  return std::make_shared<irs::memory_directory>(std::move(attrs));
}

std::shared_ptr<irs::directory> fs_directory(
    const test_base* test,
    irs::directory_attributes attrs) {
  std::shared_ptr<irs::directory> impl;

  if (test) {
    auto dir = test->test_dir();

    dir /= "index";
    std::filesystem::create_directory(dir);

    impl = std::shared_ptr<irs::fs_directory>(
      new irs::fs_directory(dir, std::move(attrs)),
      [dir](irs::fs_directory* p) {
        std::filesystem::remove_all(dir);
        delete p;
    });
  }

  return impl;
}

std::shared_ptr<irs::directory> mmap_directory(
    const test_base* test,
    irs::directory_attributes attrs) {
  std::shared_ptr<irs::directory> impl;

  if (test) {
    auto dir = test->test_dir();

    dir /= "index";
    std::filesystem::create_directory(dir);

    impl = std::shared_ptr<irs::mmap_directory>(
      new irs::mmap_directory(dir, std::move(attrs)),
      [dir](irs::mmap_directory* p) {
        std::filesystem::remove_all(dir);
        delete p;
    });
  }

  return impl;
}

} // tests
