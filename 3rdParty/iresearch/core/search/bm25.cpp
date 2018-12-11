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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <rapidjson/rapidjson/document.h> // for rapidjson::Document

#include "bm25.hpp"

#include "scorers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"

NS_LOCAL

irs::sort::ptr make_from_object(
    const rapidjson::Document& json,
    const irs::string_ref& args) {
  assert(json.IsObject());

  auto ptr = irs::memory::make_shared<irs::bm25_sort>();

  #ifdef IRESEARCH_DEBUG
    auto& scorer = dynamic_cast<irs::bm25_sort&>(*ptr);
  #else
    auto& scorer = static_cast<irs::bm25_sort&>(*ptr);
  #endif

  {
    // optional float
    const auto* key = "b";

    if (json.HasMember(key)) {
      if (!json[key].IsNumber()) {
        IR_FRMT_ERROR("Non-float value in '%s' while constructing bm25 scorer from jSON arguments: %s", key, args.c_str());

        return nullptr;
      }

      scorer.b(json[key].GetFloat());
    }
  }

  {
    // optional float
    const auto* key = "k";

    if (json.HasMember(key)) {
      if (!json[key].IsNumber()) {
        IR_FRMT_ERROR("Non-float value in '%s' while constructing bm25 scorer from jSON arguments: %s", key, args.c_str());

        return nullptr;
      }

      scorer.k(json[key].GetFloat());
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

  if (size > 2) {
    // wrong number of arguments
    IR_FRMT_ERROR(
      "Wrong number of arguments while constructing bm25 scorer from jSON arguments (must be <= 2): %s",
      args.c_str()
    );
    return nullptr;
  }

  // default args
  auto k = irs::bm25_sort::K();
  auto b = irs::bm25_sort::B();

  for (rapidjson::SizeType i = 0; i < size; ++i) {
    auto& arg = array[i];

    switch (i) {
     case 0: // parse `b` coefficient
      if (!arg.IsNumber()) {
        IR_FRMT_ERROR(
          "Non-float value at position '%u' while constructing bm25 scorer from jSON arguments: %s",
          i, args.c_str()
        );

        return nullptr;
      }

      k = static_cast<float_t>(arg.GetDouble());
      break;
     case 1: // parse `b` coefficient
      if (!arg.IsNumber()) {
        IR_FRMT_ERROR(
          "Non-float value at position '%u' while constructing bm25 scorer from jSON arguments: %s",
          i, args.c_str()
        );

        return nullptr;
      }

      b = static_cast<float_t>(arg.GetDouble());
      break;
    }
  }

  return irs::memory::make_shared<irs::bm25_sort>(k, b);
}

irs::sort::ptr make_json(const irs::string_ref& args) {
  if (args.null()) {
    // default args
    return irs::memory::make_shared<irs::bm25_sort>();
  }

  rapidjson::Document json;

  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
      "Invalid jSON arguments passed while constructing bm25 scorer, arguments: %s", 
      args.c_str()
    );

    return nullptr;
  }

  switch (json.GetType()) {
    case rapidjson::kObjectType:
      return make_from_object(json, args);
    case rapidjson::kArrayType:
      return make_from_array(json, args);
    default: // wrong type
      IR_FRMT_ERROR(
        "Invalid jSON arguments passed while constructing bm25 scorer, arguments: %s", 
        args.c_str()
      );

      return nullptr;
  }
}

REGISTER_SCORER_JSON(irs::bm25_sort, make_json);

NS_END // LOCAL

NS_ROOT

// bm25 similarity
// bm25(doc, term) = idf(term) * ((k + 1) * tf(doc, term)) / (k * (1.0 - b + b * |doc|/avgDL) + tf(doc, term))

// inverted document frequency
// idf(term) = log(1 + (# documents with this field - # documents with this term + 0.5)/(# documents with this term + 0.5))

// term frequency
// tf(doc, term) = sqrt(frequency(doc, term));

// document length
// Current implementation is using the following as the document length
// |doc| = 1 / sqrt(# of terms in a field within a document)

// average document length
// avgDL = sum(field_term_count) / (# documents with this field)

NS_BEGIN(bm25)

// empty frequency
const frequency EMPTY_FREQ;

struct stats final : stored_attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();

  stats() = default;

  virtual void clear() { 
    idf = 1.f;
    norm_const = 1.f;
    norm_length = 0.f;
  }
  
  float_t idf; // precomputed idf value
  float_t norm_const; // precomputed k*(1-b)
  float_t norm_length; // precomputed k*b/avgD
}; // stats

DEFINE_ATTRIBUTE_TYPE(iresearch::bm25::stats);
DEFINE_FACTORY_DEFAULT(stats);

typedef bm25_sort::score_t score_t;

class scorer : public iresearch::sort::scorer_base<bm25::score_t> {
 public:
  DEFINE_FACTORY_INLINE(scorer);

  scorer(
      float_t k, 
      iresearch::boost::boost_t boost,
      const bm25::stats* stats,
      const frequency* freq) NOEXCEPT
    : freq_(freq ? freq : &EMPTY_FREQ),
      num_(boost * (k + 1) * (stats ? stats->idf : 1.f)),
      norm_const_(k) {
    assert(freq_);
  }

  virtual void score(byte_type* score_buf) NOEXCEPT override {
    const float_t freq = tf();
    score_cast(score_buf) = num_ * freq / (norm_const_ + freq);
  }

