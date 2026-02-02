// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Synchronize an FST with bounded delay.

#ifndef FST_SYNCHRONIZE_H_
#define FST_SYNCHRONIZE_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <fst/cache.h>
#include <fst/test-properties.h>

#include <unordered_map>
#include <unordered_set>

namespace fst {

using SynchronizeFstOptions = CacheOptions;

namespace internal {

// Implementation class for SynchronizeFst.
// TODO(kbg,sorenj): Refactor to guarantee thread-safety.

template <class Arc>
class SynchronizeFstImpl : public CacheImpl<Arc> {
 public:
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using FstImpl<Arc>::SetType;
  using FstImpl<Arc>::SetProperties;
  using FstImpl<Arc>::SetInputSymbols;
  using FstImpl<Arc>::SetOutputSymbols;

  using CacheBaseImpl<CacheState<Arc>>::EmplaceArc;
  using CacheBaseImpl<CacheState<Arc>>::HasArcs;
  using CacheBaseImpl<CacheState<Arc>>::HasFinal;
  using CacheBaseImpl<CacheState<Arc>>::HasStart;
  using CacheBaseImpl<CacheState<Arc>>::SetArcs;
  using CacheBaseImpl<CacheState<Arc>>::SetFinal;
  using CacheBaseImpl<CacheState<Arc>>::SetStart;

  using String = std::basic_string<Label>;
  using StringView = std::basic_string_view<Label>;

  struct Element {
    Element() {}

    Element(StateId state_, StringView i, StringView o)
        : state(state_), istring(i), ostring(o) {}

    StateId state;       // Input state ID.
    StringView istring;  // Residual input labels.
    StringView ostring;  // Residual output labels.
    // Residual strings are represented by std::basic_string_view<Label> whose
    // values are owned by the hash set string_set_.
  };

  SynchronizeFstImpl(const Fst<Arc> &fst, const SynchronizeFstOptions &opts)
      : CacheImpl<Arc>(opts), fst_(fst.Copy()) {
    SetType("synchronize");
    const auto props = fst.Properties(kFstProperties, false);
    SetProperties(SynchronizeProperties(props), kCopyProperties);
    SetInputSymbols(fst.InputSymbols());
    SetOutputSymbols(fst.OutputSymbols());
  }

