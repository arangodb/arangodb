// replace-util.h


// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// Author: riley@google.com (Michael Riley)
//

// \file
// Utility classes for the recursive replacement of Fsts (RTNs).

#ifndef FST_LIB_REPLACE_UTIL_H__
#define FST_LIB_REPLACE_UTIL_H__

#include <vector>
using std::vector;
#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <unordered_set>
using std::unordered_set;
using std::unordered_multiset;
#include <map>

#include <fst/connect.h>
#include <fst/mutable-fst.h>
#include <fst/topsort.h>
#include <fst/vector-fst.h>


namespace fst {

// This specifies what labels to output on the call or return arc
// REPLACE_LABEL_NEITHER puts epsilon labels on both input and output
// REPLACE_LABEL_BOTH puts non-epsilon labels on both input and output
// REPLACE_LABEL_INPUT puts non-epsilon on input and epsilon on output
// REPLACE_LABEL_OUTPUT puts epsilon on input and non-epsilon on output
// Note that REPLACE_LABEL_INPUT and REPLACE_LABEL_OUTPUT produce transducers
enum ReplaceLabelType { REPLACE_LABEL_NEITHER = 1, REPLACE_LABEL_INPUT = 2,
                        REPLACE_LABEL_OUTPUT = 3, REPLACE_LABEL_BOTH = 4 };

// By default ReplaceUtil will copy the input label of the 'replace arc'.
// The call_label_type and return_label_type options specify how to manage
// the labels of the call arc and the return arc of the replace FST
struct ReplaceUtilOptions {
  int64 root;    // root rule for expansion
  ReplaceLabelType call_label_type;  // how to label call arc
  ReplaceLabelType return_label_type;  // how to label return arc
  int64 return_label;  // specifies label to put on return arc

  ReplaceUtilOptions(int64 r, ReplaceLabelType call_label_type,
                     ReplaceLabelType return_label_type, int64 return_label)
      : root(r),
        call_label_type(call_label_type),
        return_label_type(return_label_type),
        return_label(return_label) {}

  explicit ReplaceUtilOptions(int64 r)
      : root(r),
        call_label_type(REPLACE_LABEL_INPUT),
        return_label_type(REPLACE_LABEL_NEITHER),
        return_label(0) {}

  ReplaceUtilOptions(int64 r, bool epsilon_replace_arc)  // b/w compatibility
      : root(r),
        call_label_type((epsilon_replace_arc) ? REPLACE_LABEL_NEITHER :
                        REPLACE_LABEL_INPUT),
        return_label_type(REPLACE_LABEL_NEITHER),
        return_label(0) {}

  ReplaceUtilOptions()
      : root(kNoLabel),
        call_label_type(REPLACE_LABEL_INPUT),
        return_label_type(REPLACE_LABEL_NEITHER),
        return_label(0) {}
};

template <class Arc>
void Replace(const vector<pair<typename Arc::Label, const Fst<Arc>* > >&,
             MutableFst<Arc> *, fst::ReplaceUtilOptions opts);

// Utility class for the recursive replacement of Fsts (RTNs). The
// user provides a set of Label, Fst pairs at construction. These are
// used by methods for testing cyclic dependencies and connectedness
// and doing RTN connection and specific Fst replacement by label or
// for various optimization properties. The modified results can be
// obtained with the GetFstPairs() or GetMutableFstPairs() methods.
template <class Arc>
class ReplaceUtil {
 public:
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;

  typedef pair<Label, const Fst<Arc>*> FstPair;
  typedef pair<Label, MutableFst<Arc>*> MutableFstPair;
  typedef unordered_map<Label, Label> NonTerminalHash;

  // Constructs from mutable Fsts; Fst ownership given to ReplaceUtil.
  ReplaceUtil(const vector<MutableFstPair> &fst_pairs,
              const ReplaceUtilOptions &opts);

  // Constructs from Fsts; Fst ownership retained by caller.
  ReplaceUtil(const vector<FstPair> &fst_pairs,
              const ReplaceUtilOptions &opts);

  // Constructs from ReplaceFst internals; ownership retained by caller.
  ReplaceUtil(const vector<const Fst<Arc> *> &fst_array,
              const NonTerminalHash &nonterminal_hash,
              const ReplaceUtilOptions &opts);

  ~ReplaceUtil() {
    for (Label i = 0; i < fst_array_.size(); ++i)
      delete fst_array_[i];
  }

