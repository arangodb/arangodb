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
    bool operator==(const simple_term& other) const noexcept {
      return term == other.term;
    }

    bstring term;
  };

  struct prefix_term {
    bool operator==(const prefix_term& other) const noexcept {
      return term == other.term;
    }

    size_t scored_terms_limit{1024};
    bstring term;
  };

  struct wildcard_term {
    bool operator==(const wildcard_term& other) const noexcept {
      return term == other.term;
    }

    size_t scored_terms_limit{1024};
    bstring term;
  };

  struct levenshtein_term {
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
    bool operator==(const set_term& other) const noexcept {
      return terms == other.terms;
    }

    std::vector<bstring> terms;
  };

 private:
  struct IRESEARCH_API info_t {
    ~info_t() {
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

    info_t();
    info_t(const info_t& other);
    info_t(info_t&& other) noexcept;
    info_t(const simple_term& st);
    info_t(simple_term&& st) noexcept;
    info_t(const prefix_term& pt);
    info_t(prefix_term&& pt) noexcept;
    info_t(const wildcard_term& wt);
    info_t(wildcard_term&& wt) noexcept;
    info_t(const levenshtein_term& lt);
    info_t(levenshtein_term&& lt) noexcept;
    info_t(const set_term& lt);
    info_t(set_term&& lt) noexcept;

    info_t& operator=(const info_t& other) noexcept;
    info_t& operator=(info_t&& other) noexcept;

    bool operator==(const info_t& other) const noexcept;

   private:
    void allocate() noexcept;
    void destroy() noexcept;
    void recreate(PhrasePartType new_type) noexcept;
  };

  // positions and terms
  typedef std::map<size_t, info_t> terms_t;
  typedef terms_t::value_type term_t;

  friend size_t hash_value(const by_phrase::info_t& info);

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

  bool get(size_t pos, simple_term& t) {
    const auto& inf = phrase_.at(pos);
    if (PhrasePartType::TERM != inf.type) {
      return false;
    }
    t = inf.st;
    return true;
  }

  bool get(size_t pos, prefix_term& t) {
    const auto& inf = phrase_.at(pos);
    if (PhrasePartType::PREFIX != inf.type) {
      return false;
    }
    t = inf.pt;
    return true;
  }

  bool get(size_t pos, wildcard_term& t) {
    const auto& inf = phrase_.at(pos);
    if (PhrasePartType::WILDCARD != inf.type) {
      return false;
    }
    t = inf.wt;
    return true;
  }

  bool get(size_t pos, levenshtein_term& t) {
    const auto& inf = phrase_.at(pos);
    if (PhrasePartType::LEVENSHTEIN != inf.type) {
      return false;
    }
    t = inf.lt;
    return true;
  }

  bool get(size_t pos, set_term& t) {
    const auto& inf = phrase_.at(pos);
    if (PhrasePartType::SET != inf.type) {
      return false;
    }
    t = inf.ct;
    return true;
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
