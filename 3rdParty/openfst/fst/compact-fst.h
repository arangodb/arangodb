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
// FST Class for memory-efficient representation of common types of
// FSTs: linear automata, acceptors, unweighted FSTs, ...

#ifndef FST_COMPACT_FST_H_
#define FST_COMPACT_FST_H_

#include <climits>
#include <cstdint>
#include <iterator>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include <fst/log.h>

#include <fst/cache.h>
#include <fst/expanded-fst.h>
#include <fst/fst-decl.h>  // For optional argument declarations
#include <fst/mapped-file.h>
#include <fst/matcher.h>
#include <fst/test-properties.h>
#include <fst/util.h>

namespace fst {

struct CompactFstOptions : public CacheOptions {
  // The default caching behaviour is to do no caching. Most compactors are
  // cheap and therefore we save memory by not doing caching.
  CompactFstOptions() : CacheOptions(true, 0) {}

  explicit CompactFstOptions(const CacheOptions &opts) : CacheOptions(opts) {}
};

// New (Fst) Compactor interface - used by CompactFst. This interface
// allows complete flexibility in how the compaction is accomplished.
//
// class Compactor {
//  public:
//   // Constructor from the Fst to be compacted. If compactor is present,
//   // only optional state should be copied from it. Examples of this
//   // optional state include compression level or ArcCompactors.
//   explicit Compactor(const Fst<Arc> &fst,
//                      shared_ptr<Compactor> compactor = nullptr);
//   // Copy constructor. Must make a thread-safe copy suitable for use by
//   // by Fst::Copy(/*safe=*/true). Only thread-unsafe data structures
//   // need to be deeply copied. Ideally, this constructor is O(1) and any
//   // large structures are thread-safe and shared, while small ones may
//   // need to be copied.
//   Compactor(const Compactor &compactor);
//   // Default constructor (optional, see comment below).
//   Compactor();
//
//   // Returns the start state, number of states, and total number of arcs
//   // of the compacted Fst
//   StateId Start() const;
//   StateId NumStates() const;
//   size_t NumArcs() const;
//
//   // Accessor class for state attributes.
//   class State {
//    public:
//     State();  // Required, corresponds to kNoStateId.
//     // This constructor may, of course, also take a const Compactor *
//     // for the first argument. It is recommended to use const Compactor *
//     // if possible, but this can be Compactor * if necessary.
//     State(Compactor *c, StateId s);  // Accessor for StateId 's'.
//     StateId GetStateId() const;
//     Weight Final() const;
//     size_t NumArcs() const;
//     // Gets the 'i'th arc for the state. Requires i < NumArcs().
//     // Flags are a bitmask of the kArc*Value flags that ArcIterator uses.
//     Arc GetArc(size_t i, uint8_t flags) const;
//   };
//
//   // Modifies 'state' accessor to provide access to state id 's'.
//   void SetState(StateId s, State *state);
//
//   // Tests whether 'fst' can be compacted by this compactor.
//   template <typename A>
//   bool IsCompatible(const Fst<A> &fst) const;
//
//   // Returns the properties that are always when an FST with the
//   // specified properties is compacted using this compactor.
//   // This function should clear bits for properties that no longer
//   // hold and set those for properties that are known to hold.
//   uint64_t Properties(uint64_t props) const;
//
//   // Returns a string identifying the type of compactor.
//   static const std::string &Type();
//
//   // Returns true if an error has occurred.
//   bool Error() const;
//
//   // Writes a compactor to a file.
//   bool Write(std::ostream &strm, const FstWriteOptions &opts) const;
//
//   // Reads a compactor from a file.
//   static Compactor *Read(std::istream &strm, const FstReadOptions &opts,
//                          const FstHeader &hdr);
// };
//

// Old ArcCompactor Interface:
//
// This interface is not deprecated; it, along with CompactArcStore and
// other Stores that implement its interface, is simply more constrained
// by essentially forcing the implementation to use an index array
// and an arc array, but giving flexibility in how those are implemented.
// This interface may still be useful and more convenient if that is the
// desired representation.
//
// The ArcCompactor class determines how arcs and final weights are compacted
// and expanded.
//
// Final weights are treated as transitions to the superfinal state, i.e.,
// ilabel = olabel = kNoLabel and nextstate = kNoStateId.
//
// There are two types of compactors:
//
// * Fixed out-degree compactors: 'compactor.Size()' returns a positive integer
//   's'. An FST can be compacted by this compactor only if each state has
//   exactly 's' outgoing transitions (counting a non-Zero() final weight as a
//   transition). A typical example is a compactor for string FSTs, i.e.,
//   's == 1'.
//
// * Variable out-degree compactors: 'compactor.Size() == -1'. There are no
//   out-degree restrictions for these compactors.
//
// Interface:
//
// class ArcCompactor {
//  public:
//   // Default constructor (optional, see comment below).
//   ArcCompactor();
//
//   // Copy constructor. Must make a thread-safe copy suitable for use by
//   // by Fst::Copy(/*safe=*/true). Only thread-unsafe data structures
//   // need to be deeply copied.
//   ArcCompactor(const ArcCompactor &);
//
//   // Element is the type of the compacted transitions.
//   using Element = ...
//
//   // Returns the compacted representation of a transition 'arc'
//   // at a state 's'.
//   Element Compact(StateId s, const Arc &arc);
//
//   // Returns the transition at state 's' represented by the compacted
//   // transition 'e'.
//   Arc Expand(StateId s, const Element &e) const;
//
//   // Returns -1 for variable out-degree compactors, and the mandatory
//   // out-degree otherwise.
//   ssize_t Size() const;
//
//   // Tests whether an FST can be compacted by this compactor.
//   bool Compatible(const Fst<A> &fst) const;
//
//   // Returns the properties that are always true for an FST compacted using
//   // this compactor. Any Fst with the inverse of these properties should
//   // be incompatible.
//   uint64_t Properties() const;
//
//   // Returns a string identifying the type of compactor.
//   static const std::string &Type();
//
//   // Writes a compactor to a file.
//   bool Write(std::ostream &strm) const;
//
//   // Reads a compactor from a file.
//   static ArcCompactor *Read(std::istream &strm);
// };
//
// The default constructor is only required for FST_REGISTER to work (i.e.,
// enabling Convert() and the command-line utilities to work with this new
// compactor). However, a default constructor always needs to be specified for
// this code to compile, but one can have it simply raise an error when called,
// like so:
//
// Compactor::Compactor() {
//   FSTERROR() << "Compactor: No default constructor";
// }

// Default implementation data for CompactArcCompactor. Only old-style
// ArcCompactors are supported because the CompactArcStore constructors
// use the old API.
//
// DefaultCompact store is thread-compatible, but not thread-safe.
// The copy constructor makes a thread-safe copy.
//
// The implementation contains two arrays: 'states_' and 'compacts_'.
//
// For fixed out-degree compactors, the 'states_' array is unallocated. The
// 'compacts_' array contains the compacted transitions. Its size is
// 'ncompacts_'. The outgoing transitions at a given state are stored
// consecutively. For a given state 's', its 'compactor.Size()' outgoing
// transitions (including a superfinal transition when 's' is final), are stored
// in positions ['s*compactor.Size()', '(s+1)*compactor.Size()').
//
// For variable out-degree compactors, the states_ array has size
// 'nstates_ + 1' and contains positions in the 'compacts_' array. For a
// given state 's', the compacted transitions of 's' are stored in positions
// ['states_[s]', 'states_[s + 1]') in 'compacts_'. By convention,
// 'states_[nstates_] == ncompacts_'.
//
// In both cases, the superfinal transitions (when 's' is final, i.e.,
// 'Final(s) != Weight::Zero()') are stored first.
//
// The unsigned type U is used to represent indices into the compacts_ array.
template <class Element, class Unsigned>
class CompactArcStore {
 public:
  CompactArcStore() = default;

