////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include <rapidjson/rapidjson/document.h> // for rapidjson::Document

#include "tfidf.hpp"

#include "scorers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"
#include "utils/math_utils.hpp"

namespace {

const irs::math::sqrt<uint32_t, float_t, 1024> SQRT;

irs::sort::ptr make_from_bool(
    const rapidjson::Document& json,
    const irs::string_ref& //args
) {
  assert(json.IsBool());

  return irs::memory::make_unique<irs::tfidf_sort>(
    json.GetBool());
}

static const irs::string_ref WITH_NORMS_PARAM_NAME = "withNorms";

irs::sort::ptr make_from_object(
    const rapidjson::Document& json,
    const irs::string_ref& args) {
  assert(json.IsObject());

  auto ptr = irs::memory::make_unique<irs::tfidf_sort>();

  #ifdef IRESEARCH_DEBUG
    auto& scorer = dynamic_cast<irs::tfidf_sort&>(*ptr);
  #else
    auto& scorer = static_cast<irs::tfidf_sort&>(*ptr);
  #endif

  {
    // optional bool
   
    if (json.HasMember(WITH_NORMS_PARAM_NAME.c_str())) {
      if (!json[WITH_NORMS_PARAM_NAME.c_str()].IsBool()) {
        IR_FRMT_ERROR("Non-boolean value in '%s' while constructing tfidf scorer from jSON arguments: %s",
                      WITH_NORMS_PARAM_NAME.c_str(),
                      args.c_str());

        return nullptr;
      }

      scorer.normalize(json[WITH_NORMS_PARAM_NAME.c_str()].GetBool());
    }
  }

  return ptr;
}

irs::sort::ptr make_from_array(
    const rapidjson::Document& json,
    const irs::string_ref& args) {
  assert(json.IsArray());

  const auto array = json.GetArray();
  const auto size = array.Size();

  if (size > 1) {
    // wrong number of arguments
    IR_FRMT_ERROR(
      "Wrong number of arguments while constructing tfidf scorer from jSON arguments (must be <= 1): %s",
      args.c_str()
    );
    return nullptr;
  }

  // default args
  auto norms = irs::tfidf_sort::WITH_NORMS();

  // parse `withNorms` optional argument
  if (!array.Empty()) {
    auto& arg = array[0];
    if (!arg.IsBool()) {
      IR_FRMT_ERROR(
        "Non-float value on position `0` while constructing bm25 scorer from jSON arguments: %s",
        args.c_str()
      );
      return nullptr;
    }

    norms = arg.GetBool();
  }

  return irs::memory::make_unique<irs::tfidf_sort>(norms);
}

irs::sort::ptr make_json(const irs::string_ref& args) {
  if (args.null()) {
    return irs::memory::make_unique<irs::tfidf_sort>();
  }

  rapidjson::Document json;

  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
      "Invalid jSON arguments passed while constructing tfidf scorer, arguments: %s", 
      args.c_str()
    );

    return nullptr;
  }

  switch (json.GetType()) {
    case rapidjson::kFalseType:
    case rapidjson::kTrueType:
      return make_from_bool(json, args);
    case rapidjson::kObjectType:
      return make_from_object(json, args);
    case rapidjson::kArrayType:
      return make_from_array(json, args);
    default: // wrong type
      IR_FRMT_ERROR(
        "Invalid jSON arguments passed while constructing tfidf scorer, arguments: %s", 
        args.c_str()
      );

      return nullptr;
  }
}

REGISTER_SCORER_JSON(irs::tfidf_sort, make_json);

struct byte_ref_iterator {
  using iterator_category = std::input_iterator_tag;
  using value_type = irs::byte_type;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = void;

  const irs::byte_type* end_;
  const irs::byte_type* pos_;

  byte_ref_iterator(const irs::bytes_ref& in)
    : end_(in.c_str() + in.size()), pos_(in.c_str()) {
  }

  irs::byte_type operator*() {
    if (pos_ >= end_) {
      throw irs::io_error("invalid read past end of input");
    }

    return *pos_;
  }

  void operator++() { ++pos_; }
};

struct field_collector final: public irs::sort::field_collector {
  uint64_t docs_with_field = 0; // number of documents containing the matched field (possibly without matching terms)

