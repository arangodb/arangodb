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
// Class to encode and decode an FST.

#ifndef FST_ENCODE_H_
#define FST_ENCODE_H_

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <fst/log.h>
#include <fst/arc-map.h>
#include <fstream>
#include <fst/properties.h>
#include <fst/rmfinalepsilon.h>
#include <fst/util.h>
#include <unordered_map>

namespace fst {

enum EncodeType { ENCODE = 1, DECODE = 2 };

inline constexpr uint8_t kEncodeLabels = 0x01;
inline constexpr uint8_t kEncodeWeights = 0x02;
inline constexpr uint8_t kEncodeFlags = 0x03;

namespace internal {

// Bits storing whether or not an encode table has input and/or output symbol
// tables, for internal use only.
inline constexpr uint8_t kEncodeHasISymbols = 0x04;
inline constexpr uint8_t kEncodeHasOSymbols = 0x08;

// Identifies stream data as an encode table (and its endianity).
inline constexpr int32_t kEncodeMagicNumber = 2128178506;

}  // namespace internal

// Header for the encoder table.
class EncodeTableHeader {
 public:
  EncodeTableHeader() = default;

  // Getters.

  const std::string &ArcType() const { return arctype_; }

  uint8_t Flags() const { return flags_; }

  size_t Size() const { return size_; }

  // Setters.

  void SetArcType(const std::string &arctype) { arctype_ = arctype; }

  void SetFlags(uint8_t flags) { flags_ = flags; }

  void SetSize(size_t size) { size_ = size; }

  // IO.

  bool Read(std::istream &strm, const std::string &source);

  bool Write(std::ostream &strm, const std::string &source) const;

 private:
  std::string arctype_;
  uint8_t flags_;
  size_t size_;
};

namespace internal {

// The following class encapsulates implementation details for the encoding and
// decoding of label/weight triples used for encoding and decoding of FSTs. The
// EncodeTable is bidirectional, i.e, it stores both the Triple of encode labels
// and weights to a unique label, and the reverse.
template <class Arc>
class EncodeTable {
 public:
  using Label = typename Arc::Label;
  using Weight = typename Arc::Weight;

  // Encoded data consists of arc input/output labels and arc weight.
  struct Triple {
    Triple() = default;

    Triple(Label ilabel, Label olabel, Weight weight)
        : ilabel(ilabel), olabel(olabel), weight(std::move(weight)) {}

    // Constructs from arc and flags.
    Triple(const Arc &arc, uint8_t flags)
        : ilabel(arc.ilabel),
          olabel(flags & kEncodeLabels ? arc.olabel : 0),
          weight(flags & kEncodeWeights ? arc.weight : Weight::One()) {}

    static std::unique_ptr<Triple> Read(std::istream &strm) {
      auto triple = std::make_unique<Triple>();
      ReadType(strm, &triple->ilabel);
      ReadType(strm, &triple->olabel);
      ReadType(strm, &triple->weight);
      return triple;
    }

    void Write(std::ostream &strm) const {
      WriteType(strm, ilabel);
      WriteType(strm, olabel);
      WriteType(strm, weight);
    }

    // Exploited below for TripleEqual functor.
    bool operator==(const Triple &other) const {
      return (ilabel == other.ilabel && olabel == other.olabel &&
              weight == other.weight);
    }

    Label ilabel;
    Label olabel;
    Weight weight;
  };

  // Equality functor for two Triple pointers.
  struct TripleEqual {
    bool operator()(const Triple *x, const Triple *y) const { return *x == *y; }
  };

  // Hash functor for one Triple pointer.
  class TripleHash {
   public:
    explicit TripleHash(uint8_t flags) : flags_(flags) {}

    size_t operator()(const Triple *triple) const {
      size_t hash = triple->ilabel;
      static constexpr int lshift = 5;
      static constexpr int rshift = CHAR_BIT * sizeof(size_t) - 5;
      if (flags_ & kEncodeLabels) {
        hash = hash << lshift ^ hash >> rshift ^ triple->olabel;
      }
      if (flags_ & kEncodeWeights) {
        hash = hash << lshift ^ hash >> rshift ^ triple->weight.Hash();
      }
      return hash;
    }

   private:
    uint8_t flags_;
  };

