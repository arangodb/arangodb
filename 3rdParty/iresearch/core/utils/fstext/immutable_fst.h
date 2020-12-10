////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_IMMUTABLE_FST_H
#define IRESEARCH_IMMUTABLE_FST_H

#include <fst/fst.h>
#include <fst/vector-fst.h>
#include <fst/expanded-fst.h>

#include "shared.hpp"
#include "store/data_output.hpp"
#include "store/data_input.hpp"
#include "utils/misc.hpp"

namespace fst {
namespace fstext {

namespace detail {
DEFINE_HAS_MEMBER(FinalRef);
}

template<typename Arc>
class ImmutableFst;

template<typename A>
class ImmutableFstImpl : public internal::FstImpl<A> {
 public:
  using Arc = A;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  using internal::FstImpl<A>::SetInputSymbols;
  using internal::FstImpl<A>::SetOutputSymbols;
  using internal::FstImpl<A>::SetType;
  using internal::FstImpl<A>::SetProperties;
  using internal::FstImpl<A>::Properties;

  static constexpr const char kTypePrefix[] = "immutable";

  ImmutableFstImpl()
      : narcs_(0),
        nstates_(0),
        start_(kNoStateId) {
    SetType(std::string(kTypePrefix) + "<" + Arc::Type() + ">");
    SetProperties(kNullProperties | kStaticProperties);
  }

  StateId Start() const noexcept { return start_; }

  Weight Final(StateId s) const noexcept { return states_[s].weight; }

  const Weight& FinalRef(StateId s) const noexcept { return states_[s].weight; }

  StateId NumStates() const noexcept { return nstates_; }

  size_t NumArcs(StateId s) const noexcept { return states_[s].narcs; }

  size_t NumInputEpsilons(StateId) const noexcept { return 0; }

  size_t NumOutputEpsilons(StateId) const noexcept { return 0; }

  static std::shared_ptr<ImmutableFstImpl<Arc>> Read(irs::data_input& strm);

  const Arc* Arcs(StateId s) const noexcept { return states_[s].arcs; }

  // Provide information needed for generic state iterator.
  void InitStateIterator(StateIteratorData<Arc> *data) const noexcept {
    data->base = nullptr;
    data->nstates = nstates_;
  }

  // Provide information needed for the generic arc iterator.
  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const noexcept {
    data->base = nullptr;
    data->arcs = states_[s].arcs;
    data->narcs = states_[s].narcs;
    data->ref_count = nullptr;
  }

 private:
  friend class ImmutableFst<Arc>;

  enum class Version : irs::byte_type {
    MIN = 0
  };

  struct State {
    const Arc* arcs; // Start of state's arcs in *arcs_.
    size_t narcs;    // Number of arcs (per state).
    Weight weight;   // Final weight.
  };

  // Properties always true of this FST class.
  static constexpr uint64 kStaticProperties = kExpanded;

  std::unique_ptr<State[]> states_;
  std::unique_ptr<Arc[]> arcs_;
  std::unique_ptr<irs::byte_type[]> weights_;
  size_t narcs_;                               // Number of arcs.
  StateId nstates_;                            // Number of states.
  StateId start_;                              // Initial state.

