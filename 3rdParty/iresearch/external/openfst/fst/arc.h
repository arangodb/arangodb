// arc.h

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
//
// Commonly used Fst arc types.

#ifndef FST_LIB_ARC_H__
#define FST_LIB_ARC_H__

#include <string>


#include <fst/expectation-weight.h>
#include <fst/float-weight.h>
#include <fst/lexicographic-weight.h>
#include <fst/power-weight.h>
#include <fst/product-weight.h>
#include <fst/signed-log-weight.h>
#include <fst/sparse-power-weight.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <fst/string-weight.h>


namespace fst {

template <class W>
class ArcTpl {
 public:
  typedef W Weight;
  typedef int Label;
  typedef int StateId;

  ArcTpl(Label i, Label o, const Weight& w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  ArcTpl() {}

  static const string &Type(void) {
    static const string type =
        (Weight::Type() == "tropical") ? "standard" : Weight::Type();
    return type;
  }

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;
};

typedef ArcTpl<TropicalWeight> StdArc;
typedef ArcTpl<LogWeight> LogArc;
typedef ArcTpl<Log64Weight> Log64Arc;
typedef ArcTpl<SignedLogWeight> SignedLogArc;
typedef ArcTpl<SignedLog64Weight> SignedLog64Arc;
typedef ArcTpl<MinMaxWeight> MinMaxArc;


// Arc with integer labels and state Ids and string weights.
template <StringType S = STRING_LEFT>
class StringArc {
 public:
  typedef int Label;
  typedef StringWeight<int, S> Weight;
  typedef int StateId;

  StringArc(Label i, Label o, Weight w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  StringArc() {}

  static const string &Type() {  // Arc type name
    static const string type =
        S == STRING_LEFT ? "left_standard_string" :
        (S == STRING_RIGHT ? "right_standard_string" :
         "restricted_standard_string");
    return type;
  }

  Label ilabel;       // Transition input label
  Label olabel;       // Transition output label
  Weight weight;      // Transition weight
  StateId nextstate;  // Transition destination state
};


// Arc with label and state Id type the same as template arg and with
// weights over the Gallic semiring w.r.t the output labels and weights of A.
template <class A, GallicType G = GALLIC_LEFT>
struct GallicArc {
  typedef A Arc;
  typedef typename A::Label Label;
  typedef typename A::StateId StateId;
  typedef GallicWeight<Label, typename A::Weight, G> Weight;

  GallicArc() {}

  GallicArc(Label i, Label o, Weight w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  GallicArc(const A &arc)
      : ilabel(arc.ilabel), olabel(arc.ilabel),
        weight(arc.olabel, arc.weight), nextstate(arc.nextstate) {}

  static const string &Type() {  // Arc type name
    static const string type =
        (G == GALLIC_LEFT ? "left_gallic_" :
         (G == GALLIC_RIGHT ? "right_gallic_" :
          (G == GALLIC_RESTRICT ? "restricted_gallic_" :
           (G == GALLIC_MIN ? "min_gallic_" :
            "gallic_")))) + A::Type();
    return type;
  }

  Label ilabel;       // Transition input label
  Label olabel;       // Transition output label
  Weight weight;      // Transition weight
  StateId nextstate;  // Transition destination state
};


// Arc with the reverse of the weight found in its template arg.
template <class A> struct ReverseArc {
  typedef A Arc;
  typedef typename A::Label Label;
  typedef typename A::Weight AWeight;
  typedef typename AWeight::ReverseWeight Weight;
  typedef typename A::StateId StateId;

  ReverseArc(Label i, Label o, Weight w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  ReverseArc() {}

  static const string &Type() {  // Arc type name
    static const string type = "reverse_" + Arc::Type();
    return type;
  }

  Label ilabel;       // Transition input label
  Label olabel;       // Transition output label
  Weight weight;      // Transition weight
  StateId nextstate;  // Transition destination state
};


// Arc with integer labels and state Ids and lexicographic weights.
template<class W1, class W2>
struct LexicographicArc {
  typedef int Label;
  typedef LexicographicWeight<W1, W2> Weight;
  typedef int StateId;

  LexicographicArc(Label i, Label o, Weight w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  LexicographicArc() {}

  static const string &Type() {  // Arc type name
    static const string type = Weight::Type();
    return type;
  }

  Label ilabel;       // Transition input label
  Label olabel;       // Transition output label
  Weight weight;      // Transition weight
  StateId nextstate;  // Transition destination state
};


// Arc with integer labels and state Ids and product weights.
template<class W1, class W2>
struct ProductArc {
  typedef int Label;
  typedef ProductWeight<W1, W2> Weight;
  typedef int StateId;

  ProductArc(Label i, Label o, Weight w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  ProductArc() {}

  static const string &Type() {  // Arc type name
    static const string type = Weight::Type();
    return type;
  }

  Label ilabel;       // Transition input label
  Label olabel;       // Transition output label
  Weight weight;      // Transition weight
  StateId nextstate;  // Transition destination state
};


// Arc with label and state Id type the same as first template arg and with
// weights over the n-th cartesian power of the weight type of the
// template arg.
template <class A, unsigned int n>
struct PowerArc {
  typedef A Arc;
  typedef typename A::Label Label;
  typedef typename A::StateId StateId;
  typedef PowerWeight<typename A::Weight, n> Weight;

  PowerArc() {}

  PowerArc(Label i, Label o, Weight w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  static const string &Type() {  // Arc type name
    static string type;
    if (type.empty()) {
      string power;
      Int64ToStr(n, &power);
      type = A::Type() + "_^" + power;
    }
    return type;
  }

  Label ilabel;       // Transition input label
  Label olabel;       // Transition output label
  Weight weight;      // Transition weight
  StateId nextstate;  // Transition destination state
};


// Arc with label and state Id type the same as first template arg and with
// weights over the arbitrary cartesian power of the weight type.
template <class A, class K = int>
struct SparsePowerArc {
  typedef A Arc;
  typedef typename A::Label Label;
  typedef typename A::StateId StateId;
  typedef SparsePowerWeight<typename A::Weight, K> Weight;

  SparsePowerArc() {}

  SparsePowerArc(Label i, Label o, Weight w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  static const string &Type() {  // Arc type name
    static string type;
    if (type.empty()) { type = A::Type() + "_^n"; }
    if(sizeof(K) != sizeof(uint32)) {
      string size;
      Int64ToStr(8 * sizeof(K), &size);
      type += "_" + size;
    }
    return type;
  }

  Label ilabel;       // Transition input label
  Label olabel;       // Transition output label
  Weight weight;      // Transition weight
  StateId nextstate;  // Transition destination state
};


// Arc with label and state Id type the same as first template arg and with
// expectation weight over the first template arg weight type and the
// second template arg.
template <class A, class X2>
struct ExpectationArc {
  typedef A Arc;
  typedef typename A::Label Label;
  typedef typename A::StateId StateId;
  typedef typename A::Weight X1;
  typedef ExpectationWeight<X1, X2> Weight;

  ExpectationArc() {}

  ExpectationArc(Label i, Label o, Weight w, StateId s)
      : ilabel(i), olabel(o), weight(w), nextstate(s) {}

  static const string &Type() {  // Arc type name
    static string type;
    if (type.empty()) {
      type = "expectation_" + A::Type() + "_" + X2::Type();
    }
    return type;
  }

  Label ilabel;       // Transition input label
  Label olabel;       // Transition output label
  Weight weight;      // Transition weight
  StateId nextstate;  // Transition destination state
};

}  // namespace fst

#endif  // FST_LIB_ARC_H__