  // Makes a thread-safe copy. O(1).
  CompactArcStore(const CompactArcStore &) = default;

  template <class Arc, class ArcCompactor>
  CompactArcStore(const Fst<Arc> &fst, const ArcCompactor &arc_compactor);

  template <class Iterator, class ArcCompactor>
  CompactArcStore(const Iterator begin, const Iterator end,
                  const ArcCompactor &arc_compactor);

  ~CompactArcStore() = default;

  template <class ArcCompactor>
  static CompactArcStore *Read(std::istream &strm, const FstReadOptions &opts,
                               const FstHeader &hdr,
                               const ArcCompactor &arc_compactor);

  bool Write(std::ostream &strm, const FstWriteOptions &opts) const;

  // Returns the starting index in 'compacts_' of the transitions
  // for state 'i'. See class-level comment for further details.
  // Requires that the CompactArcStore was constructed with a
  // variable out-degree compactor. Requires 0 <= i <= NumStates().
  // By convention, States(NumStates()) == NumCompacts().
  Unsigned States(ssize_t i) const { return states_[i]; }

  // Returns the compacted Element at position i. See class-level comment
  // for further details. Requires 0 <= i < NumCompacts().
  const Element &Compacts(size_t i) const { return compacts_[i]; }

  size_t NumStates() const { return nstates_; }

  size_t NumCompacts() const { return ncompacts_; }

  size_t NumArcs() const { return narcs_; }

  ssize_t Start() const { return start_; }

  bool Error() const { return error_; }

  // Returns a string identifying the type of data storage container.
  static const std::string &Type();