  virtual void collect(const irs::sub_reader& /*segment*/,
                       const irs::term_reader& field) override {
    docs_with_field += field.docs_count();
  }

  virtual void reset() noexcept override {
    docs_with_field = 0;
  }

  virtual void collect(const irs::bytes_ref& in) override {
    byte_ref_iterator itr(in);
    auto docs_with_field_value = irs::vread<uint64_t>(itr);

    if (itr.pos_ != itr.end_) {
      throw irs::io_error("input not read fully");
    }

    docs_with_field += docs_with_field_value;
  }

  virtual void write(irs::data_output& out) const override {
    out.write_vlong(docs_with_field);
  }
};

struct term_collector final: public irs::sort::term_collector {
  uint64_t docs_with_term = 0; // number of documents containing the matched term

  virtual void collect(const irs::sub_reader& /*segment*/,
                       const irs::term_reader& /*field*/,
                       const irs::attribute_provider& term_attrs) override {
    auto* meta = irs::get<irs::term_meta>(term_attrs);

    if (meta) {
      docs_with_term += meta->docs_count;
    }
  }

  virtual void reset() noexcept override {
    docs_with_term = 0;
  }

  virtual void collect(const irs::bytes_ref& in) override {
    byte_ref_iterator itr(in);
    auto docs_with_term_value = irs::vread<uint64_t>(itr);

    if (itr.pos_ != itr.end_) {
      throw irs::io_error("input not read fully");
    }

    docs_with_term += docs_with_term_value;
  }

  virtual void write(irs::data_output& out) const override {
    out.write_vlong(docs_with_term);
  }
};

FORCE_INLINE float_t tfidf(uint32_t freq, float_t idf) noexcept {
  return idf * SQRT(freq);
}

} // LOCAL

namespace iresearch {
namespace tfidf {

// empty frequency
const frequency EMPTY_FREQ;

struct idf final {
  float_t value;
};

typedef tfidf_sort::score_t score_t;

struct score_ctx : public irs::score_ctx {
  score_ctx(
      byte_type* score_buf,
      irs::boost_t boost,
      const tfidf::idf& idf,
      const frequency* freq,
      const filter_boost* fb = nullptr) noexcept
    : score_buf(score_buf),
      freq(freq ? freq : &EMPTY_FREQ),
      filter_boost(fb),
      idf(boost * idf.value) {
    assert(freq);
  }

  byte_type* score_buf;
  const frequency* freq;
  const irs::filter_boost* filter_boost;
  float_t idf; // precomputed : boost * idf
}; // score_ctx

struct norm_score_ctx final : public score_ctx {
  norm_score_ctx(
      byte_type* score_buf,
      norm&& norm,
      boost_t boost,
      const tfidf::idf& idf,
      const frequency* freq,
      const struct filter_boost* fb = nullptr) noexcept
    : score_ctx(score_buf, boost, idf, freq, fb),
      norm_(std::move(norm)) {
  }

  irs::norm norm_;
}; // norm_score_ctx

class sort final: public irs::prepared_sort_basic<tfidf::score_t, tfidf::idf> {
 public:
  explicit sort(bool normalize, bool boost_as_score) noexcept
    : normalize_(normalize), boost_as_score_(boost_as_score) {
  }

  virtual void collect(
      byte_type* stats_buf,
      const irs::index_reader& /*index*/,
      const irs::sort::field_collector* field,
      const irs::sort::term_collector* term) const override {
    auto& idf = stats_cast(stats_buf);

#ifdef IRESEARCH_DEBUG
    auto* field_ptr = dynamic_cast<const field_collector*>(field);
    assert(!field || field_ptr);
    auto* term_ptr = dynamic_cast<const term_collector*>(term);
    assert(!term || term_ptr);
#else
    auto* field_ptr = static_cast<const field_collector*>(field);
    auto* term_ptr = static_cast<const term_collector*>(term);
#endif

    const auto docs_with_field = field_ptr ? field_ptr->docs_with_field : 0; // nullptr possible if e.g. 'all' filter
    const auto docs_with_term = term_ptr ? term_ptr->docs_with_term : 0; // nullptr possible if e.g.'by_column_existence' filter

    idf.value += float_t(
      std::log((docs_with_field + 1) / double_t(docs_with_term + 1)) + 1.0
    );
    assert(idf.value >= 0.f);
  }

