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

#ifndef IRESEARCH_FST_H
#define IRESEARCH_FST_H

#include "shared.hpp"
#include "utils/string.hpp"
#include "utils/noncopyable.hpp"

#if defined(_MSC_VER)
  #pragma warning(disable : 4018)
  #pragma warning(disable : 4100)
  #pragma warning(disable : 4244)
  #pragma warning(disable : 4245)
  #pragma warning(disable : 4267)
  #pragma warning(disable : 4291)
  #pragma warning(disable : 4389)
  #pragma warning(disable : 4396)
  #pragma warning(disable : 4512)
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wsign-compare"
  #pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif

#include <fst/vector-fst.h>

#if defined(_MSC_VER)
  #pragma warning(default : 4512)
  #pragma warning(default : 4396)
  #pragma warning(default : 4389)
  #pragma warning(default : 4291)
  #pragma warning(default : 4267)
  #pragma warning(default : 4245)
  #pragma warning(default : 4244)
  #pragma warning(default : 4100)
  #pragma warning(default : 4018)
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include <boost/functional/hash.hpp>

NS_ROOT
//////////////////////////////////////////////////////////////////////////////
/// @class fst_builder
/// @brief helper class for building minimal acyclic subsequential transducers
///        algorithm is described there:
///        http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.24.3698
//////////////////////////////////////////////////////////////////////////////
template<typename Char, typename Fst>
class fst_builder : util::noncopyable {
 public:
  typedef Fst fst_t;
  typedef Char char_t;
  typedef basic_string_ref<char_t> key_t;
  typedef typename fst_t::Weight weight_t;
  typedef typename fst_t::Arc arc_t;
  typedef typename fst_t::StateId stateid_t;
  typedef typename arc_t::Label label_t;

  static const stateid_t final = 0;

  explicit fst_builder(fst_t& fst) : fst_(fst) {
    // initialize final state
    fst_.AddState();
    fst_.SetFinal(final, weight_t::One());
  }

  void add(const key_t& in, const weight_t& out) {
    // inputs should be sorted
    assert(last_.empty() || last_ < in);

    if (in.empty()) {
      start_out_ = fst::Times(start_out_, out);
      return;
    }

    // determine common prefix
    const size_t pref = 1 + prefix(last_, in);

    // add states for current input
    add_states(in.size());

    // minimize last word suffix
    minimize(pref);

    // add current word suffix
    for (size_t i = pref; i <= in.size(); ++i) {
      states_[i - 1].arcs.emplace_back(in[i - 1], &states_[i]);
    }

    const bool is_final = last_.size() != in.size() || pref != (in.size() + 1);

    decltype(fst::DivideLeft(out, out)) output = out;

    for (size_t i = 1; i < pref; ++i) {
      state& s = states_[i];
      state& p = states_[i - 1];

      assert(!p.arcs.empty() && p.arcs.back().label == in[i - 1]);

      auto& last_out = p.arcs.back().out;

      if (last_out != weight_t::One()) {
        auto prefix = fst::Plus(last_out, out);
        const auto suffix = fst::DivideLeft(last_out, prefix);

        for (arc& a : s.arcs) {
          a.out = fst::Times(suffix, a.out);
        }

        if (s.final) {
          s.out = fst::Times(suffix, s.out);
        }

        last_out = std::move(prefix);
        output = fst::DivideLeft(output, last_out);
      }
    }

    if (is_final) {
      // set final state
      {
        state& s = states_[in.size()];
        s.final = true;
      }

      // set output
      {
        state& s = states_[pref - 1];
        assert(!s.arcs.empty() && s.arcs.back().label == in[pref-1]);
        s.arcs.back().out = std::move(output);
      }
    } else {
      state& s = states_[in.size()];
      assert(s.arcs.size());
      assert(s.arcs.back().label == in[pref - 1]);
      s.arcs.back().out = fst::Times(s.arcs.back().out, output);
    }

    last_ = in;
  }

  void finish() {
    stateid_t start;
    if (states_.empty()) {
      start = final;
    } else {
      // minimize last word suffix
      minimize(1);
      start = states_map_.insert(states_[0], fst_);
    }

    // set the start state
    fst_.SetStart(start);
    fst_.SetFinal(start, start_out_);
  }

  void reset() {
    // remove states
    fst_.DeleteStates();

    // initialize final state
    fst_.AddState();
    fst_.SetFinal(final, weight_t::One());

    states_map_.reset();
    last_ = key_t();
    start_out_ = weight_t();
  }

 private:
  struct state;

  struct arc : private util::noncopyable {
    arc(label_t label, state* target)
      : target(target),
        label(label) {
    }

    arc(arc&& rhs) NOEXCEPT
      : target(rhs.target),
        label(rhs.label),
        out(std::move(rhs.out)) {
    }
    
    bool operator==(const arc_t& rhs) const {
      return label == rhs.ilabel
        && id == rhs.nextstate
        && out == rhs.weight;
    }

