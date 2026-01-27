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

#include "nearest_neighbors_stream.hpp"

#include <fasttext.h>

#include <string_view>

#include "store/store_utils.hpp"
#include "utils/fasttext_utils.hpp"
#include "utils/vpack_utils.hpp"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"

namespace irs::analysis {
namespace {

constexpr std::string_view MODEL_LOCATION_PARAM_NAME{"model_location"};
constexpr std::string_view TOP_K_PARAM_NAME{"top_k"};

std::atomic<nearest_neighbors_stream::model_provider_f> MODEL_PROVIDER{nullptr};

bool parse_vpack_options(const VPackSlice slice,
                         nearest_neighbors_stream::options& options,
                         const char* action) {
  if (VPackValueType::Object == slice.type()) {
    auto model_location_slice = slice.get(MODEL_LOCATION_PARAM_NAME);
    if (!model_location_slice.isString()) {
      IRS_LOG_ERROR(
        absl::StrCat("Invalid vpack while ", action,
                     " nearest_neighbors_stream from VPack arguments. ",
                     MODEL_LOCATION_PARAM_NAME, " value should be a string."));
      return false;
    }
    options.model_location = model_location_slice.stringView();
    auto top_k_slice = slice.get(TOP_K_PARAM_NAME);
    if (!top_k_slice.isNone()) {
      if (!top_k_slice.isNumber()) {
        IRS_LOG_ERROR(
          absl::StrCat("Invalid vpack while ", action,
                       " nearest_neighbors_stream from VPack arguments. ",
                       TOP_K_PARAM_NAME, " value should be an integer."));
        return false;
      }
      const auto top_k = top_k_slice.getNumber<size_t>();
      if (top_k > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        IRS_LOG_ERROR(
          absl::StrCat("Invalid value provided while ", action,
                       " nearest_neighbors_stream from VPack arguments. ",
                       TOP_K_PARAM_NAME, " value should be an int32_t."));
        return false;
      }
      options.top_k = static_cast<uint32_t>(top_k);
    }

    return true;
  }

  IRS_LOG_ERROR(absl::StrCat(
    "Invalid vpack while ", action,
    " nearest_neighbors_stream from VPack arguments. Object was expected."));

  return false;
}

analyzer::ptr construct(const nearest_neighbors_stream::options& options) {
  auto model_provider = MODEL_PROVIDER.load(std::memory_order_relaxed);

  nearest_neighbors_stream::model_ptr model;

  try {
    if (model_provider) {
      model = model_provider(options.model_location);
    } else {
      auto new_model = std::make_shared<fasttext::ImmutableFastText>();
      new_model->loadModel(options.model_location);

      model = new_model;
    }
  } catch (const std::exception& e) {
    IRS_LOG_ERROR(absl::StrCat("Failed to load fasttext kNN model from: ",
                               options.model_location, ", error: ", e.what()));
  } catch (...) {
    IRS_LOG_ERROR(absl::StrCat("Failed to load fasttext kNN model from: ",
                               options.model_location));
  }

  if (!model) {
    return nullptr;
  }

  return std::make_unique<nearest_neighbors_stream>(options, std::move(model));
}

analyzer::ptr make_vpack(const VPackSlice slice) {
  nearest_neighbors_stream::options options{};
  if (parse_vpack_options(slice, options, "constructing")) {
    return construct(options);
  }
  return nullptr;
}

analyzer::ptr make_vpack(std::string_view args) {
  VPackSlice slice{reinterpret_cast<const uint8_t*>(args.data())};
  return make_vpack(slice);
}

analyzer::ptr make_json(std::string_view args) {
  try {
    if (irs::IsNull(args)) {
      IRS_LOG_ERROR(
        "Null arguments while constructing nearest_neighbors_stream ");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.data());
    return make_vpack(vpack->slice());
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while constructing nearest_neighbors_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while constructing nearest_neighbors_stream from JSON");
  }
  return nullptr;
}

bool make_vpack_config(const nearest_neighbors_stream::options& options,
                       VPackBuilder* builder) {
  VPackObjectBuilder object{builder};
  {
    builder->add(MODEL_LOCATION_PARAM_NAME, VPackValue(options.model_location));
    builder->add(TOP_K_PARAM_NAME, VPackValue(options.top_k));
  }
  return true;
}

bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* builder) {
  nearest_neighbors_stream::options options{};
  if (parse_vpack_options(slice, options, "normalizing")) {
    return make_vpack_config(options, builder);
  }
  return false;
}

bool normalize_vpack_config(std::string_view args, std::string& config) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
  VPackBuilder builder;
  if (normalize_vpack_config(slice, &builder)) {
    config.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
    return true;
  }
  return false;
}

