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
// This file contains declarations of classes in the Fst template library.

#ifndef FST_FST_DECL_H_
#define FST_FST_DECL_H_

#include <sys/types.h>

#include <cstdint>
#include <memory>  // for allocator<>

#include <fst/windows_defs.inc>

namespace fst {

// Symbol table and iterator.

class SymbolTable;

class SymbolTableIterator;

// Weight templates and weights.

template <class T>
class FloatWeightTpl;

template <class T>
class TropicalWeightTpl;

template <class T>
class LogWeightTpl;

template <class T>
class MinMaxWeightTpl;

using FloatWeight = FloatWeightTpl<float>;

using TropicalWeight = TropicalWeightTpl<float>;

using LogWeight = LogWeightTpl<float>;

using MinMaxWeight = MinMaxWeightTpl<float>;

// Arc templates and arcs.

template <class Weight>
struct ArcTpl;

using StdArc = ArcTpl<TropicalWeight>;

using LogArc = ArcTpl<LogWeight>;

// Stores.

template <class Element, class U>
class CompactArcStore;

template <class Arc>
class DefaultCacheStore;

// Compactors.

template <class AC, class U, class S = CompactArcStore<typename AC::Element, U>>
class CompactArcCompactor;

// FST templates.

template <class Arc, class Compactor, class CacheStore = DefaultCacheStore<Arc>>
class CompactFst;

// The Unsigned type is used to represent indices into the compact arc array.
template <class Arc, class ArcCompactor, class Unsigned = uint32_t,
          class CompactStore =
              CompactArcStore<typename ArcCompactor::Element, Unsigned>,
          class CacheStore = DefaultCacheStore<Arc>>
using CompactArcFst =
    CompactFst<Arc, CompactArcCompactor<ArcCompactor, Unsigned, CompactStore>,
               CacheStore>;

template <class Arc, class U = uint32_t>
class ConstFst;

template <class Arc, class Weight, class Matcher>
class EditFst;

template <class Arc>
class ExpandedFst;

template <class Arc>
class Fst;

template <class Arc>
class MutableFst;

template <class Arc, class Allocator = std::allocator<Arc>>
class VectorState;

template <class Arc, class State = VectorState<Arc>>
class VectorFst;

template <class Arc, class U = ssize_t>
class DefaultReplaceStateTable;

// On-the-fly operations.

template <class Arc, class Compare>
class ArcSortFst;

template <class Arc>
class ClosureFst;

template <class Arc, class Store = DefaultCacheStore<Arc>>
class ComposeFst;

template <class Arc>
class ConcatFst;

template <class Arc>
class DeterminizeFst;

template <class Arc>
class DifferenceFst;

template <class Arc>
class IntersectFst;

template <class Arc>
class InvertFst;

template <class AArc, class BArc, class Mapper>
class ArcMapFst;

template <class Arc>
class ProjectFst;

template <class AArc, class BArc, class Selector>
class RandGenFst;

template <class Arc>
class RelabelFst;

template <class Arc, class StateTable = DefaultReplaceStateTable<Arc>,
          class Store = DefaultCacheStore<Arc>>
class ReplaceFst;

template <class Arc>
class RmEpsilonFst;

template <class Arc>
class UnionFst;

// Heap.

template <class T, class Compare>
class Heap;

// ArcCompactors.

template <class Arc>
class AcceptorCompactor;

template <class Arc>
class StringCompactor;

template <class Arc>
class UnweightedAcceptorCompactor;

template <class Arc>
class UnweightedCompactor;

template <class Arc>
class WeightedStringCompactor;

// Compact Arc FSTs.

template <class Arc, class U = uint32_t>
using CompactStringFst = CompactArcFst<Arc, StringCompactor<Arc>, U>;

template <class Arc, class U = uint32_t>
using CompactWeightedStringFst =
    CompactArcFst<Arc, WeightedStringCompactor<Arc>, U>;

template <class Arc, class U = uint32_t>
using CompactAcceptorFst = CompactArcFst<Arc, AcceptorCompactor<Arc>, U>;

template <class Arc, class U = uint32_t>
using CompactUnweightedFst = CompactArcFst<Arc, UnweightedCompactor<Arc>, U>;

template <class Arc, class U = uint32_t>
using CompactUnweightedAcceptorFst =
    CompactArcFst<Arc, UnweightedAcceptorCompactor<Arc>, U>;

// StdArc aliases for FSTs.

using StdConstFst = ConstFst<StdArc>;
using StdExpandedFst = ExpandedFst<StdArc>;
using StdFst = Fst<StdArc>;
using StdMutableFst = MutableFst<StdArc>;
using StdVectorFst = VectorFst<StdArc>;

// StdArc aliases for on-the-fly operations.

template <class Compare>
using StdArcSortFst = ArcSortFst<StdArc, Compare>;

using StdClosureFst = ClosureFst<StdArc>;

using StdComposeFst = ComposeFst<StdArc>;

using StdConcatFst = ConcatFst<StdArc>;

using StdDeterminizeFst = DeterminizeFst<StdArc>;

using StdDifferenceFst = DifferenceFst<StdArc>;

using StdIntersectFst = IntersectFst<StdArc>;

using StdInvertFst = InvertFst<StdArc>;

using StdProjectFst = ProjectFst<StdArc>;

using StdRelabelFst = RelabelFst<StdArc>;

using StdReplaceFst = ReplaceFst<StdArc>;

using StdRmEpsilonFst = RmEpsilonFst<StdArc>;

using StdUnionFst = UnionFst<StdArc>;

// Filter states.

template <class T>
class IntegerFilterState;

using CharFilterState = IntegerFilterState<signed char>;

using ShortFilterState = IntegerFilterState<short>;  // NOLINT

using IntFilterState = IntegerFilterState<int>;

// Matchers and filters.

template <class FST>
class Matcher;

template <class Matcher1, class Matcher2 = Matcher1>
class NullComposeFilter;

template <class Matcher1, class Matcher2 = Matcher1>
class TrivialComposeFilter;

template <class Matcher1, class Matcher2 = Matcher1>
class SequenceComposeFilter;

template <class Matcher1, class Matcher2 = Matcher1>
class AltSequenceComposeFilter;

template <class Matcher1, class Matcher2 = Matcher1>
class MatchComposeFilter;

template <class Matcher1, class Matcher2 = Matcher1>
class NoMatchComposeFilter;

}  // namespace fst

#endif  // FST_FST_DECL_H_
