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

#ifndef IRESEARCH_EMBEDDING_CLASSIFICATION_STREAM_H
#define IRESEARCH_EMBEDDING_CLASSIFICATION_STREAM_H

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/frozen_attributes.hpp"

namespace fasttext {
class FastText;
}

namespace iresearch {
namespace analysis {

class classification_stream final
    : public analyzer,
      private util::noncopyable {
 public:
  using model_ptr = std::shared_ptr<const fasttext::FastText>;
  using model_provider_f = model_ptr(*)(std::string_view);

  static model_provider_f set_model_provider(model_provider_f provider) noexcept;

  struct Options {
    explicit Options(
        std::string model_location = "",
        int32_t top_k = 1,
        double threshold = 0.0) noexcept
      : model_location{std::move(model_location)},
        threshold{threshold},
        top_k{top_k} {
    }

    std::string model_location;
    double threshold;
    int32_t top_k;
  };

  static constexpr string_ref type_name() noexcept { return "classification"; }

  static void init(); // for registration in a static build

  explicit classification_stream(const Options& options, model_ptr mode) noexcept;

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

  attributes attrs_;
  model_ptr model_;
  std::vector<std::pair<float, std::string>> predictions_;
  std::vector<std::pair<float, std::string>>::iterator predictions_it_;
  double threshold_;
  int32_t top_k_;
};

} // analysis
} // iresearch


#endif //IRESEARCH_EMBEDDING_CLASSIFICATION_STREAM_H