  // True if the non-terminal dependencies are cyclic. Cyclic
  // dependencies will result in an unexpandable replace fst.
  bool CyclicDependencies() const {
    GetDependencies(false);
    return depprops_ & kCyclic;
  }

  // Returns true if no useless Fsts, states or transitions.
  bool Connected() const {
    GetDependencies(false);
    uint64 props = kAccessible | kCoAccessible;
    for (Label i = 0; i < fst_array_.size(); ++i) {
      if (!fst_array_[i])
        continue;
      if (fst_array_[i]->Properties(props, true) != props || !depaccess_[i])
        return false;
    }
    return true;
  }

  // Removes useless Fsts, states and transitions.
  void Connect();

  // Replaces Fsts specified by labels.
  // Does nothing if there are cyclic dependencies.
  void ReplaceLabels(const vector<Label> &labels);

  // Replaces Fsts that have at most 'nstates' states, 'narcs' arcs and
  // 'nnonterm' non-terminals (updating in reverse dependency order).
  // Does nothing if there are cyclic dependencies.
  void ReplaceBySize(size_t nstates, size_t narcs, size_t nnonterms);

  // Replaces singleton Fsts.
  // Does nothing if there are cyclic dependencies.
  void ReplaceTrivial() { ReplaceBySize(2, 1, 1); }

  // Replaces non-terminals that have at most 'ninstances' instances
  // (updating in dependency order).
  // Does nothing if there are cyclic dependencies.
  void ReplaceByInstances(size_t ninstances);

  // Replaces non-terminals that have only one instance.
  // Does nothing if there are cyclic dependencies.
  void ReplaceUnique() { ReplaceByInstances(1); }

  // Returns Label, Fst pairs; Fst ownership retained by ReplaceUtil.
  void GetFstPairs(vector<FstPair> *fst_pairs);

  // Returns Label, MutableFst pairs; Fst ownership given to caller.
  void GetMutableFstPairs(vector<MutableFstPair> *mutable_fst_pairs);

 private:
  // Per Fst statistics
  struct ReplaceStats {
    StateId nstates;    // # of states
    StateId nfinal;     // # of final states
    size_t narcs;       // # of arcs
    Label nnonterms;    // # of non-terminals in Fst
    size_t nref;        // # of non-terminal instances referring to this Fst

    // # of times that ith Fst references this Fst
    map<Label, size_t> inref;
    // # of times that this Fst references the ith Fst
    map<Label, size_t> outref;

    ReplaceStats()
        : nstates(0),
          nfinal(0),
          narcs(0),
          nnonterms(0),
          nref(0) {}
  };

  // Check Mutable Fsts exist o.w. create them.
  void CheckMutableFsts();

  // Computes the dependency graph of the replace Fsts.
  // If 'stats' is true, dependency statistics computed as well.
  void GetDependencies(bool stats) const;

  void ClearDependencies() const {
    depfst_.DeleteStates();
    stats_.clear();
    depprops_ = 0;
    have_stats_ = false;
  }

  // Get topological order of dependencies. Returns false with cyclic input.
  bool GetTopOrder(const Fst<Arc> &fst, vector<Label> *toporder) const;

  // Update statistics assuming that jth Fst will be replaced.
  void UpdateStats(Label j);