  explicit EncodeTable(uint8_t flags)
      : flags_(flags), triple2label_(1024, TripleHash(flags)) {}

  // Given an arc, encodes either input/output labels or input/costs or both.
  Label Encode(const Arc &arc) {
    // Encoding weights of a weighted superfinal transition could result in
    // a clash with a true epsilon arc; to avoid this we hallucinate kNoLabel
    // labels instead.
    if (arc.nextstate == kNoStateId && (flags_ & kEncodeWeights)) {
      return Encode(std::make_unique<Triple>(kNoLabel, kNoLabel, arc.weight));
    } else {
      return Encode(std::make_unique<Triple>(arc, flags_));
    }
  }

  // Given an encoded arc label, decodes back to input/output labels and costs.
  const Triple *Decode(Label label) const {
    if (label < 1 || label > triples_.size()) {
      LOG(ERROR) << "EncodeTable::Decode: Unknown decode label: " << label;
      return nullptr;
    }
    return triples_[label - 1].get();
  }

  size_t Size() const { return triples_.size(); }

  static EncodeTable *Read(std::istream &strm, const std::string &source);

  bool Write(std::ostream &strm, const std::string &source) const;

  // This is masked to hide internal-only isymbol and osymbol bits.

  uint8_t Flags() const { return flags_ & kEncodeFlags; }

  const SymbolTable *InputSymbols() const { return isymbols_.get(); }

  const SymbolTable *OutputSymbols() const { return osymbols_.get(); }

  void SetInputSymbols(const SymbolTable *syms) {
    if (syms) {
      isymbols_.reset(syms->Copy());
      flags_ |= kEncodeHasISymbols;
    } else {
      isymbols_.reset();
      flags_ &= ~kEncodeHasISymbols;
    }
  }

  void SetOutputSymbols(const SymbolTable *syms) {
    if (syms) {
      osymbols_.reset(syms->Copy());
      flags_ |= kEncodeHasOSymbols;
    } else {
      osymbols_.reset();
      flags_ &= ~kEncodeHasOSymbols;
    }
  }

 private:
  Label Encode(std::unique_ptr<Triple> triple) {
    auto insert_result =
        triple2label_.emplace(triple.get(), triples_.size() + 1);
    if (insert_result.second) triples_.push_back(std::move(triple));
    return insert_result.first->second;
  }

  uint8_t flags_;
  std::vector<std::unique_ptr<Triple>> triples_;
  std::unordered_map<const Triple *, Label, TripleHash, TripleEqual>
      triple2label_;
  std::unique_ptr<SymbolTable> isymbols_;
  std::unique_ptr<SymbolTable> osymbols_;

  EncodeTable(const EncodeTable &) = delete;
  EncodeTable &operator=(const EncodeTable &) = delete;
};

template <class Arc>
EncodeTable<Arc> *EncodeTable<Arc>::Read(std::istream &strm,
                                         const std::string &source) {
  EncodeTableHeader hdr;
  if (!hdr.Read(strm, source)) return nullptr;
  const auto flags = hdr.Flags();
  const auto size = hdr.Size();
  auto table = std::make_unique<EncodeTable>(flags);
  for (int64_t i = 0; i < size; ++i) {
    table->triples_.emplace_back(std::move(Triple::Read(strm)));
    table->triple2label_[table->triples_.back().get()] = table->triples_.size();
  }
  if (flags & kEncodeHasISymbols) {
    table->isymbols_.reset(SymbolTable::Read(strm, source));
  }
  if (flags & kEncodeHasOSymbols) {
    table->osymbols_.reset(SymbolTable::Read(strm, source));
  }
  if (!strm) {
    LOG(ERROR) << "EncodeTable::Read: Read failed: " << source;
    return nullptr;
  }
  return table.release();
}

template <class Arc>
bool EncodeTable<Arc>::Write(std::ostream &strm,
                             const std::string &source) const {
  EncodeTableHeader hdr;
  hdr.SetArcType(Arc::Type());
  hdr.SetFlags(flags_);  // Real flags, not masked ones.
  hdr.SetSize(Size());
  if (!hdr.Write(strm, source)) return false;
  for (const auto &triple : triples_) triple->Write(strm);
  if (flags_ & kEncodeHasISymbols) isymbols_->Write(strm);
  if (flags_ & kEncodeHasOSymbols) osymbols_->Write(strm);
  strm.flush();
  if (!strm) {
    LOG(ERROR) << "EncodeTable::Write: Write failed: " << source;
    return false;
  }
  return true;
}

}  // namespace internal

// A mapper to encode/decode weighted transducers. Encoding of an FST is used
// for performing classical determinization or minimization on a weighted
// transducer viewing it as an unweighted acceptor over encoded labels.
//
// The mapper stores the encoding in a local hash table (EncodeTable). This
// table is shared (and reference-counted) between the encoder and decoder.
// A decoder has read-only access to the EncodeTable.
//
// The EncodeMapper allows on the fly encoding of the machine. As the
// EncodeTable is generated the same table may by used to decode the machine
// on the fly. For example in the following sequence of operations
//
//  Encode -> Determinize -> Decode
//
// we will use the encoding table generated during the encode step in the
// decode, even though the encoding is not complete.
template <class Arc>
class EncodeMapper {
  using Label = typename Arc::Label;
  using Weight = typename Arc::Weight;