 private:
  std::shared_ptr<MappedFile> states_region_;
  std::shared_ptr<MappedFile> compacts_region_;
  // Unowned pointer into states_region_.
  Unsigned *states_ = nullptr;
  // Unowned pointer into compacts_region_.
  Element *compacts_ = nullptr;
  size_t nstates_ = 0;
  size_t ncompacts_ = 0;
  size_t narcs_ = 0;
  ssize_t start_ = kNoStateId;
  bool error_ = false;
};

template <class Element, class Unsigned>
template <class Arc, class ArcCompactor>
CompactArcStore<Element, Unsigned>::CompactArcStore(
    const Fst<Arc> &fst, const ArcCompactor &arc_compactor) {
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  start_ = fst.Start();
  // Counts # of states and arcs.
  StateId nfinals = 0;
  for (StateIterator<Fst<Arc>> siter(fst); !siter.Done(); siter.Next()) {
    ++nstates_;
    const auto s = siter.Value();
    narcs_ += fst.NumArcs(s);
    if (fst.Final(s) != Weight::Zero()) ++nfinals;
  }
  if (arc_compactor.Size() == -1) {
    states_region_ = fst::WrapUnique(MappedFile::Allocate(
        sizeof(states_[0]) * (nstates_ + 1), alignof(decltype(states_[0]))));
    states_ = static_cast<Unsigned *>(states_region_->mutable_data());
    ncompacts_ = narcs_ + nfinals;
    compacts_region_ = fst::WrapUnique(MappedFile::Allocate(
        sizeof(compacts_[0]) * ncompacts_, alignof(decltype(compacts_[0]))));
    compacts_ = static_cast<Element *>(compacts_region_->mutable_data());
    states_[nstates_] = ncompacts_;
  } else {
    states_ = nullptr;
    ncompacts_ = nstates_ * arc_compactor.Size();
    if ((narcs_ + nfinals) != ncompacts_) {
      FSTERROR() << "CompactArcStore: ArcCompactor incompatible with FST";
      error_ = true;
      return;
    }
    compacts_region_ = fst::WrapUnique(MappedFile::Allocate(
        sizeof(compacts_[0]) * ncompacts_, alignof(decltype(compacts_[0]))));
    compacts_ = static_cast<Element *>(compacts_region_->mutable_data());
  }
  size_t pos = 0;
  size_t fpos = 0;
  for (size_t s = 0; s < nstates_; ++s) {
    fpos = pos;
    if (arc_compactor.Size() == -1) states_[s] = pos;
    if (fst.Final(s) != Weight::Zero()) {
      compacts_[pos++] = arc_compactor.Compact(
          s, Arc(kNoLabel, kNoLabel, fst.Final(s), kNoStateId));
    }
    for (ArcIterator<Fst<Arc>> aiter(fst, s); !aiter.Done(); aiter.Next()) {
      compacts_[pos++] = arc_compactor.Compact(s, aiter.Value());
    }
    if ((arc_compactor.Size() != -1) && (pos != fpos + arc_compactor.Size())) {
      FSTERROR() << "CompactArcStore: ArcCompactor incompatible with FST";
      error_ = true;
      return;
    }
  }
  if (pos != ncompacts_) {
    FSTERROR() << "CompactArcStore: ArcCompactor incompatible with FST";
    error_ = true;
    return;
  }
}

template <class Element, class Unsigned>
template <class Iterator, class ArcCompactor>
CompactArcStore<Element, Unsigned>::CompactArcStore(
    const Iterator begin, const Iterator end,
    const ArcCompactor &arc_compactor) {
  using Arc = typename ArcCompactor::Arc;
  using Weight = typename Arc::Weight;
  if (arc_compactor.Size() != -1) {
    ncompacts_ = std::distance(begin, end);
    if (arc_compactor.Size() == 1) {
      // For strings, allows implicit final weight. Empty input is the empty
      // string.
      if (ncompacts_ == 0) {
        ++ncompacts_;
      } else {
        const auto arc =
            arc_compactor.Expand(ncompacts_ - 1, *(begin + (ncompacts_ - 1)));
        if (arc.ilabel != kNoLabel) ++ncompacts_;
      }
    }
    if (ncompacts_ % arc_compactor.Size()) {
      FSTERROR() << "CompactArcStore: Size of input container incompatible"
                 << " with arc compactor";
      error_ = true;
      return;
    }
    if (ncompacts_ == 0) return;
    start_ = 0;
    nstates_ = ncompacts_ / arc_compactor.Size();
    compacts_region_ = fst::WrapUnique(MappedFile::Allocate(
        sizeof(compacts_[0]) * ncompacts_, alignof(decltype(compacts_[0]))));
    compacts_ = static_cast<Element *>(compacts_region_->mutable_data());
    size_t i = 0;
    Iterator it = begin;
    for (; it != end; ++it, ++i) {
      compacts_[i] = *it;
      if (arc_compactor.Expand(i, *it).ilabel != kNoLabel) ++narcs_;
    }
    if (i < ncompacts_) {
      compacts_[i] = arc_compactor.Compact(
          i, Arc(kNoLabel, kNoLabel, Weight::One(), kNoStateId));
    }
  } else {
    if (std::distance(begin, end) == 0) return;
    // Count # of states, arcs and compacts.
    auto it = begin;
    for (size_t i = 0; it != end; ++it, ++i) {
      const auto arc = arc_compactor.Expand(i, *it);
      if (arc.ilabel != kNoLabel) {
        ++narcs_;
        ++ncompacts_;
      } else {
        ++nstates_;
        if (arc.weight != Weight::Zero()) ++ncompacts_;
      }
    }
    start_ = 0;
    compacts_region_ = fst::WrapUnique(MappedFile::Allocate(
        sizeof(compacts_[0]) * ncompacts_, alignof(decltype(compacts_[0]))));
    compacts_ = static_cast<Element *>(compacts_region_->mutable_data());
    states_region_ = fst::WrapUnique(MappedFile::Allocate(
        sizeof(states_[0]) * (nstates_ + 1), alignof(decltype(states_[0]))));
    states_ = static_cast<Unsigned *>(states_region_->mutable_data());
    states_[nstates_] = ncompacts_;
    size_t i = 0;
    size_t s = 0;
    for (it = begin; it != end; ++it) {
      const auto arc = arc_compactor.Expand(i, *it);
      if (arc.ilabel != kNoLabel) {
        compacts_[i++] = *it;
      } else {
        states_[s++] = i;
        if (arc.weight != Weight::Zero()) compacts_[i++] = *it;
      }
    }
    if ((s != nstates_) || (i != ncompacts_)) {
      FSTERROR() << "CompactArcStore: Ill-formed input container";
      error_ = true;
      return;
    }
  }
}

template <class Element, class Unsigned>
template <class ArcCompactor>
CompactArcStore<Element, Unsigned> *CompactArcStore<Element, Unsigned>::Read(
    std::istream &strm, const FstReadOptions &opts, const FstHeader &hdr,
    const ArcCompactor &arc_compactor) {
  auto data = std::make_unique<CompactArcStore>();
  data->start_ = hdr.Start();
  data->nstates_ = hdr.NumStates();
  data->narcs_ = hdr.NumArcs();
  if (arc_compactor.Size() == -1) {
    if ((hdr.GetFlags() & FstHeader::IS_ALIGNED) && !AlignInput(strm)) {
      LOG(ERROR) << "CompactArcStore::Read: Alignment failed: " << opts.source;
      return nullptr;
    }
    auto b = (data->nstates_ + 1) * sizeof(Unsigned);
    data->states_region_.reset(MappedFile::Map(
        strm, opts.mode == FstReadOptions::MAP, opts.source, b));
    if (!strm || !data->states_region_) {
      LOG(ERROR) << "CompactArcStore::Read: Read failed: " << opts.source;
      return nullptr;
    }
    data->states_ =
        static_cast<Unsigned *>(data->states_region_->mutable_data());
  } else {
    data->states_ = nullptr;
  }
  data->ncompacts_ = arc_compactor.Size() == -1
                         ? data->states_[data->nstates_]
                         : data->nstates_ * arc_compactor.Size();
  if ((hdr.GetFlags() & FstHeader::IS_ALIGNED) && !AlignInput(strm)) {
    LOG(ERROR) << "CompactArcStore::Read: Alignment failed: " << opts.source;
    return nullptr;
  }
  size_t b = data->ncompacts_ * sizeof(Element);
  data->compacts_region_.reset(
      MappedFile::Map(strm, opts.mode == FstReadOptions::MAP, opts.source, b));
  if (!strm || !data->compacts_region_) {
    LOG(ERROR) << "CompactArcStore::Read: Read failed: " << opts.source;
    return nullptr;
  }
  data->compacts_ =
      static_cast<Element *>(data->compacts_region_->mutable_data());
  return data.release();
}

template <class Element, class Unsigned>
bool CompactArcStore<Element, Unsigned>::Write(
    std::ostream &strm, const FstWriteOptions &opts) const {
  if (states_) {
    if (opts.align && !AlignOutput(strm)) {
      LOG(ERROR) << "CompactArcStore::Write: Alignment failed: " << opts.source;
      return false;
    }
    strm.write(reinterpret_cast<const char *>(states_),
               (nstates_ + 1) * sizeof(Unsigned));
  }
  if (opts.align && !AlignOutput(strm)) {
    LOG(ERROR) << "CompactArcStore::Write: Alignment failed: " << opts.source;
    return false;
  }
  strm.write(reinterpret_cast<const char *>(compacts_),
             ncompacts_ * sizeof(Element));
  strm.flush();
  if (!strm) {
    LOG(ERROR) << "CompactArcStore::Write: Write failed: " << opts.source;
    return false;
  }
  return true;
}

template <class Element, class Unsigned>
const std::string &CompactArcStore<Element, Unsigned>::Type() {
  static const std::string *const type = new std::string("compact");
  return *type;
}

template <class C, class U, class S>
class CompactArcState;

// Wraps an old-style arc compactor and a compact store as a new Fst compactor.
// The copy constructors of AC and S must make thread-safe copies and should
// be O(1).
template <class AC, class U,
          class S /*= CompactArcStore<typename AC::Element, U>*/>
class CompactArcCompactor {
 public:
  using ArcCompactor = AC;
  using Unsigned = U;
  using CompactStore = S;
  using Element = typename AC::Element;
  using Arc = typename AC::Arc;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  using State = CompactArcState<AC, U, S>;
  friend State;

  CompactArcCompactor() : arc_compactor_(nullptr), compact_store_(nullptr) {}

  // Constructs from Fst.
  explicit CompactArcCompactor(const Fst<Arc> &fst,
                               ArcCompactor &&arc_compactor = ArcCompactor())
      : CompactArcCompactor(
            fst, std::make_shared<ArcCompactor>(std::move(arc_compactor))) {}

  CompactArcCompactor(const Fst<Arc> &fst,
                      std::shared_ptr<ArcCompactor> arc_compactor)
      : arc_compactor_(std::move(arc_compactor)),
        compact_store_(std::make_shared<S>(fst, *arc_compactor_)) {}

  CompactArcCompactor(const Fst<Arc> &fst,
                      std::shared_ptr<CompactArcCompactor> compactor)
      : arc_compactor_(compactor->arc_compactor_),
        compact_store_(compactor->compact_store_ == nullptr
                           ? std::make_shared<S>(fst, *arc_compactor_)
                           : compactor->compact_store_) {}

  // Constructs from CompactStore.
  CompactArcCompactor(std::shared_ptr<ArcCompactor> arc_compactor,
                      std::shared_ptr<CompactStore> compact_store)
      : arc_compactor_(std::move(arc_compactor)),
        compact_store_(std::move(compact_store)) {}

  // The following 2 constructors take as input two iterators delimiting a set
  // of (already) compacted transitions, starting with the transitions out of
  // the initial state. The format of the input differs for fixed out-degree
  // and variable out-degree arc compactors.
  //
  // - For fixed out-degree arc compactors, the final weight (encoded as a
  // compacted transition) needs to be given only for final states. All strings
  // (arc compactor of size 1) will be assume to be terminated by a final state
  // even when the final state is not implicitely given.
  //
  // - For variable out-degree arc compactors, the final weight (encoded as a
  // compacted transition) needs to be given for all states and must appeared
  // first in the list (for state s, final weight of s, followed by outgoing
  // transitons in s).
  //
  // These 2 constructors allows the direct construction of a CompactArcFst
  // without first creating a more memory-hungry regular FST. This is useful
  // when memory usage is severely constrained.
  //
  // Usage:
  // CompactArcFst<...> fst(
  //     std::make_shared<CompactArcFst<...>::Compactor>(b, e));
  template <class Iterator>
  CompactArcCompactor(const Iterator b, const Iterator e,
                      std::shared_ptr<ArcCompactor> arc_compactor)
      : arc_compactor_(std::move(arc_compactor)),
        compact_store_(std::make_shared<S>(b, e, *arc_compactor_)) {}