  virtual const flags& features() const override {
    static const irs::flags FEATURES[] = {
      irs::flags({ type<frequency>::get() }), // without normalization
      irs::flags({ type<frequency>::get(), type<norm>::get() }), // with normalization
    };

    return FEATURES[normalize_];
  }

  virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
    return irs::memory::make_unique<field_collector>();
  }

  virtual score_function prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const byte_type* stats_buf,
      byte_type* score_buf,
      const attribute_provider& doc_attrs,
      boost_t boost) const override {
    auto* freq = irs::get<frequency>(doc_attrs);

    if (!freq) {
      if (!boost_as_score_ || 0.f == boost) {
        return { nullptr, nullptr };
      }

      // if there is no frequency then all the scores will be the same (e.g. filter irs::all)
      irs::sort::score_cast<tfidf::score_t>(score_buf) = boost;

      return {
        reinterpret_cast<score_ctx*>(score_buf),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          return reinterpret_cast<byte_type*>(ctx);
        }
      };
    }

    auto& stats = stats_cast(stats_buf);
    auto* filter_boost = irs::get<irs::filter_boost>(doc_attrs);

    // add norm attribute if requested
    if (normalize_) {
      irs::norm norm;

      auto* doc = irs::get<document>(doc_attrs);

      if (!doc) {
        // we need 'document' attribute to be exposed
        return { nullptr, nullptr };
      }

      if (norm.reset(segment, field.meta().norm, *doc)) {
        
        if (filter_boost) {
          return {
            memory::make_unique<tfidf::norm_score_ctx>(score_buf, std::move(norm), boost, stats, freq, filter_boost),
            [](irs::score_ctx* ctx) noexcept -> const byte_type* {
              auto& state = *static_cast<tfidf::norm_score_ctx*>(ctx);
              assert(state.filter_boost);
              irs::sort::score_cast<tfidf::score_t>(state.score_buf) = ::tfidf(state.freq->value,
                                                                               state.idf * state.filter_boost->value) *
                                                                       state.norm_.read();

              return state.score_buf;
            }
          };
        } else {
          return {
            memory::make_unique<tfidf::norm_score_ctx>(score_buf, std::move(norm), boost, stats, freq),
            [](irs::score_ctx* ctx) noexcept -> const byte_type* {
              auto& state = *static_cast<tfidf::norm_score_ctx*>(ctx);
              irs::sort::score_cast<tfidf::score_t>(state.score_buf) = ::tfidf(state.freq->value, state.idf) * state.norm_.read();

              return state.score_buf;
            }
          };
        }
      }
    }

    if (filter_boost) {
      return {
        memory::make_unique<tfidf::score_ctx>(score_buf, boost, stats, freq, filter_boost),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          auto& state = *static_cast<tfidf::score_ctx*>(ctx);
          assert(state.filter_boost);
          irs::sort::score_cast<score_t>(state.score_buf) = ::tfidf(state.freq->value,
                                                                    state.idf * state.filter_boost->value);
          return state.score_buf;
      }
      };
    } else {
      return {
        memory::make_unique<tfidf::score_ctx>(score_buf, boost, stats, freq),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          auto& state = *static_cast<tfidf::score_ctx*>(ctx);
          irs::sort::score_cast<score_t>(state.score_buf) = ::tfidf(state.freq->value, state.idf);

          return state.score_buf;
        }
      };
    }
  }

  virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
    return irs::memory::make_unique<term_collector>();
  }

 private:
  bool normalize_;
  bool boost_as_score_;
}; // sort

} // tfidf 

/*static*/ sort::ptr tfidf_sort::make(bool normalize, bool boost_as_score) {
  return std::make_unique<tfidf_sort>(normalize, boost_as_score);
}

tfidf_sort::tfidf_sort(bool normalize, bool boost_as_score) noexcept
  : sort(irs::type<tfidf_sort>::get()),
    normalize_(normalize),
    boost_as_score_(boost_as_score) {
}

/*static*/ void tfidf_sort::init() {
  REGISTER_SCORER_JSON(tfidf_sort, make_json); // match registration above
}

sort::prepared::ptr tfidf_sort::prepare() const {
  return memory::make_unique<tfidf::sort>(normalize_, boost_as_score_);
}

} // ROOT
