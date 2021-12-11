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

#include "velocypack/Parser.h"
#include "velocypack/Slice.h"

#include "store/store_utils.hpp"
#include "utils/fasttext_utils.hpp"
#include "utils/vpack_utils.hpp"

#include <string_view>

namespace {

using namespace irs::analysis;

constexpr std::string_view MODEL_LOCATION_PARAM_NAME {"model_location"};
constexpr std::string_view TOP_K_PARAM_NAME {"top_k"};

std::atomic<nearest_neighbors_stream::model_provider_f> MODEL_PROVIDER{nullptr};

bool parse_vpack_options(const VPackSlice slice, nearest_neighbors_stream::options& options, const char* action) {
  if (VPackValueType::Object == slice.type()) {
    auto model_location_slice = slice.get(MODEL_LOCATION_PARAM_NAME);
    if (!model_location_slice.isString()) {
      IR_FRMT_ERROR(
        "Invalid vpack while %s nearest_neighbors_stream from VPack arguments. %s value should be a string.",
        action,
        MODEL_LOCATION_PARAM_NAME.data());
      return false;
    }
    options.model_location = irs::get_string<std::string>(model_location_slice);
    auto top_k_slice = slice.get(TOP_K_PARAM_NAME);
    if (!top_k_slice.isNone()) {
      if (!top_k_slice.isNumber()) {
        IR_FRMT_ERROR(
          "Invalid vpack while %s nearest_neighbors_stream from VPack arguments. %s value should be an integer.",
          action,
          TOP_K_PARAM_NAME.data());
        return false;
      }
      const auto top_k = top_k_slice.getNumber<size_t>();
      if (top_k > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        IR_FRMT_ERROR(
          "Invalid value provided while %s nearest_neighbors_stream from VPack arguments. %s value should be an int32_t.",
          action,
          TOP_K_PARAM_NAME.data());
        return false;
      }
      options.top_k = static_cast<uint32_t>(top_k);
    }

    return true;
  }

  IR_FRMT_ERROR(
    "Invalid vpack while %s nearest_neighbors_stream from VPack arguments. Object was expected.",
    action);

  return false;
}

analyzer::ptr construct(const nearest_neighbors_stream::options& options) {
  auto model_provider = ::MODEL_PROVIDER.load(std::memory_order_relaxed);

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
    IR_FRMT_ERROR(
      "Failed to load fasttext kNN model from '%s', error '%s'",
      options.model_location.c_str(), e.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Failed to load fasttext kNN model from '%s'",
      options.model_location.c_str());
  }

  if (!model) {
    return nullptr;
  }

  return irs::memory::make_unique<nearest_neighbors_stream>(options, std::move(model));
}

analyzer::ptr make_vpack(const VPackSlice slice) {
  nearest_neighbors_stream::options options{};
  if (parse_vpack_options(slice, options, "constructing")) {
    return construct(options);
  }
  return nullptr;
}

analyzer::ptr make_vpack(const irs::string_ref& args) {
  VPackSlice slice{reinterpret_cast<const uint8_t*>(args.c_str())};
  return make_vpack(slice);
}

analyzer::ptr make_json(const irs::string_ref& args) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while constructing nearest_neighbors_stream ");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.c_str());
    return make_vpack(vpack->slice());
  } catch (const VPackException &ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while constructing nearest_neighbors_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing nearest_neighbors_stream from JSON");
  }
  return nullptr;
}

bool make_vpack_config(
    const nearest_neighbors_stream::options& options,
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


bool normalize_vpack_config(const irs::string_ref& args, std::string& config) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  VPackBuilder builder;
  if (normalize_vpack_config(slice, &builder)) {
    config.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
    return true;
  }
  return false;
}

bool normalize_json_config(const irs::string_ref& args, std::string& definition) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while normalizing nearest_neighbors_stream ");
      return false;
    }
    auto vpack = VPackParser::fromJson(args.c_str());
    VPackBuilder builder;
    if (normalize_vpack_config(vpack->slice(), &builder)) {
      definition = builder.toString();
      return !definition.empty();
    }
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while normalizing nearest_neighbors_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while normalizing nearest_neighbors_stream from JSON");
  }
  return false;
}

REGISTER_ANALYZER_VPACK(irs::analysis::nearest_neighbors_stream, make_vpack, normalize_vpack_config);
REGISTER_ANALYZER_JSON(irs::analysis::nearest_neighbors_stream, make_json, normalize_json_config);

} // namespace

namespace iresearch {
namespace analysis {

/*static*/ void nearest_neighbors_stream::init() {
  REGISTER_ANALYZER_JSON(nearest_neighbors_stream, make_json, normalize_json_config);
  REGISTER_ANALYZER_VPACK(nearest_neighbors_stream, make_vpack, normalize_vpack_config);
}

/*static*/ nearest_neighbors_stream::model_provider_f nearest_neighbors_stream::set_model_provider(
    model_provider_f provider) noexcept {
  return ::MODEL_PROVIDER.exchange(provider, std::memory_order_relaxed);
}

nearest_neighbors_stream::nearest_neighbors_stream(
    const options& options,
    model_ptr model) noexcept
  : analyzer{irs::type<nearest_neighbors_stream>::get()},
    model_{std::move(model)},
    neighbors_it_{neighbors_.end()},
    n_tokens_{0},
    current_token_ind_{0},
    top_k_{options.top_k} {
  assert(model_);

  model_dict_ = model_->getDictionary();
  assert(model_dict_);
}

bool nearest_neighbors_stream::next() {
  if (neighbors_it_ == neighbors_.end()) {
    if (current_token_ind_ == n_tokens_) {
      return false;
    }
    neighbors_ = model_->getNN(model_dict_->getWord(line_token_ids_[current_token_ind_]), top_k_);
    neighbors_it_ = neighbors_.begin();
    ++current_token_ind_;
  }

  auto& term = std::get<term_attribute>(attrs_);
  term.value = {
    reinterpret_cast<const byte_type *>(neighbors_it_->second.c_str()),
    neighbors_it_->second.size() };

  auto& inc = std::get<increment>(attrs_);
  inc.value = uint32_t(neighbors_it_ == neighbors_.begin());

  ++neighbors_it_;

  return true;
}

bool nearest_neighbors_stream::reset(const string_ref& data) {
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(data.size());

  bytes_ref_input s_input{ref_cast<byte_type>(data)};
  input_buf buf{&s_input};
  std::istream ss{&buf};

  n_tokens_ = model_dict_->getLine(ss, line_token_ids_, line_token_label_ids_);
  current_token_ind_ = 0;

  neighbors_.clear();
  neighbors_it_ = neighbors_.end();

  return true;
}

}
}