 public:
  explicit EncodeMapper(uint8_t flags, EncodeType type = ENCODE)
      : flags_(flags),
        type_(type),
        table_(std::make_shared<internal::EncodeTable<Arc>>(flags)),
        error_(false) {}

  EncodeMapper(const EncodeMapper &mapper)
      : flags_(mapper.flags_),
        type_(mapper.type_),
        table_(mapper.table_),
        error_(false) {}

  // Copy constructor but setting the type, typically to DECODE.
  EncodeMapper(const EncodeMapper &mapper, EncodeType type)
      : flags_(mapper.flags_),
        type_(type),
        table_(mapper.table_),
        error_(mapper.error_) {}

  Arc operator()(const Arc &arc);

  MapFinalAction FinalAction() const {
    return (type_ == ENCODE && (flags_ & kEncodeWeights))
               ? MAP_REQUIRE_SUPERFINAL
               : MAP_NO_SUPERFINAL;
  }

  constexpr MapSymbolsAction InputSymbolsAction() const {
    return MAP_CLEAR_SYMBOLS;
  }

  constexpr MapSymbolsAction OutputSymbolsAction() const {
    return MAP_CLEAR_SYMBOLS;
  }

  uint8_t Flags() const { return flags_; }

  uint64_t Properties(uint64_t inprops) {
    uint64_t outprops = inprops;
    if (error_) outprops |= kError;
    uint64_t mask = kFstProperties;
    if (flags_ & kEncodeLabels) {
      mask &= kILabelInvariantProperties & kOLabelInvariantProperties;
    }
    if (flags_ & kEncodeWeights) {
      mask &= kILabelInvariantProperties & kWeightInvariantProperties &
              (type_ == ENCODE ? kAddSuperFinalProperties
                               : kRmSuperFinalProperties);
    }
    if (type_ == ENCODE) mask |= kIDeterministic;
    return outprops & mask;
  }

  EncodeType Type() const { return type_; }

  static EncodeMapper *Read(std::istream &strm, const std::string &source,
                            EncodeType type = ENCODE) {
    auto *table = internal::EncodeTable<Arc>::Read(strm, source);
    return table ? new EncodeMapper(table->Flags(), type, table) : nullptr;
  }

  static EncodeMapper *Read(const std::string &source,
                            EncodeType type = ENCODE) {
    std::ifstream strm(source, std::ios_base::in | std::ios_base::binary);
    if (!strm) {
      LOG(ERROR) << "EncodeMapper: Can't open file: " << source;
      return nullptr;
    }
    return Read(strm, source, type);
  }

  bool Write(std::ostream &strm, const std::string &source) const {
    return table_->Write(strm, source);
  }

  bool Write(const std::string &source) const {
    std::ofstream strm(source,
                             std::ios_base::out | std::ios_base::binary);
    if (!strm) {
      LOG(ERROR) << "EncodeMapper: Can't open file: " << source;
      return false;
    }
    return Write(strm, source);
  }

  const SymbolTable *InputSymbols() const { return table_->InputSymbols(); }

  const SymbolTable *OutputSymbols() const { return table_->OutputSymbols(); }

