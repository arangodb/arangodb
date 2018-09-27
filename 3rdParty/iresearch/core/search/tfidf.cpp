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

#include "tfidf.hpp"

#include "scorers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"

NS_LOCAL

irs::sort::ptr make_from_bool(
    const rapidjson::Document& json,
    const irs::string_ref& //args
) {
  assert(json.IsBool());

  PTR_NAMED(irs::tfidf_sort, ptr, json.GetBool());
  return ptr;
}

irs::sort::ptr make_from_object(
    const rapidjson::Document& json,
    const irs::string_ref& args) {
  assert(json.IsObject());

  PTR_NAMED(irs::tfidf_sort, ptr);

  #ifdef IRESEARCH_DEBUG
    auto& scorer = dynamic_cast<irs::tfidf_sort&>(*ptr);
  #else
    auto& scorer = static_cast<irs::tfidf_sort&>(*ptr);
  #endif

  {
    // optional bool
    const auto* key= "with-norms";

    if (json.HasMember(key)) {
      if (!json[key].IsBool()) {
        IR_FRMT_ERROR("Non-boolean value in '%s' while constructing tfidf scorer from jSON arguments: %s", key, args.c_str());

        return nullptr;
      }

      scorer.normalize(json[key].GetBool());
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

  // parse `with-norms` optional argument
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

  PTR_NAMED(irs::tfidf_sort, ptr, norms);
  return ptr;
}

irs::sort::ptr make_json(const irs::string_ref& args) {
  if (args.null()) {
    PTR_NAMED(irs::tfidf_sort, ptr);
    return ptr;
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

NS_END // LOCAL

NS_ROOT
NS_BEGIN(tfidf)

// empty frequency
const frequency EMPTY_FREQ;

struct idf final : basic_stored_attribute<float_t> {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();
  idf() : basic_stored_attribute(1.f) { }

  void clear() { value = 1.f; }
};

DEFINE_ATTRIBUTE_TYPE(iresearch::tfidf::idf);
DEFINE_FACTORY_DEFAULT(idf);

typedef tfidf_sort::score_t score_t;

class collector final : public iresearch::sort::collector {
 public:
  collector(bool normalize)
    : normalize_(normalize) {
  }

  virtual void collect(
      const sub_reader& /*segment*/,
      const term_reader& field,
      const attribute_view& term_attrs
  ) override {
    auto& meta = term_attrs.get<iresearch::term_meta>();

    docs_with_field += field.docs_count();

    if (meta) {
      docs_with_term += meta->docs_count;
    }
  }

  virtual void finish(
      attribute_store& filter_attrs,
      const iresearch::index_reader& /*index*/
  ) override {
    filter_attrs.emplace<tfidf::idf>()->value =
      float_t(std::log((docs_with_field + 1) / double_t(docs_with_term + 1)) + 1.0);

    // add norm attribute if requested
    if (normalize_) {
      filter_attrs.emplace<norm>();
    }
  }

 private:
  uint64_t docs_with_field = 0; // number of documents containing at least one term for processed field
  uint64_t docs_with_term = 0; // number of documents containing processed term
  bool normalize_;
}; // collector

class scorer : public iresearch::sort::scorer_base<tfidf::score_t> {
 public:
  DECLARE_FACTORY(scorer);

  scorer(
      iresearch::boost::boost_t boost,
      const tfidf::idf* idf,
      const frequency* freq)
    : idf_(boost * (idf ? idf->value : 1.f)), 
      freq_(freq ? freq : &EMPTY_FREQ) {
    assert(freq_);
  }

  virtual void score(byte_type* score_buf) override {
    score_cast(score_buf) = tfidf();
  }

 protected:
  FORCE_INLINE float_t tfidf() const {
   return idf_ * float_t(std::sqrt(freq_->value));
  }

 private:
  float_t idf_; // precomputed : boost * idf
  const frequency* freq_;
}; // scorer

class norm_scorer final : public scorer {
 public:
  DECLARE_FACTORY(norm_scorer);

  norm_scorer(
      const iresearch::norm* norm,
      iresearch::boost::boost_t boost,
      const tfidf::idf* idf,
      const frequency* freq)
    : scorer(boost, idf, freq),
      norm_(norm) {
    assert(norm_);
  }

  virtual void score(byte_type* score_buf) override {
    score_cast(score_buf) = tfidf() * norm_->read();
  }

 private:
  const iresearch::norm* norm_;
}; // norm_scorer

class sort final: iresearch::sort::prepared_base<tfidf::score_t> {
 public:
  DECLARE_FACTORY(prepared);

  sort(bool normalize) NOEXCEPT
    : normalize_(normalize) {
  }

  virtual const flags& features() const override {
    static const irs::flags FEATURES[] = {
      irs::flags({ irs::frequency::type() }), // without normalization
      irs::flags({ irs::frequency::type(), irs::norm::type() }), // with normalization
    };

    return FEATURES[normalize_];
  }

  virtual collector::ptr prepare_collector() const override {
    return iresearch::sort::collector::make<tfidf::collector>(normalize_);
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

    auto& norm = query_attrs.get<iresearch::norm>();

    if (norm && norm->reset(segment, field.meta().norm, *doc_attrs.get<document>())) {
      return tfidf::scorer::make<tfidf::norm_scorer>(
        &*norm,
        boost::extract(query_attrs),
        query_attrs.get<tfidf::idf>().get(),
        doc_attrs.get<frequency>().get()
      );
    }

    return tfidf::scorer::make<tfidf::scorer>(
      boost::extract(query_attrs),
      query_attrs.get<tfidf::idf>().get(),
      doc_attrs.get<frequency>().get()
    );
  }

  virtual void add(score_t& dst, const score_t& src) const override {
    dst += src;
  }

  virtual bool less(const score_t& lhs, const score_t& rhs) const override {
    return lhs < rhs;
  }

 private:
  const std::function<bool(score_t, score_t)>* less_;
  bool normalize_;
}; // sort

NS_END // tfidf 

DEFINE_SORT_TYPE_NAMED(iresearch::tfidf_sort, "tfidf");
DEFINE_FACTORY_DEFAULT(irs::tfidf_sort);

tfidf_sort::tfidf_sort(bool normalize) 
  : sort(tfidf_sort::type()),
    normalize_(normalize) {
}

/*static*/ void tfidf_sort::init() {
  REGISTER_SCORER_JSON(tfidf_sort, make_json); // match registration above
}

sort::prepared::ptr tfidf_sort::prepare() const {
  return tfidf::sort::make<tfidf::sort>(normalize_);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
