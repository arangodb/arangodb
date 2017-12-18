// encode.h

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
// Author: johans@google.com (Johan Schalkwyk)
//
// \file
// Class to encode and decoder an fst.

#ifndef FST_LIB_ENCODE_H__
#define FST_LIB_ENCODE_H__

#include <climits>
#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <string>
#include <vector>
using std::vector;

#include <fst/arc-map.h>
#include <fst/rmfinalepsilon.h>


namespace fst {

static const uint32 kEncodeLabels      = 0x0001;
static const uint32 kEncodeWeights     = 0x0002;
static const uint32 kEncodeFlags       = 0x0003;  // All non-internal flags

static const uint32 kEncodeHasISymbols = 0x0004;  // For internal use
static const uint32 kEncodeHasOSymbols = 0x0008;  // For internal use

enum EncodeType { ENCODE = 1, DECODE = 2 };

// Identifies stream data as an encode table (and its endianity)
static const int32 kEncodeMagicNumber = 2129983209;


// The following class encapsulates implementation details for the
// encoding and decoding of label/weight tuples used for encoding
// and decoding of Fsts. The EncodeTable is bidirectional. I.E it
// stores both the Tuple of encode labels and weights to a unique
// label, and the reverse.
template <class A>  class EncodeTable {
 public:
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;

  // Encoded data consists of arc input/output labels and arc weight
  struct Tuple {
    Tuple() {}
    Tuple(Label ilabel_, Label olabel_, Weight weight_)
        : ilabel(ilabel_), olabel(olabel_), weight(weight_) {}
    Tuple(const Tuple& tuple)
        : ilabel(tuple.ilabel), olabel(tuple.olabel), weight(tuple.weight) {}

    Label ilabel;
    Label olabel;
    Weight weight;
  };

  // Comparison object for hashing EncodeTable Tuple(s).
  class TupleEqual {
   public:
    bool operator()(const Tuple* x, const Tuple* y) const {
      return (x->ilabel == y->ilabel &&
              x->olabel == y->olabel &&
              x->weight == y->weight);
    }
  };

  // Hash function for EncodeTabe Tuples. Based on the encode flags
  // we either hash the labels, weights or combination of them.
  class TupleKey {
   public:
    TupleKey()
        : encode_flags_(kEncodeLabels | kEncodeWeights) {}

    TupleKey(const TupleKey& key)
        : encode_flags_(key.encode_flags_) {}

    explicit TupleKey(uint32 encode_flags)
        : encode_flags_(encode_flags) {}

    size_t operator()(const Tuple* x) const {
      size_t hash = x->ilabel;
      const int lshift = 5;
      const int rshift = CHAR_BIT * sizeof(size_t) - 5;
      if (encode_flags_ & kEncodeLabels)
        hash = hash << lshift ^ hash >> rshift ^ x->olabel;
      if (encode_flags_ & kEncodeWeights)
        hash = hash << lshift ^ hash >> rshift ^ x->weight.Hash();
      return hash;
    }

   private:
    int32 encode_flags_;
  };

  typedef unordered_map<const Tuple*,
                   Label,
                   TupleKey,
                   TupleEqual> EncodeHash;

  explicit EncodeTable(uint32 encode_flags)
      : flags_(encode_flags),
        encode_hash_(1024, TupleKey(encode_flags)),
        isymbols_(0), osymbols_(0) {}

  ~EncodeTable() {
    for (size_t i = 0; i < encode_tuples_.size(); ++i) {
      delete encode_tuples_[i];
    }
    delete isymbols_;
    delete osymbols_;
  }

  // Given an arc encode either input/ouptut labels or input/costs or both
  Label Encode(const A &arc) {
    const Tuple tuple(arc.ilabel,
                      flags_ & kEncodeLabels ? arc.olabel : 0,
                      flags_ & kEncodeWeights ? arc.weight : Weight::One());
    typename EncodeHash::const_iterator it = encode_hash_.find(&tuple);
    if (it == encode_hash_.end()) {
      encode_tuples_.push_back(new Tuple(tuple));
      encode_hash_[encode_tuples_.back()] = encode_tuples_.size();
      return encode_tuples_.size();
    } else {
      return it->second;
    }
  }

  // Given an arc, look up its encoded label. Returns kNoLabel if not found.
  Label GetLabel(const A &arc) const {
    const Tuple tuple(arc.ilabel,
                      flags_ & kEncodeLabels ? arc.olabel : 0,
                      flags_ & kEncodeWeights ? arc.weight : Weight::One());
    typename EncodeHash::const_iterator it = encode_hash_.find(&tuple);
    if (it == encode_hash_.end()) {
      return kNoLabel;
    } else {
      return it->second;
    }
  }

  // Given an encode arc Label decode back to input/output labels and costs
  const Tuple* Decode(Label key) const {
    if (key < 1 || key > encode_tuples_.size()) {
      LOG(ERROR) << "EncodeTable::Decode: unknown decode key: " << key;
      return 0;
    }
    return encode_tuples_[key - 1];
  }

  size_t Size() const { return encode_tuples_.size(); }

  bool Write(ostream &strm, const string &source) const;

  static EncodeTable<A> *Read(istream &strm, const string &source);

  uint32 flags() const { return flags_ & kEncodeFlags; }

  int RefCount() const { return ref_count_.count(); }
  int IncrRefCount() { return ref_count_.Incr(); }
  int DecrRefCount() { return ref_count_.Decr(); }


  SymbolTable *InputSymbols() const { return isymbols_; }

  SymbolTable *OutputSymbols() const { return osymbols_; }

  void SetInputSymbols(const SymbolTable* syms) {
    if (isymbols_) delete isymbols_;
    if (syms) {
      isymbols_ = syms->Copy();
      flags_ |= kEncodeHasISymbols;
    } else {
      isymbols_ = 0;
      flags_ &= ~kEncodeHasISymbols;
    }
  }

  void SetOutputSymbols(const SymbolTable* syms) {
    if (osymbols_) delete osymbols_;
    if (syms) {
      osymbols_ = syms->Copy();
      flags_ |= kEncodeHasOSymbols;
    } else {
      osymbols_ = 0;
      flags_ &= ~kEncodeHasOSymbols;
    }
  }

 private:
  uint32 flags_;
  vector<Tuple*> encode_tuples_;
  EncodeHash encode_hash_;
  RefCounter ref_count_;
  SymbolTable *isymbols_;       // Pre-encoded ilabel symbol table
  SymbolTable *osymbols_;       // Pre-encoded olabel symbol table

  DISALLOW_COPY_AND_ASSIGN(EncodeTable);
};

template <class A> inline
bool EncodeTable<A>::Write(ostream &strm, const string &source) const {
  WriteType(strm, kEncodeMagicNumber);
  WriteType(strm, flags_);
  int64 size = encode_tuples_.size();
  WriteType(strm, size);
  for (size_t i = 0;  i < size; ++i) {
    const Tuple* tuple = encode_tuples_[i];
    WriteType(strm, tuple->ilabel);
    WriteType(strm, tuple->olabel);
    tuple->weight.Write(strm);
  }

  if (flags_ & kEncodeHasISymbols)
    isymbols_->Write(strm);

  if (flags_ & kEncodeHasOSymbols)
    osymbols_->Write(strm);

  strm.flush();
  if (!strm) {
    LOG(ERROR) << "EncodeTable::Write: write failed: " << source;
    return false;
  }
  return true;
}

template <class A> inline
EncodeTable<A> *EncodeTable<A>::Read(istream &strm, const string &source) {
  int32 magic_number = 0;
  ReadType(strm, &magic_number);
  if (magic_number != kEncodeMagicNumber) {
    LOG(ERROR) << "EncodeTable::Read: Bad encode table header: " << source;
    return 0;
  }
  uint32 flags;
  ReadType(strm, &flags);
  EncodeTable<A> *table = new EncodeTable<A>(flags);

  int64 size;
  ReadType(strm, &size);
  if (!strm) {
    LOG(ERROR) << "EncodeTable::Read: read failed: " << source;
    return 0;
  }

  for (size_t i = 0; i < size; ++i) {
    Tuple* tuple = new Tuple();
    ReadType(strm, &tuple->ilabel);
    ReadType(strm, &tuple->olabel);
    tuple->weight.Read(strm);
    if (!strm) {
      LOG(ERROR) << "EncodeTable::Read: read failed: " << source;
      return 0;
    }
    table->encode_tuples_.push_back(tuple);
    table->encode_hash_[table->encode_tuples_.back()] =
        table->encode_tuples_.size();
  }

  if (flags & kEncodeHasISymbols)
    table->isymbols_ = SymbolTable::Read(strm, source);

  if (flags & kEncodeHasOSymbols)
    table->osymbols_ = SymbolTable::Read(strm, source);

  return table;
}


// A mapper to encode/decode weighted transducers. Encoding of an
// Fst is useful for performing classical determinization or minimization
// on a weighted transducer by treating it as an unweighted acceptor over
// encoded labels.
//
// The Encode mapper stores the encoding in a local hash table (EncodeTable)
// This table is shared (and reference counted) between the encoder and
// decoder. A decoder has read only access to the EncodeTable.
//
// The EncodeMapper allows on the fly encoding of the machine. As the
// EncodeTable is generated the same table may by used to decode the machine
// on the fly. For example in the following sequence of operations
//
//  Encode -> Determinize -> Decode
//
// we will use the encoding table generated during the encode step in the
// decode, even though the encoding is not complete.
//
template <class A> class EncodeMapper {
  typedef typename A::Weight Weight;
  typedef typename A::Label  Label;
 public:
  EncodeMapper(uint32 flags, EncodeType type)
    : flags_(flags),
      type_(type),
      table_(new EncodeTable<A>(flags)),
      error_(false) {}

  EncodeMapper(const EncodeMapper& mapper)
      : flags_(mapper.flags_),
        type_(mapper.type_),
        table_(mapper.table_),
        error_(false) {
    table_->IncrRefCount();
  }

  // Copy constructor but setting the type, typically to DECODE
  EncodeMapper(const EncodeMapper& mapper, EncodeType type)
      : flags_(mapper.flags_),
        type_(type),
        table_(mapper.table_),
        error_(mapper.error_) {
    table_->IncrRefCount();
  }

  ~EncodeMapper() {
    if (!table_->DecrRefCount()) delete table_;
  }

  A operator()(const A &arc);

  MapFinalAction FinalAction() const {
    return (type_ == ENCODE && (flags_ & kEncodeWeights)) ?
                   MAP_REQUIRE_SUPERFINAL : MAP_NO_SUPERFINAL;
  }

  MapSymbolsAction InputSymbolsAction() const { return MAP_CLEAR_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_CLEAR_SYMBOLS;}

  uint64 Properties(uint64 inprops) {
    uint64 outprops = inprops;
    if (error_) outprops |= kError;

    uint64 mask = kFstProperties;
    if (flags_ & kEncodeLabels)
      mask &= kILabelInvariantProperties & kOLabelInvariantProperties;
    if (flags_ & kEncodeWeights)
      mask &= kILabelInvariantProperties & kWeightInvariantProperties &
          (type_ == ENCODE ? kAddSuperFinalProperties :
           kRmSuperFinalProperties);

    return outprops & mask;
  }

  uint32 flags() const { return flags_; }
  EncodeType type() const { return type_; }
  const EncodeTable<A> &table() const { return *table_; }

  bool Write(ostream &strm, const string& source) {
    return table_->Write(strm, source);
  }

  bool Write(const string& filename) {
    ofstream strm(filename.c_str(), ofstream::out | ofstream::binary);
    if (!strm) {
      LOG(ERROR) << "EncodeMap: Can't open file: " << filename;
      return false;
    }
    return Write(strm, filename);
  }

  static EncodeMapper<A> *Read(istream &strm,
                               const string& source,
                               EncodeType type = ENCODE) {
    EncodeTable<A> *table = EncodeTable<A>::Read(strm, source);
    return table ? new EncodeMapper(table->flags(), type, table) : 0;
  }

  static EncodeMapper<A> *Read(const string& filename,
                               EncodeType type = ENCODE) {
    ifstream strm(filename.c_str(), ios_base::in | ios_base::binary);
    if (!strm) {
      LOG(ERROR) << "EncodeMap: Can't open file: " << filename;
      return NULL;
    }
    return Read(strm, filename, type);
  }

  SymbolTable *InputSymbols() const { return table_->InputSymbols(); }

  SymbolTable *OutputSymbols() const { return table_->OutputSymbols(); }

  void SetInputSymbols(const SymbolTable* syms) {
    table_->SetInputSymbols(syms);
  }

  void SetOutputSymbols(const SymbolTable* syms) {
    table_->SetOutputSymbols(syms);
  }

 private:
  uint32 flags_;
  EncodeType type_;
  EncodeTable<A>* table_;
  bool error_;

  explicit EncodeMapper(uint32 flags, EncodeType type, EncodeTable<A> *table)
      : flags_(flags), type_(type), table_(table), error_(false) {}
  void operator=(const EncodeMapper &);  // Disallow.
};

template <class A> inline
A EncodeMapper<A>::operator()(const A &arc) {
  if (type_ == ENCODE) {  // labels and/or weights to single label
    if ((arc.nextstate == kNoStateId && !(flags_ & kEncodeWeights)) ||
        (arc.nextstate == kNoStateId && (flags_ & kEncodeWeights) &&
         arc.weight == Weight::Zero())) {
      return arc;
    } else {
      Label label = table_->Encode(arc);
      return A(label,
               flags_ & kEncodeLabels ? label : arc.olabel,
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
        FSTERROR() <<
            "EncodeMapper: Weight-encoded arc has non-trivial weight";
        error_ = true;
      }
      const typename EncodeTable<A>::Tuple* tuple = table_->Decode(arc.ilabel);
      if (!tuple) {
        FSTERROR() << "EncodeMapper: decode failed";
        error_ = true;
        return A(kNoLabel, kNoLabel, Weight::NoWeight(), arc.nextstate);
      } else {
        return A(tuple->ilabel,
                 flags_ & kEncodeLabels ? tuple->olabel : arc.olabel,
                 flags_ & kEncodeWeights ? tuple->weight : arc.weight,
                 arc.nextstate);
      }
    }
  }
}


// Complexity: O(nstates + narcs)
template<class A> inline
void Encode(MutableFst<A> *fst, EncodeMapper<A>* mapper) {
  mapper->SetInputSymbols(fst->InputSymbols());
  mapper->SetOutputSymbols(fst->OutputSymbols());
  ArcMap(fst, mapper);
}

template<class A> inline
void Decode(MutableFst<A>* fst, const EncodeMapper<A>& mapper) {
  ArcMap(fst, EncodeMapper<A>(mapper, DECODE));
  RmFinalEpsilon(fst);
  fst->SetInputSymbols(mapper.InputSymbols());
  fst->SetOutputSymbols(mapper.OutputSymbols());
}


// On the fly label and/or weight encoding of input Fst
//
// Complexity:
// - Constructor: O(1)
// - Traversal: O(nstates_visited + narcs_visited), assuming constant
//   time to visit an input state or arc.
template <class A>
class EncodeFst : public ArcMapFst<A, A, EncodeMapper<A> > {
 public:
  typedef A Arc;
  typedef EncodeMapper<A> C;
  typedef ArcMapFstImpl< A, A, EncodeMapper<A> > Impl;
  using ImplToFst<Impl>::GetImpl;