  template <class Iterator>
  CompactArcCompactor(const Iterator b, const Iterator e)
      : CompactArcCompactor(b, e, std::make_shared<ArcCompactor>()) {}

  // Copy constructor. This makes a thread-safe copy, so requires that
  // The ArcCompactor and CompactStore copy constructors make thread-safe
  // copies.
  CompactArcCompactor(const CompactArcCompactor &compactor)
      : arc_compactor_(
            compactor.GetArcCompactor() == nullptr
                ? nullptr
                : std::make_shared<ArcCompactor>(*compactor.GetArcCompactor())),
        compact_store_(compactor.GetCompactStore() == nullptr
                           ? nullptr
                           : std::make_shared<CompactStore>(
                                 *compactor.GetCompactStore())) {}

  template <class OtherC>
  explicit CompactArcCompactor(
      const CompactArcCompactor<OtherC, U, S> &compactor)
      : arc_compactor_(
            compactor.GetArcCompactor() == nullptr
                ? nullptr
                : std::make_shared<ArcCompactor>(*compactor.GetArcCompactor())),
        compact_store_(compactor.GetCompactStore() == nullptr
                           ? nullptr
                           : std::make_shared<CompactStore>(
                                 *compactor.GetCompactStore())) {}

  StateId Start() const { return compact_store_->Start(); }
  StateId NumStates() const { return compact_store_->NumStates(); }
  size_t NumArcs() const { return compact_store_->NumArcs(); }

  void SetState(StateId s, State *state) const {
    if (state->GetStateId() != s) state->Set(this, s);
  }

  static CompactArcCompactor *Read(std::istream &strm,
                                   const FstReadOptions &opts,
                                   const FstHeader &hdr) {
    std::shared_ptr<ArcCompactor> arc_compactor(ArcCompactor::Read(strm));
    if (arc_compactor == nullptr) return nullptr;
    std::shared_ptr<S> compact_store(S::Read(strm, opts, hdr, *arc_compactor));
    if (compact_store == nullptr) return nullptr;
    return new CompactArcCompactor(arc_compactor, compact_store);
  }

  bool Write(std::ostream &strm, const FstWriteOptions &opts) const {
    return arc_compactor_->Write(strm) && compact_store_->Write(strm, opts);
  }

  uint64_t Properties(uint64_t props) const {
    // ArcCompactor properties can just be or-ed in since it is assumed that
    // if the ArcCompactor sets a property, any FST with the inverse
    // property is incompatible.
    return arc_compactor_->Properties() | props;
  }

  bool IsCompatible(const Fst<Arc> &fst) const {
    return arc_compactor_->Compatible(fst);
  }

  bool Error() const { return compact_store_->Error(); }

  bool HasFixedOutdegree() const { return arc_compactor_->Size() != -1; }

  static const std::string &Type() {
    static const std::string *const type = [] {
      std::string type = "compact";
      if (sizeof(U) != sizeof(uint32_t)) type += std::to_string(8 * sizeof(U));
      type += "_";
      type += ArcCompactor::Type();
      if (CompactStore::Type() != "compact") {
        type += "_";
        type += CompactStore::Type();
      }
      return new std::string(type);
    }();
    return *type;
  }

  const ArcCompactor *GetArcCompactor() const { return arc_compactor_.get(); }
  const CompactStore *GetCompactStore() const { return compact_store_.get(); }

  ArcCompactor *MutableArcCompactor() { return arc_compactor_.get(); }
  CompactStore *MutableCompactStore() { return compact_store_.get(); }

  std::shared_ptr<ArcCompactor> SharedArcCompactor() { return arc_compactor_; }
  std::shared_ptr<CompactStore> SharedCompactStore() { return compact_store_; }

  // TODO(allauzen): remove dependencies on this method and make private.
  Arc ComputeArc(StateId s, Unsigned i, uint8_t flags) const {
    return arc_compactor_->Expand(s, compact_store_->Compacts(i), flags);
  }

 private:
  std::pair<Unsigned, Unsigned> CompactsRange(StateId s) const {
    std::pair<size_t, size_t> range;
    if (HasFixedOutdegree()) {
      range.first = s * arc_compactor_->Size();
      range.second = arc_compactor_->Size();
    } else {
      range.first = compact_store_->States(s);
      range.second = compact_store_->States(s + 1) - range.first;
    }
    return range;
  }

 private:
  std::shared_ptr<ArcCompactor> arc_compactor_;
  std::shared_ptr<CompactStore> compact_store_;
};

// Default implementation of state attributes accessor class for
// CompactArcCompactor. Use of efficient specialization strongly encouraged.
template <class ArcCompactor, class U, class S>
class CompactArcState {
 public:
  using Arc = typename ArcCompactor::Arc;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  using Compactor = CompactArcCompactor<ArcCompactor, U, S>;

  CompactArcState() = default;

  CompactArcState(const Compactor *compactor, StateId s)
      : compactor_(compactor),
        s_(s),
        range_(compactor->CompactsRange(s)),
        has_final_(
            range_.second != 0 &&
            compactor->ComputeArc(s, range_.first, kArcILabelValue).ilabel ==
                kNoLabel) {
    if (has_final_) {
      ++range_.first;
      --range_.second;
    }
  }

  void Set(const Compactor *compactor, StateId s) {
    compactor_ = compactor;
    s_ = s;
    range_ = compactor->CompactsRange(s);
    if (range_.second != 0 &&
        compactor->ComputeArc(s, range_.first, kArcILabelValue).ilabel ==
            kNoLabel) {
      has_final_ = true;
      ++range_.first;
      --range_.second;
    } else {
      has_final_ = false;
    }
  }

  StateId GetStateId() const { return s_; }

  Weight Final() const {
    if (!has_final_) return Weight::Zero();
    return compactor_->ComputeArc(s_, range_.first - 1, kArcWeightValue).weight;
  }

  size_t NumArcs() const { return range_.second; }

  Arc GetArc(size_t i, uint8_t flags) const {
    return compactor_->ComputeArc(s_, range_.first + i, flags);
  }