  SynchronizeFstImpl(const SynchronizeFstImpl &impl)
      : CacheImpl<Arc>(impl), fst_(impl.fst_->Copy(true)) {
    SetType("synchronize");
    SetProperties(impl.Properties(), kCopyProperties);
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  StateId Start() {
    if (!HasStart()) {
      auto start = fst_->Start();
      if (start == kNoStateId) return kNoStateId;
      const StringView empty = FindString(String());
      start = FindState(Element(fst_->Start(), empty, empty));
      SetStart(start);
    }
    return CacheImpl<Arc>::Start();
  }

  Weight Final(StateId s) {
    if (!HasFinal(s)) {
      const auto &element = elements_[s];
      const auto weight = element.state == kNoStateId
                              ? Weight::One()
                              : fst_->Final(element.state);
      if ((weight != Weight::Zero()) && element.istring.empty() &&
          element.ostring.empty()) {
        SetFinal(s, weight);
      } else {
        SetFinal(s, Weight::Zero());
      }
    }
    return CacheImpl<Arc>::Final(s);
  }

  size_t NumArcs(StateId s) {
    if (!HasArcs(s)) Expand(s);
    return CacheImpl<Arc>::NumArcs(s);
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s)) Expand(s);
    return CacheImpl<Arc>::NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s)) Expand(s);
    return CacheImpl<Arc>::NumOutputEpsilons(s);
  }

  uint64_t Properties() const override { return Properties(kFstProperties); }

  // Sets error if found, returning other FST impl properties.
  uint64_t Properties(uint64_t mask) const override {
    if ((mask & kError) && fst_->Properties(kError, false)) {
      SetProperties(kError, kError);
    }
    return FstImpl<Arc>::Properties(mask);
  }

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) {
    if (!HasArcs(s)) Expand(s);
    CacheImpl<Arc>::InitArcIterator(s, data);
  }

  // Returns the first character of the string obtained by concatenating the
  // string and the label.
  Label Car(StringView str, Label label = 0) const {
    if (!str.empty()) {
      return str[0];
    } else {
      return label;
    }
  }

  // Computes the residual string obtained by removing the first
  // character in the concatenation of the string and the label.
  StringView Cdr(StringView str, Label label = 0) {
    if (str.empty()) return FindString(String());
    return Concat(str.substr(1), label);
  }

  // Computes the concatenation of the string and the label.
  StringView Concat(StringView str, Label label = 0) {
    String r(str);
    if (label) r.push_back(label);
    return FindString(std::move(r));
  }

  // Tests if the concatenation of the string and label is empty.
  bool Empty(StringView str, Label label = 0) const {
    if (str.empty()) {
      return label == 0;
    } else {
      return false;
    }
  }

  StringView FindString(String &&str) {
    const auto [str_it, unused] = string_set_.insert(std::forward<String>(str));
    return *str_it;
  }

  // Finds state corresponding to an element. Creates new state if element
  // is not found.
  StateId FindState(const Element &element) {
    const auto &[iter, inserted] =
        element_map_.emplace(element, elements_.size());
    if (inserted) {
      elements_.push_back(element);
    }
    return iter->second;
  }

  // Computes the outgoing transitions from a state, creating new destination
  // states as needed.
  void Expand(StateId s) {
    const auto element = elements_[s];
    if (element.state != kNoStateId) {
      for (ArcIterator<Fst<Arc>> aiter(*fst_, element.state); !aiter.Done();
           aiter.Next()) {
        const auto &arc = aiter.Value();
        if (!Empty(element.istring, arc.ilabel) &&
            !Empty(element.ostring, arc.olabel)) {
          StringView istring = Cdr(element.istring, arc.ilabel);
          StringView ostring = Cdr(element.ostring, arc.olabel);
          EmplaceArc(s, Car(element.istring, arc.ilabel),
                     Car(element.ostring, arc.olabel), arc.weight,
                     FindState(Element(arc.nextstate, istring, ostring)));
        } else {
          StringView istring = Concat(element.istring, arc.ilabel);
          StringView ostring = Concat(element.ostring, arc.olabel);
          EmplaceArc(s, 0, 0, arc.weight,
                     FindState(Element(arc.nextstate, istring, ostring)));
        }
      }
    }
    const auto weight = element.state == kNoStateId
                            ? Weight::One()
                            : fst_->Final(element.state);
    if ((weight != Weight::Zero()) &&
        (element.istring.size() + element.ostring.size() > 0)) {
      StringView istring = Cdr(element.istring);
      StringView ostring = Cdr(element.ostring);
      EmplaceArc(s, Car(element.istring), Car(element.ostring), weight,
                 FindState(Element(kNoStateId, istring, ostring)));
    }
    SetArcs(s);
  }

 private:
  // Equality function for Elements; assumes strings have been hashed.
  class ElementEqual {
   public:
    bool operator()(const Element &x, const Element &y) const {
      return x.state == y.state && x.istring.data() == y.istring.data() &&
             x.ostring.data() == y.ostring.data();
    }
  };

  // Hash function for Elements to FST states.
  class ElementKey {
   public:
    size_t operator()(const Element &x) const {
      size_t key = x.state;
      key = (key << 1) ^ x.istring.size();
      for (Label label : x.istring) {
        key = (key << 1) ^ label;
      }
      key = (key << 1) ^ x.ostring.size();
      for (Label label : x.ostring) {
        key = (key << 1) ^ label;
      }
      return key;
    }
  };

  // Hash function for set of strings. This only has to be specified since
  // `std::hash<std::basic_string<T>>` is only guaranteed to be defined for
  // certain values of `T`. Not defining this works fine on clang, but fails
  // under GCC.
  class StringKey {
   public:
    size_t operator()(StringView x) const {
      size_t key = x.size();
      for (Label label : x) key = (key << 1) ^ label;
      return key;
    }
  };

  using ElementMap =
      std::unordered_map<Element, StateId, ElementKey, ElementEqual>;
  using StringSet = std::unordered_set<String, StringKey>;

  std::unique_ptr<const Fst<Arc>> fst_;
  std::vector<Element> elements_;  // Maps FST state to Elements.
  ElementMap element_map_;         // Maps Elements to FST state.
  StringSet string_set_;
};

}  // namespace internal

