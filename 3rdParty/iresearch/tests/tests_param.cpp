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

std::pair<std::shared_ptr<irs::directory>, std::string> memory_directory(const test_base*) {
  return std::make_pair(
    std::make_shared<irs::memory_directory>(),
    "memory"
  );
}

std::pair<std::shared_ptr<irs::directory>, std::string> fs_directory(const test_base* test) {
  std::shared_ptr<irs::directory> impl;

  if (test) {
    auto dir = test->test_dir();

    dir /= "index";
    dir.mkdir(false);

    impl = std::shared_ptr<irs::fs_directory>(
      new irs::fs_directory(dir.utf8()),
      [dir](irs::fs_directory* p) {
        dir.remove();
        delete p;
    });
  }

  return std::make_pair(impl, "fs");
}

std::pair<std::shared_ptr<irs::directory>, std::string> mmap_directory(const test_base* test) {
  std::shared_ptr<irs::directory> impl;

  if (test) {
    auto dir = test->test_dir();

    dir /= "index";
    dir.mkdir(false);

    impl = std::shared_ptr<irs::mmap_directory>(
      new irs::mmap_directory(dir.utf8()),
      [dir](irs::mmap_directory* p) {
        dir.remove();
        delete p;
    });
  }

  return std::make_pair(impl, "mmap");
}

// -----------------------------------------------------------------------------
// --SECTION--                                          directory_test_case_base
// -----------------------------------------------------------------------------

/*static*/ std::string directory_test_case_base::to_string(
    const testing::TestParamInfo<tests::dir_factory_f>& info
) {
  return (*info.param)(nullptr).second;
}

void directory_test_case_base::SetUp() {
  test_base::SetUp();

  auto* factory = GetParam();
  ASSERT_NE(nullptr, factory);

  dir_ = (*factory)(this).first;
  ASSERT_NE(nullptr, dir_);
}

void directory_test_case_base::TearDown() {
  dir_ = nullptr;
  test_base::TearDown();
}

} // tests
