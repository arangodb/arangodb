// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Function to test equality of two FSTs.

#ifndef FST_EQUAL_H_
#define FST_EQUAL_H_

#include <fst/log.h>

#include <fst/fst.h>
#include <fst/test-properties.h>


namespace fst {

constexpr uint32 kEqualFsts = 0x0001;
constexpr uint32 kEqualFstTypes = 0x0002;
constexpr uint32 kEqualCompatProperties = 0x0004;
constexpr uint32 kEqualCompatSymbols = 0x0008;
constexpr uint32 kEqualAll =
    kEqualFsts | kEqualFstTypes | kEqualCompatProperties | kEqualCompatSymbols;

class WeightApproxEqual {
 public:
  explicit WeightApproxEqual(float delta) : delta_(delta) {}

  template <class Weight>
  bool operator()(const Weight &w1, const Weight &w2) const {
    return ApproxEqual(w1, w2, delta_);
  }

 private:
  float delta_;
};

// Tests if two Fsts have the same states and arcs in the same order (when
// etype & kEqualFst).
// Also optional checks equality of Fst types (etype & kEqualFstTypes) and
// compatibility of stored properties (etype & kEqualCompatProperties) and
// of symbol tables (etype & kEqualCompatSymbols).
template <class Arc, class WeightEqual>
bool Equal(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
           WeightEqual weight_equal, uint32 etype = kEqualFsts) {
  if ((etype & kEqualFstTypes) && (fst1.Type() != fst2.Type())) {
    VLOG(1) << "Equal: Mismatched FST types (" << fst1.Type() << " != "
            << fst2.Type() << ")";
    return false;
  }
  if ((etype & kEqualCompatProperties) &&
      !CompatProperties(fst1.Properties(kCopyProperties, false),
                        fst2.Properties(kCopyProperties, false))) {
    VLOG(1) << "Equal: Properties not compatible";
    return false;
  }
  if (etype & kEqualCompatSymbols) {
    if (!CompatSymbols(fst1.InputSymbols(), fst2.InputSymbols(), false)) {
      VLOG(1) << "Equal: Input symbols not compatible";
      return false;
    }
    if (!CompatSymbols(fst1.OutputSymbols(), fst2.OutputSymbols(), false)) {
      VLOG(1) << "Equal: Output symbols not compatible";
      return false;
    }
  }
  if (!(etype & kEqualFsts)) return true;
  if (fst1.Start() != fst2.Start()) {
    VLOG(1) << "Equal: Mismatched start states (" << fst1.Start() << " != "
            << fst2.Start() << ")";
    return false;
  }
  StateIterator<Fst<Arc>> siter1(fst1);
  StateIterator<Fst<Arc>> siter2(fst2);
  while (!siter1.Done() || !siter2.Done()) {
    if (siter1.Done() || siter2.Done()) {
      VLOG(1) << "Equal: Mismatched number of states";
      return false;
    }
    const auto s1 = siter1.Value();
    const auto s2 = siter2.Value();
    if (s1 != s2) {
      VLOG(1) << "Equal: Mismatched states (" << s1 << "!= "
              << s2 << ")";
      return false;
    }
    const auto &final1 = fst1.Final(s1);
    const auto &final2 = fst2.Final(s2);
    if (!weight_equal(final1, final2)) {
      VLOG(1) << "Equal: Mismatched final weights at state " << s1
              << " (" << final1 << " != " << final2 << ")";
      return false;
    }
    ArcIterator<Fst<Arc>> aiter1(fst1, s1);
    ArcIterator<Fst<Arc>> aiter2(fst2, s2);
    for (auto a = 0; !aiter1.Done() || !aiter2.Done(); ++a) {
      if (aiter1.Done() || aiter2.Done()) {
        VLOG(1) << "Equal: Mismatched number of arcs at state " << s1;
        return false;
      }
      const auto &arc1 = aiter1.Value();
      const auto &arc2 = aiter2.Value();
      if (arc1.ilabel != arc2.ilabel) {
        VLOG(1) << "Equal: Mismatched arc input labels at state " << s1
                << ", arc " << a << " (" << arc1.ilabel << " != "
                << arc2.ilabel << ")";
        return false;
      } else if (arc1.olabel != arc2.olabel) {
        VLOG(1) << "Equal: Mismatched arc output labels at state " << s1
                << ", arc " << a << " (" << arc1.olabel << " != "
                << arc2.olabel << ")";
        return false;
      } else if (!weight_equal(arc1.weight, arc2.weight)) {
        VLOG(1) << "Equal: Mismatched arc weights at state " << s1
                << ", arc " << a << " (" << arc1.weight << " != "
                << arc2.weight << ")";
        return false;
      } else if (arc1.nextstate != arc2.nextstate) {
        VLOG(1) << "Equal: Mismatched next state at state " << s1
                << ", arc " << a << " (" << arc1.nextstate << " != "
                << arc2.nextstate << ")";
        return false;
      }
      aiter1.Next();
      aiter2.Next();
    }
    // Sanity checks: should never fail.
    if (fst1.NumArcs(s1) != fst2.NumArcs(s2)) {
      FSTERROR() << "Equal: Inconsistent arc counts at state " << s1
                 << " (" << fst1.NumArcs(s1) << " != "
                 << fst2.NumArcs(s2) << ")";
      return false;
    }
    if (fst1.NumInputEpsilons(s1) != fst2.NumInputEpsilons(s2)) {
      FSTERROR() << "Equal: Inconsistent input epsilon counts at state " << s1
                 << " (" << fst1.NumInputEpsilons(s1) << " != "
                 << fst2.NumInputEpsilons(s2) << ")";
      return false;
    }
    if (fst1.NumOutputEpsilons(s1) != fst2.NumOutputEpsilons(s2)) {
      FSTERROR() << "Equal: Inconsistent output epsilon counts at state " << s1
                 << " (" << fst1.NumOutputEpsilons(s1) << " != "
                 << fst2.NumOutputEpsilons(s2) << ")";
    }
    siter1.Next();
    siter2.Next();
  }
  return true;
}

template <class Arc>
bool Equal(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
           float delta = kDelta, uint32 etype = kEqualFsts) {
  return Equal(fst1, fst2, WeightApproxEqual(delta), etype);
}

// Support double deltas without forcing all clients to cast to float.
// Without this overload, Equal<Arc, WeightEqual=double> will be chosen,
// since it is a better match than double -> float narrowing, but
// the instantiation will fail.
template <class Arc>
bool Equal(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
           double delta, uint32 etype = kEqualFsts) {
  return Equal(fst1, fst2, WeightApproxEqual(static_cast<float>(delta)), etype);
}


}  // namespace fst

#endif  // FST_EQUAL_H_