  void SetInputSymbols(const SymbolTable *syms) {
    table_->SetInputSymbols(syms);
  }

  void SetOutputSymbols(const SymbolTable *syms) {
    table_->SetOutputSymbols(syms);
  }

 private:
  uint8_t flags_;
  EncodeType type_;
  std::shared_ptr<internal::EncodeTable<Arc>> table_;
  bool error_;

  explicit EncodeMapper(uint8_t flags, EncodeType type,
                        internal::EncodeTable<Arc> *table)
      : flags_(flags), type_(type), table_(table), error_(false) {}

  EncodeMapper &operator=(const EncodeMapper &) = delete;
};

template <class Arc>
Arc EncodeMapper<Arc>::operator()(const Arc &arc) {
  if (type_ == ENCODE) {
    // If this arc is a hallucinated final state, and we're either not encoding
    // weights, or we're encoding weights but this is non-final, we use an
    // identity-encoding.
    if (arc.nextstate == kNoStateId &&
        ((!(flags_ & kEncodeWeights) ||
          ((flags_ & kEncodeWeights) && arc.weight == Weight::Zero())))) {
      return arc;
    } else {
      const auto label = table_->Encode(arc);
      return Arc(label, flags_ & kEncodeLabels ? label : arc.olabel,
                 flags_ & kEncodeWeights ? Weight::One() : arc.weight,
                 arc.nextstate);
    }
  } else {  // type_ == DECODE
    if (arc.nextstate == kNoStateId) {
      return arc;
    } else {
      if (arc.ilabel == 0) return arc;
      if (flags_ & kEncodeLabels && arc.ilabel != arc.olabel) {
        FSTERROR() << "EncodeMapper: Label-encoded arc has different "
                      "input and output labels";
        error_ = true;
      }
      if (flags_ & kEncodeWeights && arc.weight != Weight::One()) {
        FSTERROR() << "EncodeMapper: Weight-encoded arc has non-trivial weight";
        error_ = true;
      }
      const auto triple = table_->Decode(arc.ilabel);
      if (!triple) {
        FSTERROR() << "EncodeMapper: Decode failed";
        error_ = true;
        return Arc(kNoLabel, kNoLabel, Weight::NoWeight(), arc.nextstate);
      } else if (triple->ilabel == kNoLabel) {
        // Hallucinated kNoLabel from a weighted superfinal transition.
        return Arc(0, 0, triple->weight, arc.nextstate);
      } else {
        return Arc(triple->ilabel,
                   flags_ & kEncodeLabels ? triple->olabel : arc.olabel,
                   flags_ & kEncodeWeights ? triple->weight : arc.weight,
                   arc.nextstate);
      }
    }
  }
}

// Complexity: O(E + V).
template <class Arc>
inline void Encode(MutableFst<Arc> *fst, EncodeMapper<Arc> *mapper) {
  mapper->SetInputSymbols(fst->InputSymbols());
  mapper->SetOutputSymbols(fst->OutputSymbols());
  ArcMap(fst, mapper);
}

template <class Arc>
inline void Decode(MutableFst<Arc> *fst, const EncodeMapper<Arc> &mapper) {
  ArcMap(fst, EncodeMapper<Arc>(mapper, DECODE));
  RmFinalEpsilon(fst);
  fst->SetInputSymbols(mapper.InputSymbols());
  fst->SetOutputSymbols(mapper.OutputSymbols());
}

// On-the-fly encoding of an input FST.
//
// Complexity:
//
//   Construction: O(1)
//   Traversal: O(e + v)
//
// where e is the number of arcs visited and v is the number of states visited.
// Constant time and space to visit an input state or arc is assumed and
// exclusive of caching.
template <class Arc>
class EncodeFst : public ArcMapFst<Arc, Arc, EncodeMapper<Arc>> {
 public:
  using Mapper = EncodeMapper<Arc>;
  using Impl = internal::ArcMapFstImpl<Arc, Arc, Mapper>;

  EncodeFst(const Fst<Arc> &fst, Mapper *encoder)
      : ArcMapFst<Arc, Arc, Mapper>(fst, encoder, ArcMapFstOptions()) {
    encoder->SetInputSymbols(fst.InputSymbols());
    encoder->SetOutputSymbols(fst.OutputSymbols());
  }

