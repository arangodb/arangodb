////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "pipeline_token_stream.hpp"

#include <rapidjson/rapidjson/document.h> // for rapidjson::Document
#include <rapidjson/rapidjson/writer.h> // for rapidjson::Writer
#include <rapidjson/rapidjson/stringbuffer.h> // for rapidjson::StringBuffer

namespace {

constexpr irs::string_ref PIPELINE_PARAM_NAME = "pipeline";
constexpr irs::string_ref TYPE_PARAM_NAME = "type";
constexpr irs::string_ref PROPERTIES_PARAM_NAME = "properties";

const irs::offset NO_OFFSET;

class empty_analyzer final
  : public irs::analysis::analyzer, private irs::util::noncopyable {
 public:
  empty_analyzer() : analyzer(irs::type<empty_analyzer>::get()) {}
  virtual irs::attribute* get_mutable(irs::type_info::type_id) override { return nullptr;  }
  static constexpr irs::string_ref type_name() noexcept { return "empty_analyzer"; }
  virtual bool next() override { return false; }
  virtual bool reset(const irs::string_ref&) override { return false; }
};

empty_analyzer EMPTY_ANALYZER;

using  options_normalize_t = std::vector<std::pair<std::string, std::string>>;

template<typename T>
bool parse_json_config(const irs::string_ref& args, T& options) {
  if constexpr (std::is_same_v<T, irs::analysis::pipeline_token_stream::options_t>) {
    assert(options.empty());
  }
  rapidjson::Document json;
  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
      "Invalid jSON arguments passed while constructing pipeline_token_stream, "
      "arguments: %s",
      args.c_str());

    return false;
  }

  if (rapidjson::kObjectType != json.GetType()) {
    IR_FRMT_ERROR(
      "Not a jSON object passed while constructing pipeline_token_stream, "
      "arguments: %s",
      args.c_str());

    return false;
  }

  if (json.HasMember(PIPELINE_PARAM_NAME.c_str())) {
    auto& pipeline = json[PIPELINE_PARAM_NAME.c_str()];
    if (pipeline.IsArray()) {
      for (auto pipe = pipeline.Begin(), end = pipeline.End(); pipe != end; ++pipe) {
        if (pipe->IsObject()) {
          irs::string_ref type;
          if (pipe->HasMember(TYPE_PARAM_NAME.c_str())) {
            auto& type_atr = (*pipe)[TYPE_PARAM_NAME.c_str()];
            if (type_atr.IsString()) {
              type = type_atr.GetString();
            } else {
              IR_FRMT_ERROR(
                "Failed to read '%s' attribute of  '%s' member as string while constructing "
                "pipeline_token_stream from jSON arguments: %s",
                TYPE_PARAM_NAME.c_str(), PIPELINE_PARAM_NAME.c_str(), args.c_str());
              return false;
            }
          } else {
            IR_FRMT_ERROR(
              "Failed to get '%s' attribute of  '%s' member while constructing "
              "pipeline_token_stream from jSON arguments: %s",
              TYPE_PARAM_NAME.c_str(), PIPELINE_PARAM_NAME.c_str(), args.c_str());
            return false;
          }
          if (pipe->HasMember(PROPERTIES_PARAM_NAME.c_str())) {
            auto& properties_atr = (*pipe)[PROPERTIES_PARAM_NAME.c_str()];
            rapidjson::StringBuffer properties_buffer;
            rapidjson::Writer< rapidjson::StringBuffer> writer(properties_buffer);
            properties_atr.Accept(writer);
            if constexpr (std::is_same_v<T, irs::analysis::pipeline_token_stream::options_t>) {
              auto analyzer = irs::analysis::analyzers::get(
                type,
                irs::type<irs::text_format::json>::get(),
                properties_buffer.GetString());
              if (analyzer) {
                options.push_back(std::move(analyzer));
              } else {
                IR_FRMT_ERROR(
                  "Failed to create pipeline member of type '%s' with properties '%s' while constructing "
                  "pipeline_token_stream from jSON arguments: %s",
                  type.c_str(), properties_buffer.GetString(), args.c_str());
                return false;
              }
            } else {
              std::string normalized;
              if (!irs::analysis::analyzers::normalize(normalized, type,
                                                       irs::type<irs::text_format::json>::get(),
                properties_buffer.GetString())) {
                IR_FRMT_ERROR(
                  "Failed to normalize pipeline member of type '%s' with properties '%s' while constructing "
                  "pipeline_token_stream from jSON arguments: %s",
                  type.c_str(), properties_buffer.GetString(), args.c_str());
                return false;
              }
              options.emplace_back(type, normalized);
            }
          } else {
            IR_FRMT_ERROR(
              "Failed to get '%s' attribute of  '%s' member while constructing "
              "pipeline_token_stream from jSON arguments: %s",
              PROPERTIES_PARAM_NAME.c_str(), PIPELINE_PARAM_NAME.c_str(), args.c_str());
            return false;
          }
        } else {
          IR_FRMT_ERROR(
            "Failed to read '%s' member as object while constructing "
            "pipeline_token_stream from jSON arguments: %s",
            PIPELINE_PARAM_NAME.c_str(), args.c_str());
          return false;
        }
      }
    } else {
      IR_FRMT_ERROR(
        "Failed to read '%s' attribute as array while constructing "
        "pipeline_token_stream from jSON arguments: %s",
        PIPELINE_PARAM_NAME.c_str(), args.c_str());
      return false;
    }
  } else {
    IR_FRMT_ERROR(
      "Not found parameter '%s' while constructing pipeline_token_stream, "
      "arguments: %s",
      PIPELINE_PARAM_NAME.c_str(),
      args.c_str());
    return false;
  }
  if (options.empty()) {
    IR_FRMT_ERROR(
      "Empty pipeline found while constructing pipeline_token_stream, "
      "arguments: %s", args.c_str());
    return false;
  }
  return true;
}

