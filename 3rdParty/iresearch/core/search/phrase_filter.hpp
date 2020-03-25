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

#ifndef IRESEARCH_PHRASE_FILTER_H
#define IRESEARCH_PHRASE_FILTER_H

#include <map>

#include "filter.hpp"
#include "levenshtein_filter.hpp"
#include "utils/levenshtein_default_pdp.hpp"
#include "utils/string.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class by_phrase
/// @brief user-side phrase filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_phrase : public filter {
 public:
  enum class PhrasePartType {
    TERM, PREFIX, WILDCARD, LEVENSHTEIN, SET
  };

  struct simple_term {
    static constexpr PhrasePartType type = PhrasePartType::TERM;

    bool operator==(const simple_term& other) const noexcept {
      return term == other.term;
    }

    bstring term;
  };

  struct prefix_term {
    static constexpr PhrasePartType type = PhrasePartType::PREFIX;

    bool operator==(const prefix_term& other) const noexcept {
      return term == other.term;
    }

    size_t scored_terms_limit{1024};
    bstring term;
  };

  struct wildcard_term {
    static constexpr PhrasePartType type = PhrasePartType::WILDCARD;

    bool operator==(const wildcard_term& other) const noexcept {
      return term == other.term;
    }

    size_t scored_terms_limit{1024};
    bstring term;
  };

  struct levenshtein_term {
    static constexpr PhrasePartType type = PhrasePartType::LEVENSHTEIN;

    bool operator==(const levenshtein_term& other) const noexcept {
      return with_transpositions == other.with_transpositions &&
          max_distance == other.max_distance &&
          provider == other.provider &&
          term == other.term;
    }

    bool with_transpositions{false};
    byte_type max_distance{0};
    size_t scored_terms_limit{1024};
    by_edit_distance::pdp_f provider{irs::default_pdp};
    bstring term;
  };

  struct set_term {
    static constexpr PhrasePartType type = PhrasePartType::SET;

    bool operator==(const set_term& other) const noexcept {
      return terms == other.terms;
    }

    std::vector<bstring> terms;
  };

 private:
  struct IRESEARCH_API phrase_part {
    ~phrase_part() {
      destroy();
    }

    PhrasePartType type;

    union {
      simple_term st;
      prefix_term pt;
      wildcard_term wt;
      levenshtein_term lt;
      set_term ct;
    };

    phrase_part();
    phrase_part(const phrase_part& other);
    phrase_part(phrase_part&& other) noexcept;
    phrase_part(const simple_term& st);
    phrase_part(simple_term&& st) noexcept;
    phrase_part(const prefix_term& pt);
    phrase_part(prefix_term&& pt) noexcept;
    phrase_part(const wildcard_term& wt);
    phrase_part(wildcard_term&& wt) noexcept;
    phrase_part(const levenshtein_term& lt);
    phrase_part(levenshtein_term&& lt) noexcept;
    phrase_part(const set_term& lt);
    phrase_part(set_term&& lt) noexcept;

    phrase_part& operator=(const phrase_part& other) noexcept;
    phrase_part& operator=(phrase_part&& other) noexcept;

    bool operator==(const phrase_part& other) const noexcept;

   private:
    void allocate() noexcept;
    void destroy() noexcept;
    void recreate(PhrasePartType new_type) noexcept;
  };

  // positions and terms
  typedef std::map<size_t, phrase_part> terms_t;
  typedef terms_t::value_type term_t;

  friend size_t hash_value(const by_phrase::phrase_part& info);

 public:
  // returns set of features required for filter
  static const flags& required();

  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY();

  by_phrase();

  by_phrase& field(std::string fld) noexcept {
    fld_ = std::move(fld);
    return *this;
  }

  const std::string& field() const noexcept { return fld_; }

  // inserts term to the specified position
  template<typename PhrasePart>
  by_phrase& insert(PhrasePart&& t, size_t pos) {
    is_simple_term_only_ &= std::is_same<PhrasePart, simple_term>::value; // constexpr
    phrase_[pos] = std::forward<PhrasePart>(t);
    return *this;
  }

  template<typename PhrasePart>
  by_phrase& push_back(PhrasePart&& t, size_t offs = 0) {
    return insert(std::forward<PhrasePart>(t), next_pos() + offs);
  }

  template<typename PhrasePart>
  const PhrasePart* get(size_t pos) const noexcept {
    const auto& inf = phrase_.at(pos);
    if (inf.type != PhrasePart::type) {
      return nullptr;
    }
    return reinterpret_cast<const PhrasePart*>(&inf.st);
  }

  bool empty() const noexcept { return phrase_.empty(); }
  size_t size() const noexcept { return phrase_.size(); }

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& ctx
  ) const override;

  virtual size_t hash() const noexcept override;

 protected:
  virtual bool equals(const filter& rhs) const noexcept override;

 private:
  size_t next_pos() const {
    return phrase_.empty() ? 0 : 1 + phrase_.rbegin()->first;
  }

  size_t first_pos() const {
    return phrase_.empty() ? 0 : phrase_.begin()->first;
  }

  filter::prepared::ptr fixed_prepare_collect(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost,
      order::prepared::fixed_terms_collectors collectors) const;

  filter::prepared::ptr variadic_prepare_collect(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost,
      order::prepared::variadic_terms_collectors collectors) const;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string fld_;
  terms_t phrase_;
  bool is_simple_term_only_{true};
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // by_phrase

NS_END // ROOT

#endif