  ImmutableFstImpl(const ImmutableFstImpl &) = delete;
  ImmutableFstImpl &operator=(const ImmutableFstImpl &) = delete;
};

template<typename Arc>
std::shared_ptr<ImmutableFstImpl<Arc>> ImmutableFstImpl<Arc>::Read(irs::data_input& stream) {
  auto impl = std::make_shared<ImmutableFstImpl<Arc>>();

  // read header
  if (Version(stream.read_byte()) != Version::MIN) {
    return nullptr;
  }

  const uint64_t props = stream.read_long();
  const size_t total_weight_size = stream.read_long();
  const StateId start = stream.read_vint();
  const size_t nstates = stream.read_vlong();
  const size_t narcs = stream.read_vlong();

  auto states = std::make_unique<State[]>(nstates);
  auto arcs = std::make_unique<Arc[]>(narcs);
  auto weights = std::make_unique<irs::byte_type[]>(total_weight_size);

  // read states & arcs
  auto* weight = weights.get();
  auto* arc = arcs.get();
  for (auto state = states.get(), end = state + nstates; state != end; ++state) {
    state->arcs = arc;
    state->narcs = stream.read_byte(); // FIXME total number of arcs can be encoded with 1 byte
    state->weight = { weight, stream.read_vlong() };

    weight += state->weight.Size();

    for (auto* end = arc + state->narcs; arc != end; ++arc) {
      arc->ilabel = stream.read_byte();
      arc->nextstate = stream.read_vint();
      arc->weight = { weight, stream.read_vlong() };
      weight += arc->weight.Size();
    }
  }

  // read weights
  stream.read_bytes(weights.get(), total_weight_size);

  // noexcept block
  impl->properties_ = props;
  impl->start_ = start;
  impl->nstates_ = nstates;
  impl->narcs_ = narcs;
  impl->states_ = std::move(states);
  impl->arcs_ = std::move(arcs);
  impl->weights_ = std::move(weights);

  return impl;
}

template<typename A>
class ImmutableFst : public ImplToExpandedFst<ImmutableFstImpl<A>> {
 public:
  using Arc = A;
  using StateId = typename Arc::StateId;

  using Impl = ImmutableFstImpl<A>;

  friend class StateIterator<ImmutableFst<Arc>>;
  friend class ArcIterator<ImmutableFst<Arc>>;

  template<typename F, typename G>
  void friend Cast(const F&, G*);

  ImmutableFst() : ImplToExpandedFst<Impl>(std::make_shared<Impl>()) {}

  explicit ImmutableFst(const ImmutableFst<A> &fst, bool safe = false)
    : ImplToExpandedFst<Impl>(fst) {}

  // Gets a copy of this ConstFst. See Fst<>::Copy() for further doc.
  ImmutableFst<A>* Copy(bool safe = false) const override {
    return new ImmutableFst<A>(*this, safe);
  }

  static ImmutableFst<A>* Read(irs::data_input& strm) {
    auto impl = Impl::Read(strm);
    return impl ? new ImmutableFst<A>(std::move(impl)) : nullptr;
  }

  // for OpenFST API compliance
  static ImmutableFst<A>* Read(std::istream& strm,
                               const FstReadOptions& /*opts*/) {
#ifdef IRESEARCH_DEBUG
    auto* rdbuf = dynamic_cast<irs::input_buf*>(strm.rdbuf());
#else
    auto* rdbuf = static_cast<irs::input_buf*>(strm.rdbuf());
#endif
    assert(rdbuf && rdbuf->internal());

    return Read(*rdbuf->internal());
  }

  template<typename FST, typename Stats>
  static bool Write(const FST& fst,
                    irs::data_output& strm,
                    const Stats& stats);

  void InitStateIterator(StateIteratorData<Arc> *data) const override {
    GetImpl()->InitStateIterator(data);
  }

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const override {
    GetImpl()->InitArcIterator(s, data);
  }

  using ImplToFst<Impl, ExpandedFst<Arc>>::GetImpl;

 private:
  explicit ImmutableFst(std::shared_ptr<Impl> impl)
      : ImplToExpandedFst<Impl>(impl) {}

  using ImplToExpandedFst<ImmutableFstImpl<A>>::Write;