bool normalize_json_config(const irs::string_ref& args, std::string& definition) {
  options_normalize_t options;
  if (parse_json_config(args, options)) {
    definition.clear();
    definition.append("{\"").append(PIPELINE_PARAM_NAME).append("\":[");
    bool first{ true };
    for (auto analyzer : options) {
      if (first) {
        first = false;
      } else {
        definition.append(",");
      }
      definition.append("{\"").append(TYPE_PARAM_NAME)
        .append("\":\"").append(analyzer.first).append("\",")
        .append("\"").append(PROPERTIES_PARAM_NAME).append("\":")
        .append(analyzer.second).append("}");
    }
    definition.append("]}");
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
/// pipeline: Array of objects containing analyzers definition inside pipeline.
/// Each definition is an object with the following attributes:
/// type: analyzer type name (one of registered analyzers type)
/// properties: object with properties for corresponding analyzer
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  irs::analysis::pipeline_token_stream::options_t options;
  if (parse_json_config(args, options)) {
    return std::make_shared<irs::analysis::pipeline_token_stream>(std::move(options));
  } else {
    return nullptr;
  }
}


REGISTER_ANALYZER_JSON(irs::analysis::pipeline_token_stream, make_json,
  normalize_json_config);

irs::attribute* find_payload(const std::vector<irs::analysis::analyzer::ptr>& pipeline) {
  for (auto it = pipeline.rbegin(); it != pipeline.rend(); ++it) {
    auto payload = irs::get_mutable<irs::payload>(it->get());
    if (payload) {
      return payload;
    }
  }
  return nullptr;
}

bool all_have_offset(const std::vector<irs::analysis::analyzer::ptr>& pipeline) {
  return std::all_of(
      pipeline.begin(), pipeline.end(),
      [](const irs::analysis::analyzer::ptr& v) {
    return nullptr != irs::get<irs::offset>(*v);
  });
}

}

