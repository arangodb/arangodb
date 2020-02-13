// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Commonly used FST arc types.

#ifndef FST_ARC_H_
#define FST_ARC_H_

#include <climits>
#include <string>
#include <type_traits>
#include <utility>


#include <fst/expectation-weight.h>
#include <fst/float-weight.h>
#include <fst/lexicographic-weight.h>
#include <fst/power-weight.h>
#include <fst/product-weight.h>
#include <fst/signed-log-weight.h>
#include <fst/sparse-power-weight.h>
#include <fst/string-weight.h>


namespace fst {

template <class W>
struct ArcTpl {
 public:
  using Weight = W;
  using Label = int;
  using StateId = int;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  ArcTpl() noexcept(std::is_nothrow_default_constructible<Weight>::value) {}

  template <class T>
  ArcTpl(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  ArcTpl(Label ilabel, Label olabel, StateId nextstate)
      : ArcTpl(ilabel, olabel, Weight::One(), nextstate) {}

  static const std::string &Type() {
    static const auto *const type = new std::string(
        Weight::Type() == "tropical" ? "standard" : Weight::Type());
    return *type;
  }
};

using StdArc = ArcTpl<TropicalWeight>;
using LogArc = ArcTpl<LogWeight>;
using Log64Arc = ArcTpl<Log64Weight>;
using SignedLogArc = ArcTpl<SignedLogWeight>;
using SignedLog64Arc = ArcTpl<SignedLog64Weight>;
using MinMaxArc = ArcTpl<MinMaxWeight>;

// Arc with integer labels and state IDs and string weights.
template <StringType S = STRING_LEFT>
struct StringArc {
 public:
  using Label = int;
  using Weight = StringWeight<int, S>;
  using StateId = int;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  StringArc() = default;

  template <class T>
  StringArc(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  StringArc(Label ilabel, Label olabel, StateId nextstate)
      : StringArc(ilabel, olabel, Weight::One(), nextstate) {}

  static const std::string &Type() {
    static const auto *const type = new std::string(
        S == STRING_LEFT ? "left_standard_string"
                         : (S == STRING_RIGHT ? "right_standard_string"
                                              : "restricted_standard_string"));
    return *type;
  }
};

// Arc with label and state Id type the same as template arg and with
// weights over the Gallic semiring w.r.t the output labels and weights of A.
template <class A, GallicType G = GALLIC_LEFT>
struct GallicArc {
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = GallicWeight<Label, typename Arc::Weight, G>;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  GallicArc() = default;

  template <class T>
  GallicArc(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  GallicArc(Label ilabel, Label olabel, StateId nextstate)
      : GallicArc(ilabel, olabel, Weight::One(), nextstate) {}

  explicit GallicArc(const Arc &arc)
      : ilabel(arc.ilabel),
        olabel(arc.ilabel),
        weight(arc.olabel, arc.weight),
        nextstate(arc.nextstate) {}

  static const std::string &Type() {
    static const auto *const type = new std::string(
        (G == GALLIC_LEFT
             ? "left_gallic_"
             : (G == GALLIC_RIGHT
                    ? "right_gallic_"
                    : (G == GALLIC_RESTRICT
                           ? "restricted_gallic_"
                           : (G == GALLIC_MIN ? "min_gallic_" : "gallic_")))) +
        Arc::Type());
    return *type;
  }
};

// Arc with the reverse of the weight found in its template arg.
template <class A>
struct ReverseArc {
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight::ReverseWeight;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  ReverseArc() = default;

  template <class T>
  ReverseArc(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  ReverseArc(Label ilabel, Label olabel, StateId nextstate)
      : ReverseArc(ilabel, olabel, Weight::One(), nextstate) {}

  static const std::string &Type() {
    static const auto *const type = new std::string("reverse_" + Arc::Type());
    return *type;
  }
};

// Arc with integer labels and state IDs and lexicographic weights.
template <class Weight1, class Weight2>
struct LexicographicArc {
  using Label = int;
  using StateId = int;
  using Weight = LexicographicWeight<Weight1, Weight2>;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  LexicographicArc() = default;

  template <class T>
  LexicographicArc(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  LexicographicArc(Label ilabel, Label olabel, StateId nextstate)
      : LexicographicArc(ilabel, olabel, Weight::One(), nextstate) {}

  static const std::string &Type() {
    static const std::string *const type = new std::string(Weight::Type());
    return *type;
  }
};

// Arc with integer labels and state IDs and product weights.
template <class Weight1, class Weight2>
struct ProductArc {
  using Label = int;
  using StateId = int;
  using Weight = ProductWeight<Weight1, Weight2>;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  ProductArc() = default;

  template <class T>
  ProductArc(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  ProductArc(Label ilabel, Label olabel, StateId nextstate)
      : ProductArc(ilabel, olabel, Weight::One(), nextstate) {}

  static const std::string &Type() {
    static const auto *const type = new std::string(Weight::Type());
    return *type;
  }
};

// Arc with label and state ID type the same as first template argument and with
// weights over the n-th Cartesian power of the weight type of the template
// argument.
template <class A, size_t n>
struct PowerArc {
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = PowerWeight<typename Arc::Weight, n>;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  PowerArc() = default;

  template <class T>
  PowerArc(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  PowerArc(Label ilabel, Label olabel, StateId nextstate)
      : PowerArc(ilabel, olabel, Weight::One(), nextstate) {}

  static const std::string &Type() {
    static const auto *const type =
        new std::string(Arc::Type() + "_^" + std::to_string(n));
    return *type;
  }
};

// Arc with label and state ID type the same as first template argument and with
// weights over the arbitrary Cartesian power of the weight type.
template <class A, class K = int>
struct SparsePowerArc {
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::Label;
  using Weight = SparsePowerWeight<typename Arc::Weight, K>;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  SparsePowerArc() = default;

  template <class T>
  SparsePowerArc(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  SparsePowerArc(Label ilabel, Label olabel, StateId nextstate)
      : SparsePowerArc(ilabel, olabel, Weight::One(), nextstate) {}

  static const std::string &Type() {
    static const std::string *const type = [] {
      std::string type = Arc::Type() + "_^n";
      if (sizeof(K) != sizeof(uint32)) {
        type += "_" + std::to_string(CHAR_BIT * sizeof(K));
      }
      return new std::string(type);
    }();
    return *type;
  }
};

// Arc with label and state ID type the same as first template argument and with
// expectation weight over the first template argument's weight type and the
// second template argument.
template <class A, class X2>
struct ExpectationArc {
  using Arc = A;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using X1 = typename Arc::Weight;
  using Weight = ExpectationWeight<X1, X2>;

  Label ilabel;
  Label olabel;
  Weight weight;
  StateId nextstate;

  ExpectationArc() = default;

  template <class T>
  ExpectationArc(Label ilabel, Label olabel, T &&weight, StateId nextstate)
      : ilabel(ilabel),
        olabel(olabel),
        weight(std::forward<T>(weight)),
        nextstate(nextstate) {}

  // Arc with weight One.
  ExpectationArc(Label ilabel, Label olabel, StateId nextstate)
      : ExpectationArc(ilabel, olabel, Weight::One(), nextstate) {}

  static const std::string &Type() {
    static const auto *const type =
        new std::string("expectation_" + Arc::Type() + "_" + X2::Type());
    return *type;
  }
};

}  // namespace fst

#endif  // FST_ARC_H_