 private:
  const Compactor *compactor_ = nullptr;  // borrowed ref.
  StateId s_ = kNoStateId;
  std::pair<U, U> range_ = {0, 0};
  bool has_final_ = false;
};

// Specialization for CompactArcStore.
template <class ArcCompactor, class U>
class CompactArcState<ArcCompactor, U,
                      CompactArcStore<typename ArcCompactor::Element, U>> {
 public:
  using Arc = typename ArcCompactor::Arc;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  using CompactStore = CompactArcStore<typename ArcCompactor::Element, U>;
  using Compactor = CompactArcCompactor<ArcCompactor, U, CompactStore>;

  CompactArcState() = default;

  CompactArcState(const Compactor *compactor, StateId s)
      : arc_compactor_(compactor->GetArcCompactor()), s_(s) {
    Init(compactor);
  }

  void Set(const Compactor *compactor, StateId s) {
    arc_compactor_ = compactor->GetArcCompactor();
    s_ = s;
    has_final_ = false;
    Init(compactor);
  }

  StateId GetStateId() const { return s_; }

  Weight Final() const {
    if (!has_final_) return Weight::Zero();
    return arc_compactor_->Expand(s_, *(compacts_ - 1), kArcWeightValue).weight;
  }

  size_t NumArcs() const { return num_arcs_; }

  Arc GetArc(size_t i, uint8_t flags) const {
    return arc_compactor_->Expand(s_, compacts_[i], flags);
  }

 private:
  void Init(const Compactor *compactor) {
    const auto *store = compactor->GetCompactStore();
    U offset;
    if (!compactor->HasFixedOutdegree()) {  // Variable out-degree compactor.
      offset = store->States(s_);
      num_arcs_ = store->States(s_ + 1) - offset;
    } else {  // Fixed out-degree compactor.
      offset = s_ * arc_compactor_->Size();
      num_arcs_ = arc_compactor_->Size();
    }
    if (num_arcs_ > 0) {
      compacts_ = &(store->Compacts(offset));
      if (arc_compactor_->Expand(s_, *compacts_, kArcILabelValue).ilabel ==
          kNoStateId) {
        ++compacts_;
        --num_arcs_;
        has_final_ = true;
      }
    }
  }

 private:
  const ArcCompactor *arc_compactor_ = nullptr;  // Borrowed reference.
  const typename ArcCompactor::Element *compacts_ =
      nullptr;  // Borrowed reference.
  StateId s_ = kNoStateId;
  U num_arcs_ = 0;
  bool has_final_ = false;
};

template <class F, class G>
void Cast(const F &, G *);

template <class CompactArcFST, class FST>
bool WriteCompactArcFst(
    const FST &fst,
    const typename CompactArcFST::Compactor::ArcCompactor &arc_compactor,
    std::ostream &strm, const FstWriteOptions &opts);

namespace internal {

// Implementation class for CompactFst, which contains parametrizeable
// Fst data storage (CompactArcStore by default) and Fst cache.
// C's copy constructor must make a thread-safe copy.
template <class Arc, class C, class CacheStore = DefaultCacheStore<Arc>>
class CompactFstImpl
    : public CacheBaseImpl<typename CacheStore::State, CacheStore> {
 public:
  using Weight = typename Arc::Weight;
  using StateId = typename Arc::StateId;
  using Compactor = C;

  using FstImpl<Arc>::SetType;
  using FstImpl<Arc>::SetProperties;
  using FstImpl<Arc>::Properties;
  using FstImpl<Arc>::SetInputSymbols;
  using FstImpl<Arc>::SetOutputSymbols;
  using FstImpl<Arc>::WriteHeader;

  using ImplBase = CacheBaseImpl<typename CacheStore::State, CacheStore>;
  using ImplBase::HasArcs;
  using ImplBase::HasFinal;
  using ImplBase::HasStart;
  using ImplBase::PushArc;
  using ImplBase::SetArcs;
  using ImplBase::SetFinal;
  using ImplBase::SetStart;

  CompactFstImpl()
      : ImplBase(CompactFstOptions()),
        compactor_(std::make_shared<Compactor>()) {
    SetType(Compactor::Type());
    SetProperties(kNullProperties | kStaticProperties);
  }

  // Constructs a CompactFstImpl, creating a new Compactor using
  // Compactor(fst, compactor); this uses the compactor arg only for optional
  // information, such as compression level. See the Compactor interface
  // description.
  CompactFstImpl(const Fst<Arc> &fst, std::shared_ptr<Compactor> compactor,
                 const CompactFstOptions &opts)
      : ImplBase(opts),
        compactor_(std::make_shared<Compactor>(fst, std::move(compactor))) {
    SetType(Compactor::Type());
    SetInputSymbols(fst.InputSymbols());
    SetOutputSymbols(fst.OutputSymbols());
    if (compactor_->Error()) SetProperties(kError, kError);
    uint64_t copy_properties =
        fst.Properties(kMutable, false)
            ? fst.Properties(kCopyProperties, true)
            : CheckProperties(
                  fst, kCopyProperties & ~kWeightedCycles & ~kUnweightedCycles,
                  kCopyProperties);
    if ((copy_properties & kError) || !compactor_->IsCompatible(fst)) {
      FSTERROR() << "CompactFstImpl: Input Fst incompatible with compactor";
      SetProperties(kError, kError);
      return;
    }
    SetProperties(compactor_->Properties(copy_properties) | kStaticProperties);
  }

  CompactFstImpl(std::shared_ptr<Compactor> compactor,
                 const CompactFstOptions &opts)
      : ImplBase(opts), compactor_(std::move(compactor)) {
    SetType(Compactor::Type());
    SetProperties(kStaticProperties | compactor_->Properties(0));
    if (compactor_->Error()) SetProperties(kError, kError);
  }

  // Makes a thread-safe copy; requires that Compactor's copy constructor
  // does so as well.
  CompactFstImpl(const CompactFstImpl &impl)
      : ImplBase(impl),
        compactor_(impl.compactor_ == nullptr
                       ? std::make_shared<Compactor>()
                       : std::make_shared<Compactor>(*impl.compactor_)) {
    SetType(impl.Type());
    SetProperties(impl.Properties());
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  // Allows to change the cache store from OtherCacheStore to CacheStore.
  template <class OtherCacheStore>
  explicit CompactFstImpl(
      const CompactFstImpl<Arc, Compactor, OtherCacheStore> &impl)
      : ImplBase(CacheOptions(impl.GetCacheGc(), impl.GetCacheLimit())),
        compactor_(impl.compactor_ == nullptr
                       ? std::make_shared<Compactor>()
                       : std::make_shared<Compactor>(*impl.compactor_)) {
    SetType(impl.Type());
    SetProperties(impl.Properties());
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  StateId Start() {
    if (!HasStart()) SetStart(compactor_->Start());
    return ImplBase::Start();
  }

  Weight Final(StateId s) {
    if (HasFinal(s)) return ImplBase::Final(s);
    compactor_->SetState(s, &state_);
    return state_.Final();
  }

  StateId NumStates() const {
    if (Properties(kError)) return 0;
    return compactor_->NumStates();
  }

  size_t NumArcs(StateId s) {
    if (HasArcs(s)) return ImplBase::NumArcs(s);
    compactor_->SetState(s, &state_);
    return state_.NumArcs();
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s) && !Properties(kILabelSorted)) Expand(s);
    if (HasArcs(s)) return ImplBase::NumInputEpsilons(s);
    return CountEpsilons(s, false);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s) && !Properties(kOLabelSorted)) Expand(s);
    if (HasArcs(s)) return ImplBase::NumOutputEpsilons(s);
    return CountEpsilons(s, true);
  }

