// weight.h

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
// General weight set and associated semiring operation definitions.
//
// A semiring is specified by two binary operations Plus and Times and
// two designated elements Zero and One with the following properties:
//   Plus: associative, commutative, and has Zero as its identity.
//   Times: associative and has identity One, distributes w.r.t. Plus, and
//     has Zero as an annihilator:
//          Times(Zero(), a) == Times(a, Zero()) = Zero().
//
//  A left semiring distributes on the left; a right semiring is
//  similarly defined.
//
// A Weight class must have binary functions =Plus= and =Times= and
// static member functions =Zero()= and =One()= and these must form
// (at least) a left or right semiring.
//
// In addition, the following should be defined for a Weight:
//   Member: predicate on set membership.
//   NoWeight: static member function that returns an element that is
//      not a set member; used to signal an error.
//   >>: reads textual representation of a weight.
//   <<: prints textual representation of a weight.
//   Read(istream &strm): reads binary representation of a weight.
//   Write(ostream &strm): writes binary representation of a weight.
//   Hash: maps weight to size_t.
//   ApproxEqual: approximate equality (for inexact weights)
//   Quantize: quantizes wrt delta (for inexact weights)
//   Divide: for all a,b,c s.t. Times(a, b) == c
//     --> b' = Divide(c, a, DIVIDE_LEFT) if a left semiring, b'.Member()
//      and Times(a, b') == c
//     --> a' = Divide(c, b, DIVIDE_RIGHT) if a right semiring, a'.Member()
//      and Times(a', b) == c
//     --> b' = Divide(c, a) = Divide(c, a, DIVIDE_ANY) =
//      Divide(c, a, DIVIDE_LEFT) = Divide(c, a, DIVIDE_RIGHT) if a
//      commutative semiring, b'.Member() and Times(a, b') = Times(b', a) = c
//   ReverseWeight: the type of the corresponding reverse weight.
//     Typically the same type as Weight for a (both left and right) semiring.
//     For the left string semiring, it is the right string semiring.
//   Reverse: a mapping from Weight to ReverseWeight s.t.
//     --> Reverse(Reverse(a)) = a
//     --> Reverse(Plus(a, b)) = Plus(Reverse(a), Reverse(b))
//     --> Reverse(Times(a, b)) = Times(Reverse(b), Reverse(a))
//     Typically the identity mapping in a (both left and right) semiring.
//     In the left string semiring, it maps to the reverse string
//     in the right string semiring.
//   Properties: specifies additional properties that hold:
//      LeftSemiring: indicates weights form a left semiring.
//      RightSemiring: indicates weights form a right semiring.
//      Commutative: for all a,b: Times(a,b) == Times(b,a)
//      Idempotent: for all a: Plus(a, a) == a.
//      Path: for all a, b: Plus(a, b) == a or Plus(a, b) == b.


#ifndef FST_LIB_WEIGHT_H__
#define FST_LIB_WEIGHT_H__

#include <cmath>
#include <cctype>
#include <iostream>
#include <sstream>

#include <fst/compat.h>

#include <fst/util.h>


DECLARE_string(fst_weight_parentheses);
DECLARE_string(fst_weight_separator);

