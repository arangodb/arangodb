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

#include "filter_visitor.hpp"
#include "levenshtein_filter.hpp"
#include "utils/levenshtein_default_pdp.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class phrase_state
/// @brief cached per reader phrase state
//////////////////////////////////////////////////////////////////////////////
template<template<typename...> class T>
struct phrase_state {
  typedef seek_term_iterator::cookie_ptr term_state_t;
  typedef T<term_state_t> terms_states_t;

  phrase_state() = default;

  phrase_state(phrase_state&& rhs) noexcept
    : terms(std::move(rhs.terms)),
      reader(rhs.reader) {
    rhs.reader = nullptr;
  }

  phrase_state& operator=(const phrase_state&) = delete;

  terms_states_t terms;
  const term_reader* reader{};
}; // phrase_state

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

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wplacement-new="
#endif

    template<typename PhrasePart>
    phrase_part(PhrasePart&& other) noexcept(std::is_rvalue_reference<PhrasePart>::value) {
      type = std::remove_reference<PhrasePart>::type::type;
      new (reinterpret_cast<typename std::remove_reference<PhrasePart>::type*>(&st))
        typename std::remove_reference<PhrasePart>::type(std::forward<PhrasePart>(other));
    }

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

    phrase_part& operator=(const phrase_part& other) noexcept;
    phrase_part& operator=(phrase_part&& other) noexcept;

    bool operator==(const phrase_part& other) const noexcept;

    //////////////////////////////////////////////////////////////////////////////
    /// @class phrase_term_visitor
    /// @brief filter visitor for phrase queries
    //////////////////////////////////////////////////////////////////////////////
    template<typename Collectors>
    class phrase_term_visitor final : public filter_visitor {
     public:
      phrase_term_visitor(
        const sub_reader& segment,
        const term_reader& reader,
        const Collectors& collectors
      ) : segment_(segment), reader_(reader), collectors_(collectors) {}

      phrase_term_visitor(
        const sub_reader& segment,
        const term_reader& reader,
        const Collectors& collectors,
        phrase_state<order::prepared::FixedContainer>::terms_states_t& phrase_terms
      ) : phrase_term_visitor(segment, reader, collectors) {
        phrase_terms_ = &phrase_terms;
      }

      virtual void prepare(const seek_term_iterator::ptr& terms) noexcept override {
        terms_ = &terms;
        found_ = true;
      }

      virtual void visit() override {
        assert(terms_);
        collectors_.collect(segment_, reader_, term_offset_, (*terms_)->attributes()); // collect statistics

        // estimate phrase & term
        assert(phrase_terms_);
        phrase_terms_->emplace_back((*terms_)->cookie());
      }

      void reset() noexcept {
        found_ = false;
        terms_ = nullptr;
      }

      void reset(size_t term_offset) noexcept {
        reset();
        term_offset_ = term_offset;
      }

      void reset(phrase_state<order::prepared::FixedContainer>::terms_states_t& phrase_terms, size_t term_offset) noexcept {
        reset(term_offset);
        phrase_terms_ = &phrase_terms;
      }

      bool found() const noexcept { return found_; }

     private:
      bool found_ = false;
      size_t term_offset_ = 0;
      const sub_reader& segment_;
      const term_reader& reader_;
      const Collectors& collectors_;
      phrase_state<order::prepared::FixedContainer>::terms_states_t* phrase_terms_ = nullptr;
      const seek_term_iterator::ptr* terms_ = nullptr;
    };

    static bool variadic_type_collect(
      const term_reader& reader,
      const phrase_part& phr_part,
      phrase_term_visitor<order::prepared::variadic_terms_collectors>& ptv);

   private:
    void allocate(const phrase_part& other);
    void allocate(phrase_part&& other) noexcept;
    void destroy() noexcept;
    void recreate(const phrase_part& other);
    void recreate(phrase_part&& other) noexcept;
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
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    order::prepared::fixed_terms_collectors collectors) const;

  filter::prepared::ptr variadic_prepare_collect(
    const index_reader& index,
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