namespace iresearch {
namespace analysis {

pipeline_token_stream::pipeline_token_stream(pipeline_token_stream::options_t&& options)
  : attributes{ {
    { irs::type<payload>::id(), find_payload(options)},
    { irs::type<increment>::id(), &inc_},
    { irs::type<offset>::id(), all_have_offset(options)? &offs_: nullptr},
    { irs::type<term_attribute>::id(), options.empty() ?
                                         nullptr
                                         : irs::get_mutable<term_attribute>(options.back().get())}},
    irs::type<pipeline_token_stream>::get()} {
  const auto track_offset = irs::get<offset>(*this) != nullptr;
  pipeline_.reserve(options.size());
  for (const auto& p : options) {
    assert(p);
    pipeline_.emplace_back(std::move(p), track_offset);
  }
  options.clear(); // mimic move semantic
  if (pipeline_.empty()) {
    pipeline_.emplace_back();
  }
  top_ = pipeline_.begin();
  bottom_ = --pipeline_.end();
}

/// Moves pipeline to next token.
/// Term is taken from last analyzer in pipeline
/// Offset is recalculated accordingly (only if ALL analyzers in pipeline )
/// Payload is taken from lowest analyzer having this attribute
/// Increment is calculated according to following position change rules
///  - If none of pipeline members changes position - whole pipeline holds position
///  - If one or more pipeline member moves - pipeline moves( change from max->0 is not move, see rules below!).
///    All position gaps are accumulated (e.g. if one member has inc 2(1 pos gap) and other has inc 3(2 pos gap)  - pipeline has inc 4 (1+2 pos gap))
///  - All position changes caused by parent analyzer move next (e.g. transfer from max->0 by first next after reset) are collapsed.
///    e.g if parent moves after next, all its children are resetted to new token and also moves step froward - this whole operation
///    is just one step for pipeline (if any of children has moved more than 1 step - gaps are preserved!)
///  - If parent after next is NOT moved (inc == 0) than pipeline makes one step forward if at least one child changes
///    position from any positive value back to 0 due to reset (additional gaps also preserved!) as this is
///    not change max->0 and position is indeed changed.
bool pipeline_token_stream::next() {
  uint32_t pipeline_inc;
  bool step_for_rollback{ false };
  do {
    while (!current_->next()) {
      if (current_ == top_) { // reached pipeline top and next has failed - we are done
        return false;
      }
      --current_;
    }
    pipeline_inc = current_->inc->value;
    const auto top_holds_position = current_->inc->value == 0;
    // go down to lowest pipe to get actual tokens
    while (current_ != bottom_) {
      const auto prev_term = current_->term->value;
      const auto prev_start = current_->start();
      const auto prev_end = current_->end();
      ++current_;
      // check do we need to do step forward due to rollback to 0.
      step_for_rollback |= top_holds_position && current_->pos != 0 &&
        current_->pos != irs::integer_traits<uint32_t>::const_max;
      if (!current_->reset(prev_start, prev_end, irs::ref_cast<char>(prev_term))) {
        return false;
      }
      if (!current_->next()) { // empty one found. Another round from the very beginning.
        assert(current_ != top_);
        --current_;
        break;
      }
      pipeline_inc += current_->inc->value;
      assert(current_->inc->value > 0); // first increment after reset should be positive to give 0 or next pos!
      assert(pipeline_inc > 0);
      pipeline_inc--; // compensate placing sub_analyzer from max to 0 due to reset
                      // as this step actually does not move whole pipeline
                      // sub analyzer just stays same pos as it`s parent (step for rollback to 0 will be done below if necessary!)
    }
  } while (current_ != bottom_);
  if (step_for_rollback) {
    pipeline_inc++;
  }
  inc_.value = pipeline_inc;
  offs_.start = current_->start();
  offs_.end = current_->end();
  return true;
}

bool pipeline_token_stream::reset(const string_ref& data) {
  current_ = top_;
  return pipeline_.front().reset(0, static_cast<uint32_t>(data.size()), data);
}

/*static*/ void pipeline_token_stream::init() {
  REGISTER_ANALYZER_JSON(pipeline_token_stream, make_json,
    normalize_json_config);  // match registration above
}

pipeline_token_stream::sub_analyzer_t::sub_analyzer_t(
    const irs::analysis::analyzer::ptr& a,
    bool track_offset)
  : term(irs::get<irs::term_attribute>(*a)),
    inc(irs::get<irs::increment>(*a)),
    offs(track_offset ? irs::get<irs::offset>(*a) : &NO_OFFSET),
    analyzer(a) {
  assert(inc);
  assert(term);
}

pipeline_token_stream::sub_analyzer_t::sub_analyzer_t()
  : term(nullptr), inc(nullptr), offs(nullptr),
    analyzer(irs::analysis::analyzer::ptr(), &EMPTY_ANALYZER) { }

}
}
