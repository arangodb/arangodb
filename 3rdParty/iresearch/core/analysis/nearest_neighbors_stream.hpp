////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Alex Geenen
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_NEAREST_NEIGHBORS_STREAM_H
#define IRESEARCH_NEAREST_NEIGHBORS_STREAM_H

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/frozen_attributes.hpp"

namespace fasttext {
class ImmutableFastText;
class Dictionary;
}

namespace iresearch {
namespace analysis {

class nearest_neighbors_stream final
  : public analyzer,
    private util::noncopyable {
 public:
  using model_ptr = std::shared_ptr<const fasttext::ImmutableFastText>;
  using model_provider_f = model_ptr(*)(std::string_view);

  static model_provider_f set_model_provider(model_provider_f provider) noexcept;

  struct options {
    explicit options(std::string model_location = "", int32_t top_k = 1) noexcept
      : model_location{std::move(model_location)}, top_k{top_k} {
    }

    std::string model_location;
    int32_t top_k;
  };

  static constexpr string_ref type_name() noexcept { return "nearest_neighbors"; }

  static void init(); // for registration in a static build

  explicit nearest_neighbors_stream(
    const options& options,
    model_ptr model_provider) noexcept;

  virtual attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::get_mutable(attrs_, type);
  }

  virtual bool next() override;
  virtual bool reset(const string_ref& data) override;

 private:
  using attributes = std::tuple<
    increment,
    offset,
    term_attribute>;

  model_ptr model_;
  std::shared_ptr<const fasttext::Dictionary> model_dict_;
  std::vector<std::pair<float, std::string>> neighbors_;
  std::vector<std::pair<float, std::string>>::iterator neighbors_it_;
  std::vector<int32_t> line_token_ids_;
  std::vector<int32_t> line_token_label_ids_;
  attributes attrs_;
  int32_t n_tokens_;
  int32_t current_token_ind_;
  int32_t top_k_;
};

} // analysis
} // iresearch


#endif //IRESEARCH_NEAREST_NEIGHBORS_STREAM_H
