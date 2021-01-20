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

#ifndef IRESEARCH_FST_H
#define IRESEARCH_FST_H

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

#include "shared.hpp"
#include "utils/fstext/fst_states_map.hpp"
#include "utils/string.hpp"
#include "utils/noncopyable.hpp"

namespace iresearch {

struct fst_stats {
  size_t num_states{}; // total number of states
  size_t num_arcs{};   // total number of arcs

  template<typename Weight>
  void operator()(const Weight&) noexcept { }
};

//////////////////////////////////////////////////////////////////////////////
/// @class fst_builder
/// @brief helper class for building minimal acyclic subsequential transducers
///        algorithm is described there:
///        http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.24.3698
//////////////////////////////////////////////////////////////////////////////
template<typename Char, typename Fst, typename Stats = fst_stats>
class fst_builder : util::noncopyable {
 public:
  typedef Fst fst_t;
  typedef Char char_t;
  typedef basic_string_ref<char_t> key_t;
  typedef Stats stats_t;
  typedef typename fst_t::Weight weight_t;
  typedef typename fst_t::Arc arc_t;
  typedef typename fst_t::StateId stateid_t;
  typedef typename arc_t::Label label_t;

  static constexpr stateid_t final = 0;

  explicit fst_builder(fst_t& fst)
    : states_map_(16, state_emplace(stats_)), fst_(fst) {
    reset();
  }

  void add(const key_t& in, const weight_t& out) {
    // inputs should be sorted
    assert(last_.empty() || last_ < in);

    if (in.empty()) {
      start_out_ = fst::Times(start_out_, out);
      return;
    }

    const auto size = in.size();

    // determine common prefix
    const size_t pref = 1 + common_prefix_length(last_, in);

    // add states for current input
    add_states(size);

    // minimize last word suffix
    minimize(pref);

    // add current word suffix
    for (size_t i = pref; i <= size; ++i) {
      states_[i - 1].arcs.emplace_back(in[i - 1], &states_[i]);
    }

    const bool is_final = last_.size() != size || pref != (size + 1);

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
        state& s = states_[size];
        s.final = true;
      }

      // set output
      {
        state& s = states_[pref - 1];
        assert(!s.arcs.empty() && s.arcs.back().label == in[pref-1]);
        s.arcs.back().out = std::move(output);
      }
    } else {
      state& s = states_[size];
      assert(s.arcs.size());
      assert(s.arcs.back().label == in[pref - 1]);
      s.arcs.back().out = fst::Times(s.arcs.back().out, output);
    }

    last_ = in;
  }

  stats_t finish() {
    stateid_t start = fst_builder::final;

    if (!states_.empty()) {
      // minimize last word suffix
      minimize(1);

      auto& root = states_[0];

      if (!root.arcs.empty() || !root.final) {
        start = states_map_.insert(root, fst_);
      }
    }

    // set the start state
    fst_.SetStart(start);
    fst_.SetFinal(start, start_out_);

    // count start state
    stats_(start_out_);

    return stats_;
  }

  void reset() {
    // remove states
    fst_.DeleteStates();

    // initialize final state
    fst_.AddState();
    fst_.SetFinal(final, weight_t::One());

    // reset stats
    stats_ = {};
    stats_.num_states = 1;
    stats_.num_arcs = 0;
    stats_(weight_t::One());

    states_.clear();
    states_map_.reset();
    last_ = {};
    start_out_ = weight_t{};
  }

 private:
  struct state;

  struct arc : private util::noncopyable {
    arc(label_t label, state* target)
      : target(target),
        label(label) {
    }

    arc(arc&& rhs) noexcept
      : target(rhs.target),
        label(rhs.label),
        out(std::move(rhs.out)) {
    }
    
    bool operator==(const arc_t& rhs) const noexcept {
      return label == rhs.ilabel
        && id == rhs.nextstate
        && out == rhs.weight;
    }

    bool operator!=(const arc_t& rhs) const noexcept {
      return !(*this == rhs);
    }

    union {
      state* target;
      stateid_t id;
    };
    label_t label;
    weight_t out{ weight_t::One() };
  }; // arc

  struct state : private util::noncopyable {
    explicit state(bool final = false)
      : final(final) { }

    state(state&& rhs) noexcept
      : arcs(std::move(rhs.arcs)),
        out(std::move(rhs.out)),
        final(rhs.final) {
    }

    void clear() noexcept {
      arcs.clear();
      out = weight_t::One();
      final = false;
    }

    std::vector<arc> arcs;
    weight_t out{ weight_t::One() };
    bool final{ false };
  }; // state

  static_assert(std::is_nothrow_move_constructible<state>::value,
                "default move constructor expected");

  struct state_equal {
    bool operator()(const state& lhs, stateid_t rhs, const fst_t& fst) const {
      if (lhs.arcs.size() != fst.NumArcs(rhs)) {
        return false;
      }

      fst::ArcIterator<fst_t> rhs_arc(fst, rhs);

      for (auto& lhs_arc : lhs.arcs) {
        if (lhs_arc != rhs_arc.Value()) {
          return false;
        }

        rhs_arc.Next();
      }

      assert(rhs_arc.Done());
      return true;
    }
  };

  struct state_hash {
    size_t operator()(const state& s, const fst_t& /*fst*/) const noexcept {
      size_t hash = 0;

      for (auto& a: s.arcs) {
        ::boost::hash_combine(hash, a.label);
        ::boost::hash_combine(hash, a.id);
        ::boost::hash_combine(hash, a.out.Hash());
      }

      return hash;
    }

    size_t operator()(stateid_t id, const fst_t& fst) const noexcept {
      size_t hash = 0;

      for (fst::ArcIterator<fst_t> it(fst, id); !it.Done(); it.Next()) {
        const arc_t& a = it.Value();
        ::boost::hash_combine(hash, a.ilabel);
        ::boost::hash_combine(hash, a.nextstate);
        ::boost::hash_combine(hash, a.weight.Hash());
      }

      return hash;
    }
  };

  class state_emplace {
   public:
    explicit state_emplace(stats_t& stats) noexcept
      : stats_(&stats) {
    }

    stateid_t operator()(const state& s, fst_t& fst) const {
      const stateid_t id = fst.AddState();

      if (s.final) {
        fst.SetFinal(id, s.out);
        (*stats_)(s.out);
      }

      for (const arc& a : s.arcs) {
        fst.EmplaceArc(id, a.label, a.label, a.out, a.id);
        (*stats_)(a.out);
      }

      ++stats_->num_states;
      stats_->num_arcs += s.arcs.size();

      return id;
    }

   private:
    stats_t* stats_;
  };

  using states_map = fst_states_map<
    fst_t, state,
    state_emplace, state_hash,
    state_equal, fst::kNoStateId>;

  void add_states(size_t size) {
    // reserve size + 1 for root state
    if (states_.size() < ++size) {
      states_.resize(size);
    }
  }

  void minimize(size_t pref) {
    assert(pref > 0);

    for (size_t i = last_.size(); i >= pref; --i) {
      state& s = states_[i];
      state& p = states_[i - 1];

      assert(!p.arcs.empty());
      p.arcs.back().id = s.arcs.empty() && s.final
        ? fst_builder::final
        : states_map_.insert(s, fst_);

      s.clear();
    }
  }

  stats_t stats_;
  states_map states_map_;
  std::vector<state> states_; // current states
  weight_t start_out_; // output for "empty" input
  key_t last_;
  fst_t& fst_;
}; // fst_builder

}

#endif
