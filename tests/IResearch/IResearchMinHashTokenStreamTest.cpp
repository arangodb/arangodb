////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <tuple>

#include "gtest/gtest.h"
#include "analysis/minhash_token_stream.hpp"

namespace {

class ArrayStream final : public irs::analysis::TypedAnalyzer<ArrayStream> {
 public:
  explicit ArrayStream(std::span<std::string_view const> values) noexcept
      : values_{values}, it_{values.end()} {}

  bool next() final {
    if (it_ == values_.end()) {
      return false;
    }
    std::get<irs::term_attribute>(attrs_).value =
        irs::ViewCast<irs::byte_type>(*it_);
    ++it_;
    return true;
  }

  irs::attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    return irs::get_mutable(attrs_, id);
  }

  bool reset(std::string_view) final {
    it_ = values_.begin();
    return true;
  }

 private:
  std::tuple<irs::term_attribute, irs::increment, irs::offset> attrs_;
  std::span<std::string_view const> values_;
  std::span<std::string_view const>::iterator it_;
};

}  // namespace

// minhash tokens are persisted in the index, so their text representation
// (unpadded standard base64) must never change
TEST(IResearchMinHashTokenStreamTest, tokens_are_unpadded_standard_base64) {
  constexpr std::array<std::string_view, 8> kValues{
      "quick", "brown", "fox", "jumps", "over", "the", "lazy", "dog"};
  irs::analysis::MinHashTokenStream stream{
      {.analyzer = std::make_unique<ArrayStream>(kValues), .num_hashes = 4}};
  auto* term = irs::get<irs::term_attribute>(stream);
  ASSERT_NE(nullptr, term);

  ASSERT_TRUE(stream.reset("ignored"));
  for (std::string_view expected :
       {"aq+fPy7QMmE", "zN55OxHobU0", "fE1vyfdbgBg", "g44ma/eW5Rc"}) {
    ASSERT_TRUE(stream.next());
    EXPECT_EQ(expected, irs::ViewCast<char>(term->value));
  }
  EXPECT_FALSE(stream.next());
}