  EncodeFst(const Fst<A> &fst, EncodeMapper<A>* encoder)
      : ArcMapFst<A, A, C>(fst, encoder, ArcMapFstOptions()) {
    encoder->SetInputSymbols(fst.InputSymbols());
    encoder->SetOutputSymbols(fst.OutputSymbols());
  }

  EncodeFst(const Fst<A> &fst, const EncodeMapper<A>& encoder)
      : ArcMapFst<A, A, C>(fst, encoder, ArcMapFstOptions()) {}

  // See Fst<>::Copy() for doc.
  EncodeFst(const EncodeFst<A> &fst, bool copy = false)
      : ArcMapFst<A, A, C>(fst, copy) {}

  // Get a copy of this EncodeFst. See Fst<>::Copy() for further doc.
  virtual EncodeFst<A> *Copy(bool safe = false) const {
    if (safe) {
      FSTERROR() << "EncodeFst::Copy(true): not allowed.";
      GetImpl()->SetProperties(kError, kError);
    }
    return new EncodeFst(*this);
  }
};


// On the fly label and/or weight encoding of input Fst
//
// Complexity:
// - Constructor: O(1)
// - Traversal: O(nstates_visited + narcs_visited), assuming constant
//   time to visit an input state or arc.
template <class A>
class DecodeFst : public ArcMapFst<A, A, EncodeMapper<A> > {
 public:
  typedef A Arc;
  typedef EncodeMapper<A> C;
  typedef ArcMapFstImpl< A, A, EncodeMapper<A> > Impl;
  using ImplToFst<Impl>::GetImpl;