    bool operator!=(const arc_t& rhs) const {
      return !(*this == rhs);
    }

    friend size_t hash_value(const arc& a) {
      size_t hash = 0;
      ::boost::hash_combine(hash, a.label);
      ::boost::hash_combine(hash, a.id);
      ::boost::hash_combine(hash, a.out.Hash());
      return hash;
    }

    union {
      state* target;
      stateid_t id;
    };
    label_t label;
    weight_t out{ weight_t::One() };
  }; // arc

  struct state : private util::noncopyable {
    state() = default;

    state(state&& rhs) NOEXCEPT
      : arcs(std::move(rhs.arcs)),
        out(std::move(rhs.out)),
        final(rhs.final) {
    }

    void clear() {
      arcs.clear();
      final = false;
      out = weight_t::One();
    }

    friend size_t hash_value(const state& s) { 
      size_t seed = 0;

      for (auto& arc: s.arcs) {
        ::boost::hash_combine(seed, arc);
      }

      return seed;
    }

    std::vector<arc> arcs;
    weight_t out{ weight_t::One() };
    bool final{ false };
  }; // state

  class state_map : private util::noncopyable {
   public:
    static const size_t InitialSize = 16;

    state_map(): states_(InitialSize, fst::kNoStateId) {}
    stateid_t insert(const state& s, fst_t& fst) {
      if (s.arcs.empty() && s.final) {
        return fst_builder::final;
      }

      stateid_t id;
      const size_t mask = states_.size() - 1;
      size_t pos = hash_value(s) % mask;
      for ( ;; ++pos, pos %= mask ) { // TODO: maybe use quadratic probing here
        if (fst::kNoStateId == states_[pos]) {
          states_[pos] = id = add_state(s, fst);
          ++count_;

          if (count_ > 2 * states_.size() / 3) {
            rehash(fst);
          }
          break;
        } else if (equals( s, states_[pos], fst)) {
          id = states_[pos];
          break;
        }
      }

      return id;
    }

    void reset() {
      count_ = 0;
      std::fill(states_.begin(), states_.end(), fst::kNoStateId);
    }

   private:
    static bool equals(const state& lhs, stateid_t rhs, const fst_t& fst) {
      if (fst.NumArcs( rhs ) != lhs.arcs.size() ) {
        return false;
      }

      for (fst::ArcIterator<fst_t>it(fst, rhs); !it.Done(); it.Next()) {
        if (lhs.arcs[it.Position()] !=  it.Value()) {
          return false;
        }
      }
      return true;
    }

    static size_t hash(stateid_t id, const fst_t& fst) {
      size_t hash = 0;
      for (fst::ArcIterator< fst_t > it(fst, id); !it.Done(); it.Next()) {
        const arc_t& a = it.Value();
        ::boost::hash_combine(hash, a.ilabel);
        ::boost::hash_combine(hash, a.nextstate);
        ::boost::hash_combine(hash, a.weight.Hash());
      }
      return hash;
    }

    void rehash(const fst_t& fst) {
      std::vector< stateid_t > states(states_.size() * 2, fst::kNoStateId);
      const size_t mask = states.size() - 1;
      for (stateid_t id : states_) {

        if (fst::kNoStateId == id) {
          continue;
        }

        size_t pos = hash(id, fst) % mask;
        for (;;++pos, pos %= mask) { // TODO: maybe use quadratic probing here
          if (fst::kNoStateId == states[pos] ) {
            states[pos] = id;
            break;
          }
        }
      }

      states_ = std::move(states);
    }

    stateid_t add_state(const state& s, fst_t& fst) {
      const stateid_t id = fst.AddState();
      if (s.final) {
        fst.SetFinal(id, s.out);
      }

      for (const arc& a : s.arcs) {
        fst.AddArc(id, arc_t(a.label, a.label, a.out, a.id));
      }

      return id;
    }

    // TODO: maybe use "buckets" here
    std::vector<stateid_t> states_;
    size_t count_{};
  }; // state_map

  static size_t prefix(const key_t& lhs, const key_t& rhs) {
    size_t pref = 0;
    const size_t max = std::min( lhs.size(), rhs.size() );
    while ( pref < max && lhs[pref] == rhs[pref] ) {
      ++pref;
    }
    return pref;
  }

  void add_states(size_t size) {
    // reserve size + 1 for root state
    if ( states_.size() < ++size ) {
      states_.resize(size);
    }
  }

  void minimize(size_t pref) {
    assert(pref > 0);

    for (size_t i = last_.size(); i >= pref; --i) {
      state& s = states_[i];
      state& p = states_[i - 1];
      assert(!p.arcs.empty());

      p.arcs.back().id = states_map_.insert(s, fst_);
      s.clear();
    }
  }

  state_map states_map_;
  std::vector<state> states_; // current states
  weight_t start_out_; // output for "empty" input
  key_t last_;
  fst_t& fst_;
}; // fst_builder

NS_END

#endif