// Synchronizes a transducer. This version is a delayed FST. The result is an
// equivalent FST that has the property that during the traversal of a path,
// the delay is either zero or strictly increasing, where the delay is the
// difference between the number of non-epsilon output labels and input labels
// along the path.
//
// For the algorithm to terminate, the input transducer must have bounded
// delay, i.e., the delay of every cycle must be zero.
//
// Complexity:
//
// - A has bounded delay: exponential.
// - A does not have bounded delay: does not terminate.
//
// For more information, see:
//
// Mohri, M. 2003. Edit-distance of weighted automata: General definitions and
// algorithms. International Journal of Computer Science 14(6): 957-982.
//
// This class attaches interface to implementation and handles reference
// counting, delegating most methods to ImplToFst.
template <class A>
class SynchronizeFst : public ImplToFst<internal::SynchronizeFstImpl<A>> {
 public:
  using Arc = A;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Store = DefaultCacheStore<Arc>;
  using State = typename Store::State;
  using Impl = internal::SynchronizeFstImpl<A>;

  friend class ArcIterator<SynchronizeFst<A>>;
  friend class StateIterator<SynchronizeFst<A>>;

  explicit SynchronizeFst(const Fst<A> &fst, const SynchronizeFstOptions &opts =
                                                 SynchronizeFstOptions())
      : ImplToFst<Impl>(std::make_shared<Impl>(fst, opts)) {}

  // See Fst<>::Copy() for doc.
  SynchronizeFst(const SynchronizeFst &fst, bool safe = false)
      : ImplToFst<Impl>(fst, safe) {}

  // Gets a copy of this SynchronizeFst. See Fst<>::Copy() for further doc.
  SynchronizeFst *Copy(bool safe = false) const override {
    return new SynchronizeFst(*this, safe);
  }

  inline void InitStateIterator(StateIteratorData<Arc> *data) const override;

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const override {
    GetMutableImpl()->InitArcIterator(s, data);
  }

 private:
  using ImplToFst<Impl>::GetImpl;
  using ImplToFst<Impl>::GetMutableImpl;

  SynchronizeFst &operator=(const SynchronizeFst &) = delete;
};

// Specialization for SynchronizeFst.
template <class Arc>
class StateIterator<SynchronizeFst<Arc>>
    : public CacheStateIterator<SynchronizeFst<Arc>> {
 public:
  explicit StateIterator(const SynchronizeFst<Arc> &fst)
      : CacheStateIterator<SynchronizeFst<Arc>>(fst, fst.GetMutableImpl()) {}
};

// Specialization for SynchronizeFst.
template <class Arc>
class ArcIterator<SynchronizeFst<Arc>>
    : public CacheArcIterator<SynchronizeFst<Arc>> {
 public:
  using StateId = typename Arc::StateId;

  ArcIterator(const SynchronizeFst<Arc> &fst, StateId s)
      : CacheArcIterator<SynchronizeFst<Arc>>(fst.GetMutableImpl(), s) {
    if (!fst.GetImpl()->HasArcs(s)) fst.GetMutableImpl()->Expand(s);
  }
};

template <class Arc>
inline void SynchronizeFst<Arc>::InitStateIterator(
    StateIteratorData<Arc> *data) const {
  data->base = std::make_unique<StateIterator<SynchronizeFst<Arc>>>(*this);
}

// Synchronizes a transducer. This version writes the synchronized result to a
// MutableFst. The result will be an equivalent FST that has the property that
// during the traversal of a path, the delay is either zero or strictly
// increasing, where the delay is the difference between the number of
// non-epsilon output labels and input labels along the path.
//
// For the algorithm to terminate, the input transducer must have bounded
// delay, i.e., the delay of every cycle must be zero.
//
// Complexity:
//
// - A has bounded delay: exponential.
// - A does not have bounded delay: does not terminate.
//
// For more information, see:
//
// Mohri, M. 2003. Edit-distance of weighted automata: General definitions and
// algorithms. International Journal of Computer Science 14(6): 957-982.
template <class Arc>
void Synchronize(const Fst<Arc> &ifst, MutableFst<Arc> *ofst) {
  // Caches only the last state for fastest copy.
  const SynchronizeFstOptions opts(FST_FLAGS_fst_default_cache_gc,
                                   0);
  *ofst = SynchronizeFst<Arc>(ifst, opts);
}

}  // namespace fst

#endif  // FST_SYNCHRONIZE_H_