  size_t CountEpsilons(StateId s, bool output_epsilons) {
    compactor_->SetState(s, &state_);
    const uint8_t flags = output_epsilons ? kArcOLabelValue : kArcILabelValue;
    size_t num_eps = 0;
    const size_t num_arcs = state_.NumArcs();
    for (size_t i = 0; i < num_arcs; ++i) {
      const auto &arc = state_.GetArc(i, flags);
      const auto label = output_epsilons ? arc.olabel : arc.ilabel;
      if (label == 0) {
        ++num_eps;
      } else if (label > 0) {
        break;
      }
    }
    return num_eps;
  }

  static CompactFstImpl *Read(std::istream &strm, const FstReadOptions &opts) {
    auto impl = std::make_unique<CompactFstImpl>();
    FstHeader hdr;
    if (!impl->ReadHeader(strm, opts, kMinFileVersion, &hdr)) {
      return nullptr;
    }
    // Ensures compatibility.
    if (hdr.Version() == kAlignedFileVersion) {
      hdr.SetFlags(hdr.GetFlags() | FstHeader::IS_ALIGNED);
    }
    impl->compactor_ =
        std::shared_ptr<Compactor>(Compactor::Read(strm, opts, hdr));
    if (!impl->compactor_) {
      return nullptr;
    }
    return impl.release();
  }

  bool Write(std::ostream &strm, const FstWriteOptions &opts) const {
    FstHeader hdr;
    hdr.SetStart(compactor_->Start());
    hdr.SetNumStates(compactor_->NumStates());
    hdr.SetNumArcs(compactor_->NumArcs());
    // Ensures compatibility.
    const auto file_version = opts.align ? kAlignedFileVersion : kFileVersion;
    WriteHeader(strm, opts, file_version, &hdr);
    return compactor_->Write(strm, opts);
  }

  // Provides information needed for generic state iterator.
  void InitStateIterator(StateIteratorData<Arc> *data) const {
    data->base = nullptr;
    data->nstates = compactor_->NumStates();
  }

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) {
    if (!HasArcs(s)) Expand(s);
    ImplBase::InitArcIterator(s, data);
  }

  void Expand(StateId s) {
    compactor_->SetState(s, &state_);
    const size_t num_arcs = state_.NumArcs();
    for (size_t i = 0; i < num_arcs; ++i)
      PushArc(s, state_.GetArc(i, kArcValueFlags));
    SetArcs(s);
    if (!HasFinal(s)) SetFinal(s, state_.Final());
  }

  const Compactor *GetCompactor() const { return compactor_.get(); }
  Compactor *MutableCompactor() { return compactor_.get(); }
  std::shared_ptr<Compactor> SharedCompactor() { return compactor_; }
  void SetCompactor(std::shared_ptr<Compactor> compactor) {
    // TODO(allauzen): is this correct? is this needed?
    // TODO(allauzen): consider removing and forcing this through direct calls
    // to compactor.
    compactor_ = std::move(compactor);
  }

  // Properties always true of this FST class.
  static constexpr uint64_t kStaticProperties = kExpanded;

 protected:
  template <class OtherArc, class OtherCompactor, class OtherCacheStore>
  explicit CompactFstImpl(
      const CompactFstImpl<OtherArc, OtherCompactor, OtherCacheStore> &impl)
      : compactor_(std::make_shared<Compactor>(*impl.GetCompactor())) {
    SetType(impl.Type());
    SetProperties(impl.Properties());
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

 private:
  // For k*Version constants.
  template <class CompactArcFST, class FST>
  friend bool ::fst::WriteCompactArcFst(
      const FST &fst,
      const typename CompactArcFST::Compactor::ArcCompactor &arc_compactor,
      std::ostream &strm, const FstWriteOptions &opts);

  // Current unaligned file format version.
  static constexpr int kFileVersion = 2;
  // Current aligned file format version.
  static constexpr int kAlignedFileVersion = 1;
  // Minimum file format version supported.
  static constexpr int kMinFileVersion = 1;

  std::shared_ptr<Compactor> compactor_;
  typename Compactor::State state_;
};

// Returns the compactor for the CompactFst; intended to be called as
// GetCompactor<CompactorType>(fst), which returns the compactor only if it
// is of the specified type and otherwise nullptr (via the overload below).
template <class Compactor, class Arc>
const Compactor *GetCompactor(const CompactFst<Arc, Compactor> &fst) {
  return fst.GetCompactor();
}

template <class Compactor, class Arc>
const Compactor *GetCompactor(const Fst<Arc> &fst) {
  return nullptr;
}

}  // namespace internal

// This class attaches interface to implementation and handles reference
// counting, delegating most methods to ImplToExpandedFst.
// (Template argument defaults are declared in fst-decl.h.)
template <class A, class C, class CacheStore>
class CompactFst
    : public ImplToExpandedFst<internal::CompactFstImpl<A, C, CacheStore>> {
 public:
  template <class F, class G>
  void friend Cast(const F &, G *);

  using Arc = A;
  using StateId = typename Arc::StateId;
  using Compactor = C;
  using Impl = internal::CompactFstImpl<Arc, Compactor, CacheStore>;
  using Store = CacheStore;  // for CacheArcIterator

  friend class StateIterator<CompactFst>;
  friend class ArcIterator<CompactFst>;

  CompactFst() : ImplToExpandedFst<Impl>(std::make_shared<Impl>()) {}

  explicit CompactFst(const Fst<Arc> &fst,
                      const CompactFstOptions &opts = CompactFstOptions())
      : CompactFst(fst, std::make_shared<Compactor>(fst), opts) {}

  // Constructs a CompactFst, creating a new Compactor using
  // Compactor(fst, compactor); this uses the compactor arg only for optional
  // information, such as compression level. See the Compactor interface
  // description.
  CompactFst(const Fst<Arc> &fst, std::shared_ptr<Compactor> compactor,
             const CompactFstOptions &opts = CompactFstOptions())
      : ImplToExpandedFst<Impl>(
            std::make_shared<Impl>(fst, std::move(compactor), opts)) {}

  // Convenience constructor taking a Compactor rvalue ref. Avoids
  // clutter of make_shared<Compactor> at call site.
  // Constructs a CompactFst, creating a new Compactor using
  // Compactor(fst, compactor); this uses the compactor arg only for optional
  // information, such as compression level. See the Compactor interface
  // description.
  CompactFst(const Fst<Arc> &fst, Compactor &&compactor,
             const CompactFstOptions &opts = CompactFstOptions())
      : CompactFst(fst, std::make_shared<Compactor>(std::move(compactor)),
                   opts) {}

  explicit CompactFst(std::shared_ptr<Compactor> compactor,
                      const CompactFstOptions &opts = CompactFstOptions())
      : ImplToExpandedFst<Impl>(
            std::make_shared<Impl>(std::move(compactor), opts)) {}

  // See Fst<>::Copy() for doc.
  CompactFst(const CompactFst &fst, bool safe = false)
      : ImplToExpandedFst<Impl>(fst, safe) {}

  // Get a copy of this CompactFst. See Fst<>::Copy() for further doc.
  CompactFst *Copy(bool safe = false) const override {
    return new CompactFst(*this, safe);
  }

  // Read a CompactFst from an input stream; return nullptr on error
  static CompactFst *Read(std::istream &strm, const FstReadOptions &opts) {
    auto *impl = Impl::Read(strm, opts);
    return impl ? new CompactFst(std::shared_ptr<Impl>(impl)) : nullptr;
  }

  // Read a CompactFst from a file; return nullptr on error
  // Empty source reads from standard input
  static CompactFst *Read(const std::string &source) {
    auto *impl = ImplToExpandedFst<Impl>::Read(source);
    return impl ? new CompactFst(std::shared_ptr<Impl>(impl)) : nullptr;
  }

  bool Write(std::ostream &strm, const FstWriteOptions &opts) const override {
    return GetImpl()->Write(strm, opts);
  }

  bool Write(const std::string &source) const override {
    return Fst<Arc>::WriteFile(source);
  }

  void InitStateIterator(StateIteratorData<Arc> *data) const override {
    GetImpl()->InitStateIterator(data);
  }

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const override {
    GetMutableImpl()->InitArcIterator(s, data);
  }

  MatcherBase<Arc> *InitMatcher(MatchType match_type) const override {
    return new SortedMatcher<CompactFst>(*this, match_type);
  }

  const Compactor *GetCompactor() const { return GetImpl()->GetCompactor(); }

  void SetCompactor(std::shared_ptr<Compactor> compactor) {
    GetMutableImpl()->SetCompactor(std::move(compactor));
  }

 private:
  using ImplToFst<Impl, ExpandedFst<Arc>>::GetImpl;
  using ImplToFst<Impl, ExpandedFst<Arc>>::GetMutableImpl;

  explicit CompactFst(std::shared_ptr<Impl> impl)
      : ImplToExpandedFst<Impl>(std::move(impl)) {}

  CompactFst &operator=(const CompactFst &fst) = delete;
};