  Label root_label_;                              // root non-terminal
  Label root_fst_;                                // root Fst ID
  ReplaceLabelType call_label_type_;              // see Replace()
  ReplaceLabelType return_label_type_;            // see Replace()
  int64 return_label_;                            // see Replace()
  vector<const Fst<Arc> *> fst_array_;            // Fst per ID
  vector<MutableFst<Arc> *> mutable_fst_array_;   // MutableFst per ID
  vector<Label> nonterminal_array_;               // Fst ID to non-terminal
  NonTerminalHash nonterminal_hash_;              // non-terminal to Fst ID
  mutable VectorFst<Arc> depfst_;                 // Fst ID dependencies
  mutable vector<bool> depaccess_;                // Fst ID accessibility
  mutable uint64 depprops_;                       // dependency Fst props
  mutable bool have_stats_;                       // have dependency statistics
  mutable vector<ReplaceStats> stats_;            // Per Fst statistics
  DISALLOW_COPY_AND_ASSIGN(ReplaceUtil);
};

template <class Arc>
ReplaceUtil<Arc>::ReplaceUtil(
    const vector<MutableFstPair> &fst_pairs,
    const ReplaceUtilOptions &opts)
    : root_label_(opts.root),
      call_label_type_(opts.call_label_type),
      return_label_type_(opts.return_label_type),
      return_label_(opts.return_label),
      depprops_(0),
      have_stats_(false) {
  fst_array_.push_back(0);
  mutable_fst_array_.push_back(0);
  nonterminal_array_.push_back(kNoLabel);
  for (Label i = 0; i < fst_pairs.size(); ++i) {
    Label label = fst_pairs[i].first;
    MutableFst<Arc> *fst = fst_pairs[i].second;
    nonterminal_hash_[label] = fst_array_.size();
    nonterminal_array_.push_back(label);
    fst_array_.push_back(fst);
    mutable_fst_array_.push_back(fst);
  }
  root_fst_ = nonterminal_hash_[root_label_];
  if (!root_fst_)
    FSTERROR() << "ReplaceUtil: no root FST for label: " << root_label_;
}

template <class Arc>
ReplaceUtil<Arc>::ReplaceUtil(
    const vector<FstPair> &fst_pairs,
    const ReplaceUtilOptions &opts)
    : root_label_(opts.root),
      call_label_type_(opts.call_label_type),
      return_label_type_(opts.return_label_type),
      return_label_(opts.return_label),
      depprops_(0),
      have_stats_(false) {
  fst_array_.push_back(0);
  nonterminal_array_.push_back(kNoLabel);
  for (Label i = 0; i < fst_pairs.size(); ++i) {
    Label label = fst_pairs[i].first;
    const Fst<Arc> *fst = fst_pairs[i].second;
    nonterminal_hash_[label] = fst_array_.size();
    nonterminal_array_.push_back(label);
    fst_array_.push_back(fst->Copy());
  }
  root_fst_ = nonterminal_hash_[root_label_];
  if (!root_fst_)
    FSTERROR() << "ReplaceUtil: no root FST for label: " << root_label_;
}

template <class Arc>
ReplaceUtil<Arc>::ReplaceUtil(
    const vector<const Fst<Arc> *> &fst_array,
    const NonTerminalHash &nonterminal_hash,
    const ReplaceUtilOptions &opts)
    : root_fst_(opts.root),
      call_label_type_(opts.call_label_type),
      return_label_type_(opts.return_label_type),
      return_label_(opts.return_label),
      nonterminal_array_(fst_array.size()),
      nonterminal_hash_(nonterminal_hash),
      depprops_(0),
      have_stats_(false) {
  fst_array_.push_back(0);
  for (Label i = 1; i < fst_array.size(); ++i)
    fst_array_.push_back(fst_array[i]->Copy());
  for (typename NonTerminalHash::const_iterator it =
           nonterminal_hash.begin(); it != nonterminal_hash.end(); ++it)
    nonterminal_array_[it->second] = it->first;
  root_label_ = nonterminal_array_[root_fst_];
}

template <class Arc>
void ReplaceUtil<Arc>::GetDependencies(bool stats) const {
  if (depfst_.NumStates() > 0) {
    if (stats && !have_stats_)
      ClearDependencies();
    else
      return;
  }

  have_stats_ = stats;
  if (have_stats_)
    stats_.reserve(fst_array_.size());

  for (Label i = 0; i < fst_array_.size(); ++i) {
    depfst_.AddState();
    depfst_.SetFinal(i, Weight::One());
    if (have_stats_)
      stats_.push_back(ReplaceStats());
  }
  depfst_.SetStart(root_fst_);

  // An arc from each state (representing the fst) to the
  // state representing the fst being replaced
  for (Label i = 0; i < fst_array_.size(); ++i) {
    const Fst<Arc> *ifst = fst_array_[i];
    if (!ifst)
      continue;
    for (StateIterator<Fst<Arc> > siter(*ifst); !siter.Done(); siter.Next()) {
      StateId s = siter.Value();
      if (have_stats_) {
        ++stats_[i].nstates;
        if (ifst->Final(s) != Weight::Zero())
          ++stats_[i].nfinal;
      }
      for (ArcIterator<Fst<Arc> > aiter(*ifst, s);
           !aiter.Done(); aiter.Next()) {
        if (have_stats_)
          ++stats_[i].narcs;
        const Arc& arc = aiter.Value();

        typename NonTerminalHash::const_iterator it =
            nonterminal_hash_.find(arc.olabel);
        if (it != nonterminal_hash_.end()) {
          Label j = it->second;
          depfst_.AddArc(i, Arc(arc.olabel, arc.olabel, Weight::One(), j));
          if (have_stats_) {
            ++stats_[i].nnonterms;
            ++stats_[j].nref;
            ++stats_[j].inref[i];
            ++stats_[i].outref[j];
          }
        }
      }
    }
  }

  // Gets accessibility info
  SccVisitor<Arc> scc_visitor(0, &depaccess_, 0, &depprops_);
  DfsVisit(depfst_, &scc_visitor);
}

template <class Arc>
void ReplaceUtil<Arc>::UpdateStats(Label j) {
  if (!have_stats_) {
    FSTERROR() << "ReplaceUtil::UpdateStats: stats not available";
    return;
  }

  if (j == root_fst_)  // can't replace root
    return;

  typedef typename map<Label, size_t>::iterator Iter;
  for (Iter in = stats_[j].inref.begin();
       in != stats_[j].inref.end();
       ++in) {
    Label i = in->first;
    size_t ni = in->second;
    stats_[i].nstates += stats_[j].nstates * ni;
    stats_[i].narcs += (stats_[j].narcs + 1) * ni;  // narcs - 1 + 2 (eps)
    stats_[i].nnonterms += (stats_[j].nnonterms - 1) * ni;
    stats_[i].outref.erase(stats_[i].outref.find(j));
    for (Iter out = stats_[j].outref.begin();
         out != stats_[j].outref.end();
         ++out) {
      Label k = out->first;
      size_t nk = out->second;
      stats_[i].outref[k] += ni * nk;
    }
  }

  for (Iter out = stats_[j].outref.begin();
       out != stats_[j].outref.end();
       ++out) {
    Label k = out->first;
    size_t nk = out->second;
    stats_[k].nref -= nk;
    stats_[k].inref.erase(stats_[k].inref.find(j));
    for (Iter in = stats_[j].inref.begin();
         in != stats_[j].inref.end();
         ++in) {
      Label i = in->first;
      size_t ni = in->second;
      stats_[k].inref[i] += ni * nk;
      stats_[k].nref += ni * nk;
    }
  }
}

template <class Arc>
void ReplaceUtil<Arc>::CheckMutableFsts() {
  if (mutable_fst_array_.size() == 0) {
    for (Label i = 0; i < fst_array_.size(); ++i) {
      if (!fst_array_[i]) {
        mutable_fst_array_.push_back(0);
      } else {
        mutable_fst_array_.push_back(new VectorFst<Arc>(*fst_array_[i]));
        delete fst_array_[i];
        fst_array_[i] = mutable_fst_array_[i];
      }
    }
  }
}

template <class Arc>
void ReplaceUtil<Arc>::Connect() {
  CheckMutableFsts();
  uint64 props = kAccessible | kCoAccessible;
  for (Label i = 0; i < mutable_fst_array_.size(); ++i) {
    if (!mutable_fst_array_[i])
      continue;
    if (mutable_fst_array_[i]->Properties(props, false) != props)
      fst::Connect(mutable_fst_array_[i]);
  }
  GetDependencies(false);
  for (Label i = 0; i < mutable_fst_array_.size(); ++i) {
    MutableFst<Arc> *fst = mutable_fst_array_[i];
    if (fst && !depaccess_[i]) {
      delete fst;
      fst_array_[i] = 0;
      mutable_fst_array_[i] = 0;
    }
  }
  ClearDependencies();
}

template <class Arc>
bool ReplaceUtil<Arc>::GetTopOrder(const Fst<Arc> &fst,
                                   vector<Label> *toporder) const {
  // Finds topological order of dependencies.
  vector<StateId> order;
  bool acyclic = false;

  TopOrderVisitor<Arc> top_order_visitor(&order, &acyclic);
  DfsVisit(fst, &top_order_visitor);
  if (!acyclic) {
    LOG(WARNING) << "ReplaceUtil::GetTopOrder: Cyclical label dependencies";
    return false;
  }

  toporder->resize(order.size());
  for (Label i = 0; i < order.size(); ++i)
    (*toporder)[order[i]] = i;

  return true;
}

template <class Arc>
void ReplaceUtil<Arc>::ReplaceLabels(const vector<Label> &labels) {
  CheckMutableFsts();
  unordered_set<Label> label_set;
  for (Label i = 0; i < labels.size(); ++i)
    if (labels[i] != root_label_)  // can't replace root
      label_set.insert(labels[i]);

  // Finds Fst dependencies restricted to the labels requested.
  GetDependencies(false);
  VectorFst<Arc> pfst(depfst_);
  for (StateId i = 0; i < pfst.NumStates(); ++i) {
    vector<Arc> arcs;
    for (ArcIterator< VectorFst<Arc> > aiter(pfst, i);
         !aiter.Done(); aiter.Next()) {
      const Arc &arc = aiter.Value();
      Label label = nonterminal_array_[arc.nextstate];
      if (label_set.count(label) > 0)
        arcs.push_back(arc);
    }
    pfst.DeleteArcs(i);
    for (size_t j = 0; j < arcs.size(); ++j)
      pfst.AddArc(i, arcs[j]);
  }

  vector<Label> toporder;
  if (!GetTopOrder(pfst, &toporder)) {
    ClearDependencies();
    return;
  }

  // Visits Fsts in reverse topological order of dependencies and
  // performs replacements.
  for (Label o = toporder.size() - 1; o >= 0;  --o) {
    vector<FstPair> fst_pairs;
    StateId s = toporder[o];
    for (ArcIterator< VectorFst<Arc> > aiter(pfst, s);
         !aiter.Done(); aiter.Next()) {
      const Arc &arc = aiter.Value();
      Label label = nonterminal_array_[arc.nextstate];
      const Fst<Arc> *fst = fst_array_[arc.nextstate];
      fst_pairs.push_back(std::make_pair(label, fst));
    }
    if (fst_pairs.empty())
        continue;
    Label label = nonterminal_array_[s];
    const Fst<Arc> *fst = fst_array_[s];
    fst_pairs.push_back(std::make_pair(label, fst));

    Replace(fst_pairs, mutable_fst_array_[s],
            ReplaceUtilOptions(label, call_label_type_,
                               return_label_type_, return_label_));
  }
  ClearDependencies();
}

template <class Arc>
void ReplaceUtil<Arc>::ReplaceBySize(size_t nstates, size_t narcs,
                                     size_t nnonterms) {
  vector<Label> labels;
  GetDependencies(true);

  vector<Label> toporder;
  if (!GetTopOrder(depfst_, &toporder)) {
    ClearDependencies();
    return;
  }

  for (Label o = toporder.size() - 1; o >= 0; --o) {
    Label j = toporder[o];
    if (stats_[j].nstates <= nstates &&
        stats_[j].narcs <= narcs &&
        stats_[j].nnonterms <= nnonterms) {
      labels.push_back(nonterminal_array_[j]);
      UpdateStats(j);
    }
  }
  ReplaceLabels(labels);
}

template <class Arc>
void ReplaceUtil<Arc>::ReplaceByInstances(size_t ninstances) {
  vector<Label> labels;
  GetDependencies(true);

  vector<Label> toporder;
  if (!GetTopOrder(depfst_, &toporder)) {
    ClearDependencies();
    return;
  }
  for (Label o = 0; o < toporder.size(); ++o) {
    Label j = toporder[o];
    if (stats_[j].nref <= ninstances) {
      labels.push_back(nonterminal_array_[j]);
      UpdateStats(j);
    }
  }
  ReplaceLabels(labels);
}

template <class Arc>
void ReplaceUtil<Arc>::GetFstPairs(vector<FstPair> *fst_pairs) {
  CheckMutableFsts();
  fst_pairs->clear();
  for (Label i = 0; i < fst_array_.size(); ++i) {
    Label label = nonterminal_array_[i];
    const Fst<Arc> *fst = fst_array_[i];
    if (!fst)
      continue;
    fst_pairs->push_back(std::make_pair(label, fst));
  }
}

template <class Arc>
void ReplaceUtil<Arc>::GetMutableFstPairs(
    vector<MutableFstPair> *mutable_fst_pairs) {
  CheckMutableFsts();
  mutable_fst_pairs->clear();
  for (Label i = 0; i < mutable_fst_array_.size(); ++i) {
    Label label = nonterminal_array_[i];
    MutableFst<Arc> *fst = mutable_fst_array_[i];
    if (!fst)
      continue;
    mutable_fst_pairs->push_back(std::make_pair(label, fst->Copy()));
  }
}

}  // namespace fst

#endif  // FST_LIB_REPLACE_UTIL_H__
