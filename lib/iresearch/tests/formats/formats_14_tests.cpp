////////////////////////////////////////////////////////////////////////////////
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "formats_test_case_base.hpp"
#include "store/directory_attributes.hpp"
#include "tests_shared.hpp"

namespace {

using tests::format_test_case;
using tests::format_test_case_with_encryption;

class format_14_test_case : public format_test_case_with_encryption {};

TEST_P(format_14_test_case, write_zero_block_encryption) {
  tests::json_doc_generator gen(resource("simple_sequential.json"),
                                &tests::generic_json_field_factory);

  const tests::document* doc1 = gen.next();

  // replace encryption
  ASSERT_NE(nullptr, dir().attributes().encryption());
  dir().attributes() =
    irs::directory_attributes{std::make_unique<tests::rot13_encryption>(0)};

  auto writer = irs::IndexWriter::Make(dir(), codec(), irs::OM_CREATE);
  ASSERT_NE(nullptr, writer);

  ASSERT_THROW(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                      doc1->stored.begin(), doc1->stored.end()),
               irs::index_error);
}

static constexpr auto kTestDirsWithEncryption =
  tests::getDirectories<tests::kTypesRot13_16 | tests::kTypesRot13_7>();
static const auto kDirectoriesWithEncryption =
  ::testing::ValuesIn(kTestDirsWithEncryption);

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();
static const auto kDirectories = ::testing::ValuesIn(kTestDirs);

static const auto kFormats = ::testing::Values(
  tests::format_info{"1_4", "1_0"}, tests::format_info{"1_4simd", "1_0"});

// 1.4 specific tests
INSTANTIATE_TEST_SUITE_P(format_14_test, format_14_test_case,
                         ::testing::Combine(kDirectoriesWithEncryption,
                                            kFormats),
                         format_14_test_case::to_string);

// Generic tests
INSTANTIATE_TEST_SUITE_P(format_14_test, format_test_case_with_encryption,
                         ::testing::Combine(kDirectoriesWithEncryption,
                                            kFormats),
                         format_test_case_with_encryption::to_string);

INSTANTIATE_TEST_SUITE_P(format_14_test, format_test_case,
                         ::testing::Combine(kDirectories, kFormats),
                         format_test_case::to_string);

}  // namespace