namespace fst {

//
// CONSTANT DEFINITIONS
//

// A representable float near .001
const float kDelta =                   1.0F/1024.0F;

// For all a,b,c: Times(c, Plus(a,b)) = Plus(Times(c,a), Times(c, b))
const uint64 kLeftSemiring =           0x0000000000000001ULL;

// For all a,b,c: Times(Plus(a,b), c) = Plus(Times(a,c), Times(b, c))
const uint64 kRightSemiring =          0x0000000000000002ULL;

const uint64 kSemiring = kLeftSemiring | kRightSemiring;

// For all a,b: Times(a,b) = Times(b,a)
const uint64 kCommutative =       0x0000000000000004ULL;

// For all a: Plus(a, a) = a
const uint64 kIdempotent =             0x0000000000000008ULL;

// For all a,b: Plus(a,b) = a or Plus(a,b) = b
const uint64 kPath =                   0x0000000000000010ULL;


// Determines direction of division.
enum DivideType { DIVIDE_LEFT,   // left division
                  DIVIDE_RIGHT,  // right division
                  DIVIDE_ANY };  // division in a commutative semiring

// NATURAL ORDER
//
// By definition:
//                 a <= b iff a + b = a
// The natural order is a negative partial order iff the semiring is
// idempotent. It is trivially monotonic for plus. It is left
// (resp. right) monotonic for times iff the semiring is left
// (resp. right) distributive. It is a total order iff the semiring
// has the path property. See Mohri, "Semiring Framework and
// Algorithms for Shortest-Distance Problems", Journal of Automata,
// Languages and Combinatorics 7(3):321-350, 2002. We define the
// strict version of this order below.

template <class W>
class NaturalLess {
 public:
  typedef W Weight;

  NaturalLess() {
    if (!(W::Properties() & kIdempotent)) {
      FSTERROR() << "NaturalLess: Weight type is not idempotent: "
                 << W::Type();
    }
  }

  bool operator()(const W &w1, const W &w2) const {
    return (Plus(w1, w2) == w1) && w1 != w2;
  }
};

// Power is the iterated product for arbitrary semirings such that
// Power(w, 0) is One() for the semiring, and
// Power(w, n) = Times(Power(w, n-1), w)

template <class W>
W Power(W w, size_t n) {
  W result = W::One();
  for (size_t i = 0; i < n; ++i) {
    result = Times(result, w);
  }
  return result;
}

// General weight converter - raises error.
template <class W1, class W2>
struct WeightConvert {
  W2 operator()(W1 w1) const {
    FSTERROR() << "WeightConvert: can't convert weight from \""
               << W1::Type() << "\" to \"" << W2::Type();
    return W2::NoWeight();
  }
};

// Specialized weight converter to self.
template <class W>
struct WeightConvert<W, W> {
  W operator()(W w) const { return w; }
};

// Helper class for writing textual composite weights.
class CompositeWeightWriter {
 public:
  explicit CompositeWeightWriter(ostream &strm)  // NOLINT
  : strm_(strm), i_(0) {
    if (FLAGS_fst_weight_separator.size() != 1) {
      FSTERROR() << "CompositeWeightWriter: "
                 << "FLAGS_fst_weight_separator.size() is not equal to 1";
      strm.clear(std::ios::badbit);
      return;
    }
    if (!FLAGS_fst_weight_parentheses.empty()) {
      if (FLAGS_fst_weight_parentheses.size() != 2) {
        FSTERROR() << "CompositeWeightWriter: "
                   << "FLAGS_fst_weight_parentheses.size() is not equal to 2";
        strm.clear(std::ios::badbit);
        return;
      }
    }
  }

  // Writes open parenthesis to a stream if option selected.
  void WriteBegin() {
    if (!FLAGS_fst_weight_parentheses.empty())
      strm_ << FLAGS_fst_weight_parentheses[0];
  }

  // Writes element to a stream
  template <class T>
  void WriteElement(const T &comp) {
    if (i_++ > 0)
      strm_ << FLAGS_fst_weight_separator[0];
    strm_ << comp;
  }

  // Writes close parenthesis to a stream if option selected.
  void WriteEnd() {
    if (!FLAGS_fst_weight_parentheses.empty())
      strm_ << FLAGS_fst_weight_parentheses[1];
  }