bool normalize_json_config(std::string_view args, std::string& definition) {
  try {
    if (irs::IsNull(args)) {
      IRS_LOG_ERROR(
        "Null arguments while normalizing nearest_neighbors_stream ");
      return false;
    }
    auto vpack = VPackParser::fromJson(args.data());
    VPackBuilder builder;
    if (normalize_vpack_config(vpack->slice(), &builder)) {
      definition = builder.toString();
      return !definition.empty();
    }
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while normalizing nearest_neighbors_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while normalizing nearest_neighbors_stream from JSON");
  }
  return false;
}

REGISTER_ANALYZER_VPACK(nearest_neighbors_stream, make_vpack,
                        normalize_vpack_config);
REGISTER_ANALYZER_JSON(nearest_neighbors_stream, make_json,
                       normalize_json_config);

}  // namespace

void nearest_neighbors_stream::init() {
  REGISTER_ANALYZER_JSON(nearest_neighbors_stream, make_json,
                         normalize_json_config);
  REGISTER_ANALYZER_VPACK(nearest_neighbors_stream, make_vpack,
                          normalize_vpack_config);
}

nearest_neighbors_stream::model_provider_f
nearest_neighbors_stream::set_model_provider(
  model_provider_f provider) noexcept {
  return MODEL_PROVIDER.exchange(provider, std::memory_order_relaxed);
}

nearest_neighbors_stream::nearest_neighbors_stream(const options& options,
                                                   model_ptr model) noexcept
  : model_{std::move(model)},
    neighbors_it_{neighbors_.end()},
    n_tokens_{0},
    current_token_ind_{0},
    top_k_{options.top_k} {
  IRS_ASSERT(model_);

  model_dict_ = model_->getDictionary();
  IRS_ASSERT(model_dict_);
}

bool nearest_neighbors_stream::next() {
  if (neighbors_it_ == neighbors_.end()) {
    if (current_token_ind_ == n_tokens_) {
      return false;
    }
    neighbors_ = model_->getNN(
      model_dict_->getWord(line_token_ids_[current_token_ind_]), top_k_);
    neighbors_it_ = neighbors_.begin();
    ++current_token_ind_;
  }

  auto& term = std::get<term_attribute>(attrs_);
  term.value = {
    reinterpret_cast<const byte_type*>(neighbors_it_->second.c_str()),
    neighbors_it_->second.size()};

  auto& inc = std::get<increment>(attrs_);
  inc.value = uint32_t(neighbors_it_ == neighbors_.begin());

  ++neighbors_it_;

  return true;
}

bool nearest_neighbors_stream::reset(std::string_view data) {
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(data.size());

  bytes_view_input s_input{ViewCast<byte_type>(data)};
  input_buf buf{&s_input};
  std::istream ss{&buf};

  model_dict_->getLine(ss, line_token_ids_, line_token_label_ids_);
  n_tokens_ = static_cast<int32_t>(line_token_ids_.size());
  current_token_ind_ = 0;

  neighbors_.clear();
  neighbors_it_ = neighbors_.end();

  return true;
}

}  // namespace irs::analysis