  EncodeFst(const Fst<Arc> &fst, const Mapper &encoder)
      : ArcMapFst<Arc, Arc, Mapper>(fst, encoder, ArcMapFstOptions()) {}

  // See Fst<>::Copy() for doc.
  EncodeFst(const EncodeFst &fst, bool copy = false)
      : ArcMapFst<Arc, Arc, Mapper>(fst, copy) {}

  // Makes a copy of this EncodeFst. See Fst<>::Copy() for further doc.
  EncodeFst *Copy(bool safe = false) const override {
    if (safe) {
      FSTERROR() << "EncodeFst::Copy(true): Not allowed";
      GetImpl()->SetProperties(kError, kError);
    }
    return new EncodeFst(*this);
  }

 private:
  using ImplToFst<Impl>::GetImpl;
  using ImplToFst<Impl>::GetMutableImpl;
};

// On-the-fly decoding of an input FST.
//
// Complexity:
//
//   Construction: O(1).
//   Traversal: O(e + v)
//
// Constant time and space to visit an input state or arc is assumed and
// exclusive of caching.
template <class Arc>
class DecodeFst : public ArcMapFst<Arc, Arc, EncodeMapper<Arc>> {
 public:
  using Mapper = EncodeMapper<Arc>;
  using Impl = internal::ArcMapFstImpl<Arc, Arc, Mapper>;

  DecodeFst(const Fst<Arc> &fst, const Mapper &encoder)
      : ArcMapFst<Arc, Arc, Mapper>(fst, Mapper(encoder, DECODE),
                                    ArcMapFstOptions()) {
    GetMutableImpl()->SetInputSymbols(encoder.InputSymbols());
    GetMutableImpl()->SetOutputSymbols(encoder.OutputSymbols());
  }

  // See Fst<>::Copy() for doc.
  DecodeFst(const DecodeFst &fst, bool safe = false)
      : ArcMapFst<Arc, Arc, Mapper>(fst, safe) {}

  // Makes a copy of this DecodeFst. See Fst<>::Copy() for further doc.
  DecodeFst *Copy(bool safe = false) const override {
    return new DecodeFst(*this, safe);
  }

 private:
  using ImplToFst<Impl>::GetImpl;
  using ImplToFst<Impl>::GetMutableImpl;
};

// Specialization for EncodeFst.
template <class Arc>
class StateIterator<EncodeFst<Arc>>
    : public StateIterator<ArcMapFst<Arc, Arc, EncodeMapper<Arc>>> {
 public:
  explicit StateIterator(const EncodeFst<Arc> &fst)
      : StateIterator<ArcMapFst<Arc, Arc, EncodeMapper<Arc>>>(fst) {}
};

// Specialization for EncodeFst.
template <class Arc>
class ArcIterator<EncodeFst<Arc>>
    : public ArcIterator<ArcMapFst<Arc, Arc, EncodeMapper<Arc>>> {
 public:
  ArcIterator(const EncodeFst<Arc> &fst, typename Arc::StateId s)
      : ArcIterator<ArcMapFst<Arc, Arc, EncodeMapper<Arc>>>(fst, s) {}
};

// Specialization for DecodeFst.
template <class Arc>
class StateIterator<DecodeFst<Arc>>
    : public StateIterator<ArcMapFst<Arc, Arc, EncodeMapper<Arc>>> {
 public:
  explicit StateIterator(const DecodeFst<Arc> &fst)
      : StateIterator<ArcMapFst<Arc, Arc, EncodeMapper<Arc>>>(fst) {}
};

// Specialization for DecodeFst.
template <class Arc>
class ArcIterator<DecodeFst<Arc>>
    : public ArcIterator<ArcMapFst<Arc, Arc, EncodeMapper<Arc>>> {
 public:
  ArcIterator(const DecodeFst<Arc> &fst, typename Arc::StateId s)
      : ArcIterator<ArcMapFst<Arc, Arc, EncodeMapper<Arc>>>(fst, s) {}
};

// Useful aliases when using StdArc.

using StdEncodeFst = EncodeFst<StdArc>;

using StdDecodeFst = DecodeFst<StdArc>;

}  // namespace fst

#endif  // FST_ENCODE_H_