// Writes FST in ArcCompacted format, with a possible pass over the machine
// before writing to compute the number of states and arcs.
template <class CompactArcFST, class FST>
bool WriteCompactArcFst(
    const FST &fst,
    const typename CompactArcFST::Compactor::ArcCompactor &arc_compactor,
    std::ostream &strm, const FstWriteOptions &opts) {
  using Arc = typename CompactArcFST::Arc;
  using Compactor = typename CompactArcFST::Compactor;
  using ArcCompactor = typename Compactor::ArcCompactor;
  using CompactStore = typename Compactor::CompactStore;
  using Element = typename ArcCompactor::Element;
  using Impl = typename CompactArcFST::Impl;
  using Unsigned = typename Compactor::Unsigned;
  using Weight = typename Arc::Weight;
  const auto file_version =
      opts.align ? Impl::kAlignedFileVersion : Impl::kFileVersion;
  size_t num_arcs = -1;
  size_t num_states = -1;
  auto first_pass_arc_compactor = arc_compactor;
  // Note that GetCompactor will only return non-null if the compactor has the
  // exact type Compactor == CompactArcFst::Compactor. This is what we want;
  // other types must do an extra pass to set the arc compactor state.
  if (const Compactor *const compactor =
          internal::GetCompactor<Compactor>(fst)) {
    num_arcs = compactor->NumArcs();
    num_states = compactor->NumStates();
    first_pass_arc_compactor = *compactor->GetArcCompactor();
  } else {
    // A first pass is needed to compute the state of the compactor, which
    // is saved ahead of the rest of the data structures. This unfortunately
    // means forcing a complete double compaction when writing in this format.
    // TODO(allauzen): eliminate mutable state from compactors.
    num_arcs = 0;
    num_states = 0;
    for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
      const auto s = siter.Value();
      ++num_states;
      if (fst.Final(s) != Weight::Zero()) {
        first_pass_arc_compactor.Compact(
            s, Arc(kNoLabel, kNoLabel, fst.Final(s), kNoStateId));
      }
      for (ArcIterator<FST> aiter(fst, s); !aiter.Done(); aiter.Next()) {
        ++num_arcs;
        first_pass_arc_compactor.Compact(s, aiter.Value());
      }
    }
  }
  FstHeader hdr;
  hdr.SetStart(fst.Start());
  hdr.SetNumStates(num_states);
  hdr.SetNumArcs(num_arcs);
  std::string type = "compact";
  if (sizeof(Unsigned) != sizeof(uint32_t)) {
    type += std::to_string(CHAR_BIT * sizeof(Unsigned));
  }
  type += "_";
  type += ArcCompactor::Type();
  if (CompactStore::Type() != "compact") {
    type += "_";
    type += CompactStore::Type();
  }
  const auto copy_properties = fst.Properties(kCopyProperties, true);
  if ((copy_properties & kError) || !arc_compactor.Compatible(fst)) {
    FSTERROR() << "Fst incompatible with compactor";
    return false;
  }
  uint64_t properties = copy_properties | Impl::kStaticProperties;
  internal::FstImpl<Arc>::WriteFstHeader(fst, strm, opts, file_version, type,
                                         properties, &hdr);
  first_pass_arc_compactor.Write(strm);
  if (first_pass_arc_compactor.Size() == -1) {
    if (opts.align && !AlignOutput(strm)) {
      LOG(ERROR) << "WriteCompactArcFst: Alignment failed: " << opts.source;
      return false;
    }
    Unsigned compacts = 0;
    for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
      const auto s = siter.Value();
      strm.write(reinterpret_cast<const char *>(&compacts), sizeof(compacts));
      if (fst.Final(s) != Weight::Zero()) {
        ++compacts;
      }
      compacts += fst.NumArcs(s);
    }
    strm.write(reinterpret_cast<const char *>(&compacts), sizeof(compacts));
  }
  if (opts.align && !AlignOutput(strm)) {
    LOG(ERROR) << "Could not align file during write after writing states";
  }
  const auto &second_pass_arc_compactor = arc_compactor;
  Element element;
  for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
    const auto s = siter.Value();
    if (fst.Final(s) != Weight::Zero()) {
      element = second_pass_arc_compactor.Compact(
          s, Arc(kNoLabel, kNoLabel, fst.Final(s), kNoStateId));
      strm.write(reinterpret_cast<const char *>(&element), sizeof(element));
    }
    for (ArcIterator<FST> aiter(fst, s); !aiter.Done(); aiter.Next()) {
      element = second_pass_arc_compactor.Compact(s, aiter.Value());
      strm.write(reinterpret_cast<const char *>(&element), sizeof(element));
    }
  }
  strm.flush();
  if (!strm) {
    LOG(ERROR) << "WriteCompactArcFst: Write failed: " << opts.source;
    return false;
  }
  return true;
}

// Specialization for CompactFst; see generic version in fst.h for sample
// usage (but use the CompactFst type!). This version should inline.
template <class Arc, class Compactor, class CacheStore>
class StateIterator<CompactFst<Arc, Compactor, CacheStore>> {
 public:
  using StateId = typename Arc::StateId;

  explicit StateIterator(const CompactFst<Arc, Compactor, CacheStore> &fst)
      : nstates_(fst.NumStates()), s_(0) {}

  bool Done() const { return s_ >= nstates_; }

  StateId Value() const { return s_; }

  void Next() { ++s_; }

  void Reset() { s_ = 0; }

 private:
  StateId nstates_;
  StateId s_;
};

// Specialization for CompactFst. Never caches,
// always iterates over the underlying compact elements.
template <class Arc, class Compactor, class CacheStore>
class ArcIterator<CompactFst<Arc, Compactor, CacheStore>> {
 public:
  using StateId = typename Arc::StateId;
  using State = typename Compactor::State;