 private:
  ostream &strm_;
  int i_;           // Element position.
  DISALLOW_COPY_AND_ASSIGN(CompositeWeightWriter);
};

// Helper class for reading textual composite weights. Elements are
// separated by FLAGS_fst_weight_separator. There must be at least one
// element per textual representation.  FLAGS_fst_weight_parentheses should
// be set if the composite weights themselves contain composite
// weights to ensure proper parsing.
class CompositeWeightReader {
 public:
  explicit CompositeWeightReader(istream &strm)  // NOLINT
      : strm_(strm), c_(0), has_parens_(false),
        depth_(0), open_paren_(0), close_paren_(0) {
    if (FLAGS_fst_weight_separator.size() != 1) {
      FSTERROR() << "ComposeWeightReader: "
                 << "FLAGS_fst_weight_separator.size() is not equal to 1";
      strm_.clear(std::ios::badbit);
      return;
    }
    separator_ = FLAGS_fst_weight_separator[0];
    if (!FLAGS_fst_weight_parentheses.empty()) {
      if (FLAGS_fst_weight_parentheses.size() != 2) {
        FSTERROR() << "ComposeWeightReader: "
                   << "FLAGS_fst_weight_parentheses.size() is not equal to 2";
        strm_.clear(std::ios::badbit);
        return;
      }
      has_parens_ = true;
      open_paren_ = FLAGS_fst_weight_parentheses[0];
      close_paren_ = FLAGS_fst_weight_parentheses[1];
    }
  }

  // Reads open parenthesis from a stream if option selected.
  void ReadBegin() {
    do {  // Skips white space
      c_ = strm_.get();
    } while (std::isspace(c_));

    if (has_parens_) {
      if (c_ != open_paren_) {
        FSTERROR() << "CompositeWeightReader: open paren missing: "
                   << "fst_weight_parentheses flag set correcty?";
        strm_.clear(std::ios::badbit);
        return;
      }
      ++depth_;
      c_ = strm_.get();
    }
  }

  // Reads element from a stream. Argument 'last' optionlly
  // indicates this will be the last element (allowing more
  // forgiving formatting of the last elemeng). Returns false
  // when last element is read.
  template <class T> bool ReadElement(T *comp, bool last = false);

  // Finalizes read
  void ReadEnd() {
    if (c_ != EOF && !std::isspace(c_)) {
      FSTERROR() << "CompositeWeightReader: excess character: '"
                 << static_cast<char>(c_)
                 << "': fst_weight_parentheses flag set correcty?";
      strm_.clear(std::ios::badbit);
    }
  }

 private:
  istream &strm_;      // input stream
  int c_;              // last character read
  char separator_;
  bool has_parens_;
  int depth_;          // paren depth
  char open_paren_;
  char close_paren_;
  DISALLOW_COPY_AND_ASSIGN(CompositeWeightReader);
};

template <class T>
inline bool CompositeWeightReader::ReadElement(T *comp, bool last) {
  string s;
  while ((c_ != EOF) && !std::isspace(c_) &&
         (c_ != separator_ || depth_ > 1 || last) &&
         (c_ != close_paren_ || depth_ != 1)) {
    s += c_;
    // If parens encountered before separator, they must be matched
    if (has_parens_ && c_ == open_paren_) {
      ++depth_;
    } else if (has_parens_ && c_ == close_paren_) {
      // Fail for unmatched parens
      if (depth_ == 0) {
        FSTERROR() << "CompositeWeightReader: unmatched close paren: "
                   << "fst_weight_parentheses flag set correcty?";
        strm_.clear(std::ios::badbit);
        return false;
      }
      --depth_;
    }
    c_ = strm_.get();
  }

  if (s.empty()) {
    FSTERROR() << "CompositeWeightReader: empty element: "
               << "fst_weight_parentheses flag set correcty?";
    strm_.clear(std::ios::badbit);
    return false;
  }
  istringstream strm(s);
  strm >> *comp;

  // Skips separator/close parenthesis
  if (c_ != EOF && !std::isspace(c_))
    c_ = strm_.get();

  // Clears fail bit if just EOF
  if (c_ == EOF && !strm_.bad())
    strm_.clear(std::ios::eofbit);

  return c_ != EOF && !std::isspace(c_);
}

}  // namespace fst

#endif  // FST_LIB_WEIGHT_H__
