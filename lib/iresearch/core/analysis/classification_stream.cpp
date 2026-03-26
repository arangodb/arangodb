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

#include "classification_stream.hpp"

#include <fasttext.h>

#include <string_view>

#include "store/store_utils.hpp"
#include "utils/vpack_utils.hpp"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"

namespace {

using namespace irs::analysis;

constexpr std::string_view MODEL_LOCATION_PARAM_NAME{"model_location"};
constexpr std::string_view TOP_K_PARAM_NAME{"top_k"};
constexpr std::string_view THRESHOLD_PARAM_NAME{"threshold"};

std::atomic<classification_stream::model_provider_f> MODEL_PROVIDER{nullptr};

bool parse_vpack_options(const VPackSlice slice,
                         classification_stream::Options& options,
                         const char* action) {
  if (VPackValueType::Object == slice.type()) {
    auto model_location_slice = slice.get(MODEL_LOCATION_PARAM_NAME);
    if (!model_location_slice.isString()) {
      IRS_LOG_ERROR(
        absl::StrCat("Invalid vpack while ", action,
                     " classification_stream from VPack arguments. ",
                     MODEL_LOCATION_PARAM_NAME, " value should be a string."));
      return false;
    }
    options.model_location = model_location_slice.stringView();
    auto top_k_slice = slice.get(TOP_K_PARAM_NAME);
    if (!top_k_slice.isNone()) {
      if (!top_k_slice.isNumber()) {
        IRS_LOG_ERROR(
          absl::StrCat("Invalid vpack while ", action,
                       " classification_stream from VPack arguments. ",
                       TOP_K_PARAM_NAME, " value should be an integer."));
        return false;
      }
      const auto top_k = top_k_slice.getNumber<size_t>();

      if (top_k > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        IRS_LOG_ERROR(
          absl::StrCat("Invalid value provided while ", action,
                       " classification_stream from VPack arguments. ",
                       TOP_K_PARAM_NAME, " value should be an int32_t."));
        return false;
      }

      options.top_k = static_cast<uint32_t>(top_k);
    }

    auto threshold_slice = slice.get(THRESHOLD_PARAM_NAME);
    if (!threshold_slice.isNone()) {
      if (!threshold_slice.isNumber()) {
        IRS_LOG_ERROR(
          absl::StrCat("Invalid vpack while ", action,
                       " classification_stream from VPack arguments. ",
                       THRESHOLD_PARAM_NAME, " value should be a double."));
        return false;
      }
      const auto threshold = threshold_slice.getNumber<double>();
      if (threshold < 0.0 || threshold > 1.0) {
        IRS_LOG_ERROR(absl::StrCat(
          "Invalid value provided while ", action,
          " classification_stream from VPack arguments. ", TOP_K_PARAM_NAME,
          " value should be between 0.0 and 1.0 (inclusive)."));
        return false;
      }
      options.threshold = threshold;
    }
    return true;
  }

  IRS_LOG_ERROR(absl::StrCat(
    "Invalid vpack while ", action,
    " classification_stream from VPack arguments. Object was expected."));

  return false;
}

analyzer::ptr construct(const classification_stream::Options& options) {
  auto model_provider = ::MODEL_PROVIDER.load(std::memory_order_relaxed);

  classification_stream::model_ptr model;

  try {
    if (model_provider) {
      model = model_provider(options.model_location);
    } else {
      auto new_model = std::make_shared<fasttext::FastText>();
      new_model->loadModel(options.model_location);

      model = new_model;
    }
  } catch (const std::exception& e) {
    IRS_LOG_ERROR(
      absl::StrCat("Failed to load fasttext classification model from: ",
                   options.model_location, ", error: ", e.what()));
  } catch (...) {
    IRS_LOG_ERROR(
      absl::StrCat("Failed to load fasttext classification model from: ",
                   options.model_location));
  }

  if (!model) {
    return nullptr;
  }

  return std::make_unique<classification_stream>(options, std::move(model));
}

analyzer::ptr make_vpack(const VPackSlice slice) {
  classification_stream::Options options{};
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
      IRS_LOG_ERROR("Null arguments while constructing classification_stream ");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.data());
    return make_vpack(vpack->slice());
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while constructing classification_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while constructing classification_stream from JSON");
  }
  return nullptr;
}

bool make_vpack_config(const classification_stream::Options& options,
                       VPackBuilder* builder) {
  VPackObjectBuilder object{builder};
  {
    builder->add(MODEL_LOCATION_PARAM_NAME, VPackValue(options.model_location));
    builder->add(TOP_K_PARAM_NAME, VPackValue(options.top_k));
    builder->add(THRESHOLD_PARAM_NAME, VPackValue(options.threshold));
  }
  return true;
}

bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* builder) {
  classification_stream::Options options{};
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
      IRS_LOG_ERROR("Null arguments while normalizing classification_stream ");
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
                   "' while normalizing classification_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while normalizing classification_stream from JSON");
  }
  return false;
}

REGISTER_ANALYZER_VPACK(irs::analysis::classification_stream, make_vpack,
                        normalize_vpack_config);
REGISTER_ANALYZER_JSON(irs::analysis::classification_stream, make_json,
                       normalize_json_config);

}  // namespace

namespace irs {
namespace analysis {

void classification_stream::init() {
  REGISTER_ANALYZER_JSON(classification_stream, make_json,
                         normalize_json_config);
  REGISTER_ANALYZER_VPACK(classification_stream, make_vpack,
                          normalize_vpack_config);
}

classification_stream::model_provider_f
classification_stream::set_model_provider(model_provider_f provider) noexcept {
  return ::MODEL_PROVIDER.exchange(provider, std::memory_order_relaxed);
}

classification_stream::classification_stream(const Options& options,
                                             model_ptr model) noexcept
  : model_{std::move(model)},
    predictions_it_{predictions_.end()},
    threshold_{options.threshold},
    top_k_{options.top_k} {
  IRS_ASSERT(model_);
}

bool classification_stream::next() {
  if (predictions_it_ == predictions_.end()) {
    return false;
  }

  auto& term = std::get<term_attribute>(attrs_);
  term.value = {
    reinterpret_cast<const byte_type*>(predictions_it_->second.c_str()),
    predictions_it_->second.size()};

  auto& inc = std::get<increment>(attrs_);
  inc.value = uint32_t(predictions_it_ == predictions_.begin());

  ++predictions_it_;

  return true;
}

bool classification_stream::reset(std::string_view data) {
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(data.size());

  bytes_view_input s_input{ViewCast<byte_type>(data)};
  input_buf buf{&s_input};
  std::istream ss{&buf};
  predictions_.clear();
  model_->predictLine(ss, predictions_, top_k_, static_cast<float>(threshold_));
  predictions_it_ = predictions_.begin();

  return true;
}

}  // namespace analysis
}  // namespace irs