 protected:
  FORCE_INLINE float_t tf() const NOEXCEPT {
    return float_t(std::sqrt(freq_->value));
  };

  const frequency* freq_; // document frequency
  float_t num_; // partially precomputed numerator : boost * (k + 1) * idf
  float_t norm_const_; // 'k' factor
}; // scorer

class norm_scorer final : public scorer {
 public:
  DEFINE_FACTORY_INLINE(norm_scorer);

  norm_scorer(
      float_t k, 
      iresearch::boost::boost_t boost,
      const bm25::stats* stats,
      const frequency* freq,
      const iresearch::norm* norm) NOEXCEPT
    : scorer(k, boost, stats, freq),
      norm_(norm) {
    assert(norm_);

    // if there is no norms, assume that b==0
    if (!norm_->empty()) {
      norm_const_ = stats->norm_const;
      norm_length_ = stats->norm_length;
    }
  }

  virtual void score(byte_type* score_buf) NOEXCEPT override {
    const float_t freq = tf();
    score_cast(score_buf) = num_ * freq / (norm_const_ + norm_length_ * norm_->read() + freq);
  }

 private:
  const iresearch::norm* norm_;
  float_t norm_length_{ 0.f }; // precomputed 'k*b/avgD' if norms presetn, '0' otherwise
}; // norm_scorer

class collector final : public iresearch::sort::collector {
 public:
  collector(float_t k, float_t b) NOEXCEPT
    : k_(k), b_(b) {
  }

  virtual void collect(
      const sub_reader& /*segment*/,
      const term_reader& field,
      const attribute_view& term_attrs
  ) override {
    auto& freq = field.attributes().get<frequency>();
    auto& meta = term_attrs.get<term_meta>();

    docs_with_field += field.docs_count();

    if (freq) {
      total_term_freq += freq->value;
    }

    if (meta) {
      docs_with_term += meta->docs_count;
    }
  }

  virtual void finish(
      attribute_store& filter_attrs,
      const index_reader& /*index*/
  ) override {
    auto& bm25stats = filter_attrs.emplace<stats>();

    // precomputed idf value
    bm25stats->idf =
      float_t(std::log(1 + ((docs_with_field - docs_with_term + 0.5)/(docs_with_term + 0.5))));
    assert(bm25stats->idf >= 0);

    if (b_ == 0.f) {
      // BM11 without norms
      return;
    }

    // precomputed length norm
    const float_t kb = k_ * b_;
    bm25stats->norm_const = k_ - kb;
    bm25stats->norm_length = kb;
    if (total_term_freq && docs_with_field) {
      const auto avg_doc_len = float_t(total_term_freq) / docs_with_field;
      bm25stats->norm_length /= avg_doc_len;
    }

    // add norm attribute
    filter_attrs.emplace<norm>();
  }

 private:
  uint64_t docs_with_term = 0; // number of documents containing processed term
  uint64_t docs_with_field = 0; // number of documents containing at least one term for processed field
  uint64_t total_term_freq = 0; // number of terms for processed field
  float_t k_;
  float_t b_;
}; // collector

class sort final : iresearch::sort::prepared_basic<bm25::score_t> {
 public:
  DEFINE_FACTORY_INLINE(prepared);

  sort(float_t k, float_t b) NOEXCEPT
    : k_(k), b_(b) {
  }

  virtual const flags& features() const override {
    static const irs::flags FEATURES[] = {
      irs::flags({ irs::frequency::type() }), // without normalization
      irs::flags({ irs::frequency::type(), irs::norm::type() }), // with normalization
    };

    return FEATURES[b_ != 0.f];
  }

  virtual iresearch::sort::collector::ptr prepare_collector() const override {
    return irs::sort::collector::make<bm25::collector>(k_, b_);
  }

  virtual scorer::ptr prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_store& query_attrs,
      const attribute_view& doc_attrs
  ) const override {
    if (!doc_attrs.contains<frequency>()) {
      return nullptr; // if there is no frequency then all the scores will be the same (e.g. filter irs::all)
    }

    if (b_ != 0.f) {
      auto& norm = query_attrs.get<iresearch::norm>();

      if (norm && norm->reset(segment, field.meta().norm, *doc_attrs.get<document>())) {
        return bm25::scorer::make<bm25::norm_scorer>(
          k_,
          boost::extract(query_attrs),
          query_attrs.get<bm25::stats>().get(),
          doc_attrs.get<frequency>().get(),
          &*norm
        );
      }
    }

    // BM11
    return bm25::scorer::make<bm25::scorer>(      
      k_, 
      boost::extract(query_attrs),
      query_attrs.get<bm25::stats>().get(),
      doc_attrs.get<frequency>().get()
    );
  }

 private:
  float_t k_;
  float_t b_;
}; // sort

NS_END // bm25

DEFINE_SORT_TYPE_NAMED(iresearch::bm25_sort, "bm25");
DEFINE_FACTORY_DEFAULT(irs::bm25_sort);

bm25_sort::bm25_sort(
    float_t k /*= 1.2f*/,
    float_t b /*= 0.75f*/
): sort(bm25_sort::type()), k_(k), b_(b) {
}

/*static*/ void bm25_sort::init() {
  REGISTER_SCORER_JSON(bm25_sort, make_json); // match registration above
}

sort::prepared::ptr bm25_sort::prepare() const {
  return bm25::sort::make<bm25::sort>(k_, b_);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