  ArcIterator(const CompactFst<Arc, Compactor, CacheStore> &fst, StateId s)
      : state_(fst.GetMutableImpl()->MutableCompactor(), s),
        pos_(0),
        num_arcs_(state_.NumArcs()),
        flags_(kArcValueFlags) {}

  bool Done() const { return pos_ >= num_arcs_; }

  const Arc &Value() const {
    arc_ = state_.GetArc(pos_, flags_);
    return arc_;
  }

  void Next() { ++pos_; }

  size_t Position() const { return pos_; }

  void Reset() { pos_ = 0; }

  void Seek(size_t pos) { pos_ = pos; }

  uint8_t Flags() const { return flags_; }

  void SetFlags(uint8_t flags, uint8_t mask) {
    flags_ &= ~mask;
    flags_ |= (flags & kArcValueFlags);
  }

 private:
  State state_;
  size_t pos_;
  // Cache the value of NumArcs(), since it is used in Done() and may be slow.
  size_t num_arcs_;
  mutable Arc arc_;
  uint8_t flags_;
};

// ArcCompactor for unweighted string FSTs.
template <class A>
class StringCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = Label;

  Element Compact(StateId s, const Arc &arc) const { return arc.ilabel; }

  Arc Expand(StateId s, const Element &p,
             uint8_t flags = kArcValueFlags) const {
    return Arc(p, p, Weight::One(), p != kNoLabel ? s + 1 : kNoStateId);
  }

  constexpr ssize_t Size() const { return 1; }

  constexpr uint64_t Properties() const { return kCompiledStringProperties; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const std::string &Type() {
    static const std::string *const type = new std::string("string");
    return *type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static StringCompactor *Read(std::istream &strm) {
    return new StringCompactor;
  }
};

// ArcCompactor for weighted string FSTs.
template <class A>
class WeightedStringCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = std::pair<Label, Weight>;

  Element Compact(StateId s, const Arc &arc) const {
    return std::make_pair(arc.ilabel, arc.weight);
  }

  Arc Expand(StateId s, const Element &p,
             uint8_t flags = kArcValueFlags) const {
    return Arc(p.first, p.first, p.second,
               p.first != kNoLabel ? s + 1 : kNoStateId);
  }

  constexpr ssize_t Size() const { return 1; }

  constexpr uint64_t Properties() const { return kString | kAcceptor; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const std::string &Type() {
    static const std::string *const type = new std::string("weighted_string");
    return *type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static WeightedStringCompactor *Read(std::istream &strm) {
    return new WeightedStringCompactor;
  }
};

// ArcCompactor for unweighted acceptor FSTs.
template <class A>
class UnweightedAcceptorCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = std::pair<Label, StateId>;

  Element Compact(StateId s, const Arc &arc) const {
    return std::make_pair(arc.ilabel, arc.nextstate);
  }

  Arc Expand(StateId s, const Element &p,
             uint8_t flags = kArcValueFlags) const {
    return Arc(p.first, p.first, Weight::One(), p.second);
  }

  constexpr ssize_t Size() const { return -1; }

  constexpr uint64_t Properties() const { return kAcceptor | kUnweighted; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const std::string &Type() {
    static const std::string *const type =
        new std::string("unweighted_acceptor");
    return *type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static UnweightedAcceptorCompactor *Read(std::istream &istrm) {
    return new UnweightedAcceptorCompactor;
  }
};

// ArcCompactor for weighted acceptor FSTs.
template <class A>
class AcceptorCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = std::pair<std::pair<Label, Weight>, StateId>;

  Element Compact(StateId s, const Arc &arc) const {
    return std::make_pair(std::make_pair(arc.ilabel, arc.weight),
                          arc.nextstate);
  }

  Arc Expand(StateId s, const Element &p,
             uint8_t flags = kArcValueFlags) const {
    return Arc(p.first.first, p.first.first, p.first.second, p.second);
  }

  constexpr ssize_t Size() const { return -1; }

  constexpr uint64_t Properties() const { return kAcceptor; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const std::string &Type() {
    static const std::string *const type = new std::string("acceptor");
    return *type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static AcceptorCompactor *Read(std::istream &strm) {
    return new AcceptorCompactor;
  }
};

// ArcCompactor for unweighted FSTs.
template <class A>
class UnweightedCompactor {
 public:
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using Element = std::pair<std::pair<Label, Label>, StateId>;

  Element Compact(StateId s, const Arc &arc) const {
    return std::make_pair(std::make_pair(arc.ilabel, arc.olabel),
                          arc.nextstate);
  }

  Arc Expand(StateId s, const Element &p,
             uint8_t flags = kArcValueFlags) const {
    return Arc(p.first.first, p.first.second, Weight::One(), p.second);
  }

  constexpr ssize_t Size() const { return -1; }

  constexpr uint64_t Properties() const { return kUnweighted; }

  bool Compatible(const Fst<Arc> &fst) const {
    const auto props = Properties();
    return fst.Properties(props, true) == props;
  }

  static const std::string &Type() {
    static const std::string *const type = new std::string("unweighted");
    return *type;
  }

  bool Write(std::ostream &strm) const { return true; }

  static UnweightedCompactor *Read(std::istream &strm) {
    return new UnweightedCompactor;
  }
};

template <class Arc, class Unsigned /* = uint32_t */>
using CompactStringFst = CompactArcFst<Arc, StringCompactor<Arc>, Unsigned>;

template <class Arc, class Unsigned /* = uint32_t */>
using CompactWeightedStringFst =
    CompactArcFst<Arc, WeightedStringCompactor<Arc>, Unsigned>;

template <class Arc, class Unsigned /* = uint32_t */>
using CompactAcceptorFst = CompactArcFst<Arc, AcceptorCompactor<Arc>, Unsigned>;

template <class Arc, class Unsigned /* = uint32_t */>
using CompactUnweightedFst =
    CompactArcFst<Arc, UnweightedCompactor<Arc>, Unsigned>;

template <class Arc, class Unsigned /* = uint32_t */>
using CompactUnweightedAcceptorFst =
    CompactArcFst<Arc, UnweightedAcceptorCompactor<Arc>, Unsigned>;

using StdCompactStringFst = CompactStringFst<StdArc, uint32_t>;

using StdCompactWeightedStringFst = CompactWeightedStringFst<StdArc, uint32_t>;

using StdCompactAcceptorFst = CompactAcceptorFst<StdArc, uint32_t>;

using StdCompactUnweightedFst = CompactUnweightedFst<StdArc, uint32_t>;

using StdCompactUnweightedAcceptorFst =
    CompactUnweightedAcceptorFst<StdArc, uint32_t>;

// Convenience function to make a CompactStringFst from a sequence
// of Arc::Labels. LabelIterator must be an input iterator.
template <class Arc, class Unsigned = uint32_t, class LabelIterator>
inline CompactStringFst<Arc, Unsigned> MakeCompactStringFst(
    const LabelIterator begin, const LabelIterator end) {
  using CompactStringFst = CompactStringFst<Arc, Unsigned>;
  using Compactor = typename CompactStringFst::Compactor;
  return CompactStringFst(std::make_shared<Compactor>(begin, end));
}

template <class LabelIterator>
inline StdCompactStringFst MakeStdCompactStringFst(const LabelIterator begin,
                                                   const LabelIterator end) {
  return MakeCompactStringFst<StdArc>(begin, end);
}

}  // namespace fst

#endif  // FST_COMPACT_FST_H_