  DecodeFst(const Fst<A> &fst, const EncodeMapper<A>& encoder)
      : ArcMapFst<A, A, C>(fst,
                            EncodeMapper<A>(encoder, DECODE),
                            ArcMapFstOptions()) {
    GetImpl()->SetInputSymbols(encoder.InputSymbols());
    GetImpl()->SetOutputSymbols(encoder.OutputSymbols());
  }

  // See Fst<>::Copy() for doc.
  DecodeFst(const DecodeFst<A> &fst, bool safe = false)
      : ArcMapFst<A, A, C>(fst, safe) {}

  // Get a copy of this DecodeFst. See Fst<>::Copy() for further doc.
  virtual DecodeFst<A> *Copy(bool safe = false) const {
    return new DecodeFst(*this, safe);
  }
};


// Specialization for EncodeFst.
template <class A>
class StateIterator< EncodeFst<A> >
    : public StateIterator< ArcMapFst<A, A, EncodeMapper<A> > > {
 public:
  explicit StateIterator(const EncodeFst<A> &fst)
      : StateIterator< ArcMapFst<A, A, EncodeMapper<A> > >(fst) {}
};


// Specialization for EncodeFst.
template <class A>
class ArcIterator< EncodeFst<A> >
    : public ArcIterator< ArcMapFst<A, A, EncodeMapper<A> > > {
 public:
  ArcIterator(const EncodeFst<A> &fst, typename A::StateId s)
      : ArcIterator< ArcMapFst<A, A, EncodeMapper<A> > >(fst, s) {}
};


// Specialization for DecodeFst.
template <class A>
class StateIterator< DecodeFst<A> >
    : public StateIterator< ArcMapFst<A, A, EncodeMapper<A> > > {
 public:
  explicit StateIterator(const DecodeFst<A> &fst)
      : StateIterator< ArcMapFst<A, A, EncodeMapper<A> > >(fst) {}
};


// Specialization for DecodeFst.
template <class A>
class ArcIterator< DecodeFst<A> >
    : public ArcIterator< ArcMapFst<A, A, EncodeMapper<A> > > {
 public:
  ArcIterator(const DecodeFst<A> &fst, typename A::StateId s)
      : ArcIterator< ArcMapFst<A, A, EncodeMapper<A> > >(fst, s) {}
};


// Useful aliases when using StdArc.
typedef EncodeFst<StdArc> StdEncodeFst;

typedef DecodeFst<StdArc> StdDecodeFst;

}  // namespace fst

#endif  // FST_LIB_ENCODE_H__