  ImmutableFst(const ImmutableFst&) = delete;
  ImmutableFst& operator=(const ImmutableFst&) = delete;
};

template <typename A>
template <typename FST, typename Stats>
bool ImmutableFst<A>::Write(
    const FST& fst,
    irs::data_output& stream,
    const Stats& stats) {
  auto* impl = fst.GetImpl();
  assert(impl);

  const auto properties =
    fst.Properties(kCopyProperties, true) |
    Impl::kStaticProperties;

  // write header
  stream.write_byte(static_cast<irs::byte_type>(Impl::Version::MIN));
  stream.write_long(properties);
  stream.write_long(stats.total_weight_size);
  stream.write_vint(fst.Start());
  stream.write_vlong(stats.num_states);
  stream.write_vlong(stats.num_arcs);

  // FIXME SIMD???
  // write states & arcs
  for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
    const StateId s = siter.Value();
    const size_t narcs = impl->NumArcs(s);

    assert(narcs <= std::numeric_limits<irs::byte_type>::max());
    stream.write_byte(static_cast<irs::byte_type>(narcs & 0xFF));
    if constexpr (detail::has_member_FinalRef_v<typename FST::Impl>) {
      stream.write_vlong(impl->FinalRef(s).Size());
    } else {
      stream.write_vlong(impl->Final(s).Size());
    }

    for (ArcIterator<FST> aiter(fst, s); !aiter.Done(); aiter.Next()) {
      const auto& arc = aiter.Value();

      assert(arc.ilabel <= std::numeric_limits<irs::byte_type>::max());
      stream.write_byte(static_cast<irs::byte_type>(arc.ilabel & 0xFF));
      stream.write_vint(arc.nextstate);
      stream.write_vlong(arc.weight.Size());
    }
  }

  // write weights
  for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next()) {
    const StateId s = siter.Value();

    if constexpr (detail::has_member_FinalRef_v<typename FST::Impl>) {
      const auto& weight = impl->FinalRef(s);
      if (!weight.Empty()) {
        stream.write_bytes(weight.c_str(), weight.Size());
      }
    } else {
      const auto weight = impl->Final(s);
      if (!weight.Empty()) {
        stream.write_bytes(weight.c_str(), weight.Size());
      }
    }

    for (ArcIterator<FST> aiter(fst, s); !aiter.Done(); aiter.Next()) {
      const auto& weight = aiter.Value().weight;

      if (!weight.Empty()) {
        stream.write_bytes(weight.c_str(), weight.Size());
      }
    }
  }

  return true;
}

} // fstext

// Specialization for ConstFst; see generic version in fst.h for sample usage
// (but use the ConstFst type instead). This version should inline.
template<typename Arc>
class StateIterator<fstext::ImmutableFst<Arc>> {
 public:
  using StateId = typename Arc::StateId;

  explicit StateIterator(const fstext::ImmutableFst<Arc> &fst)
      : nstates_(fst.GetImpl()->NumStates()), s_(0) {}

  bool Done() const noexcept { return s_ >= nstates_; }

  StateId Value() const noexcept { return s_; }

  void Next() noexcept { ++s_; }

  void Reset() noexcept { s_ = 0; }

 private:
  const StateId nstates_;
  StateId s_;
};

// Specialization for ConstFst; see generic version in fst.h for sample usage
// (but use the ConstFst type instead). This version should inline.
template<typename Arc>
class ArcIterator<fstext::ImmutableFst<Arc>> {
 public:
  using StateId = typename Arc::StateId;

  ArcIterator(const fstext::ImmutableFst<Arc> &fst, StateId s)
      : arcs_(fst.GetImpl()->Arcs(s)),
        begin_(arcs_),
        end_(arcs_ + fst.GetImpl()->NumArcs(s)) {
  }

  bool Done() const noexcept { return begin_ >= end_; }

  const Arc& Value() const noexcept { return *begin_; }

  void Next() noexcept { ++begin_; }

  size_t Position() const noexcept {
    return size_t(std::distance(arcs_, begin_));
  }

  void Reset() noexcept { begin_ = arcs_; }

  void Seek(size_t a) noexcept { begin_ = arcs_ + a; }

  constexpr uint32 Flags() const { return kArcValueFlags; }

  void SetFlags(uint32, uint32) {}

 private:
  const Arc* arcs_;
  const Arc* begin_;
  const Arc* end_;
};

} // fst

#endif // IRESEARCH_IMMUTABLE_FST_H

