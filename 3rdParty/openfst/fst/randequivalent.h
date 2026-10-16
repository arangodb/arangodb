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
// Tests if two FSTS are equivalent by checking if random strings from one FST
// are transduced the same by both FSTs.

#ifndef FST_RANDEQUIVALENT_H_
#define FST_RANDEQUIVALENT_H_

#include <cstdint>
#include <random>

#include <fst/log.h>

#include <fst/arcsort.h>
#include <fst/compose.h>
#include <fst/project.h>
#include <fst/randgen.h>
#include <fst/shortest-distance.h>
#include <fst/vector-fst.h>
#include <fst/weight.h>


namespace fst {

// Test if two FSTs are stochastically equivalent by randomly generating
// random paths through the FSTs.
//
// For each randomly generated path, the algorithm computes for each
// of the two FSTs the sum of the weights of all the successful paths
// sharing the same input and output labels as the considered randomly
// generated path and checks that these two values are within a user-specified
// delta. Returns optional error value (when FST_FLAGS_error_fatal = false).
template <class Arc, class ArcSelector>
bool RandEquivalent(const Fst<Arc> &fst1, const Fst<Arc> &fst2, int32_t npath,
                    const RandGenOptions<ArcSelector> &opts,
                    float delta = kDelta,
                    uint64_t seed = std::random_device()(),
                    bool *error = nullptr) {
  using Weight = typename Arc::Weight;
  if (error) *error = false;
  // Checks that the symbol table are compatible.
  if (!CompatSymbols(fst1.InputSymbols(), fst2.InputSymbols()) ||
      !CompatSymbols(fst1.OutputSymbols(), fst2.OutputSymbols())) {
    FSTERROR() << "RandEquivalent: Input/output symbol tables of 1st "
               << "argument do not match input/output symbol tables of 2nd "
               << "argument";
    if (error) *error = true;
    return false;
  }
  static const ILabelCompare<Arc> icomp;
  static const OLabelCompare<Arc> ocomp;
  VectorFst<Arc> sfst1(fst1);
  VectorFst<Arc> sfst2(fst2);
  Connect(&sfst1);
  Connect(&sfst2);
  ArcSort(&sfst1, icomp);
  ArcSort(&sfst2, icomp);
  bool result = true;
  std::mt19937 rand(seed);
  std::bernoulli_distribution coin(.5);
  for (int32_t n = 0; n < npath; ++n) {
    VectorFst<Arc> path;
    const auto &fst = coin(rand) ? sfst1 : sfst2;
    RandGen(fst, &path, opts);
    VectorFst<Arc> ipath(path);
    VectorFst<Arc> opath(path);
    Project(&ipath, ProjectType::INPUT);
    Project(&opath, ProjectType::OUTPUT);
    VectorFst<Arc> cfst1, pfst1;
    Compose(ipath, sfst1, &cfst1);
    ArcSort(&cfst1, ocomp);
    Compose(cfst1, opath, &pfst1);
    // Gives up if there are epsilon cycles in a non-idempotent semiring.
    if (!IsIdempotent<Weight>::value && pfst1.Properties(kCyclic, true)) {
      continue;
    }
    const auto sum1 = ShortestDistance(pfst1);
    VectorFst<Arc> cfst2;
    Compose(ipath, sfst2, &cfst2);
    ArcSort(&cfst2, ocomp);
    VectorFst<Arc> pfst2;
    Compose(cfst2, opath, &pfst2);
    // Gives up if there are epsilon cycles in a non-idempotent semiring.
    if (!IsIdempotent<Weight>::value && pfst2.Properties(kCyclic, true)) {
      continue;
    }
    const auto sum2 = ShortestDistance(pfst2);
    if (!ApproxEqual(sum1, sum2, delta)) {
      VLOG(1) << "Sum1 = " << sum1;
      VLOG(1) << "Sum2 = " << sum2;
      result = false;
      break;
    }
  }
  if (fst1.Properties(kError, false) || fst2.Properties(kError, false)) {
    if (error) *error = true;
    return false;
  }
  return result;
}

// Tests if two FSTs are equivalent by randomly generating a nnpath paths
// (no longer than the path_length) using a user-specified seed, optionally
// indicating an error setting an optional error argument to true.
template <class Arc>
bool RandEquivalent(const Fst<Arc> &fst1, const Fst<Arc> &fst2, int32_t npath,
                    float delta = kDelta,
                    uint64_t seed = std::random_device()(),
                    int32_t max_length = std::numeric_limits<int32_t>::max(),
                    bool *error = nullptr) {
  const UniformArcSelector<Arc> uniform_selector(seed);
  const RandGenOptions<UniformArcSelector<Arc>> opts(uniform_selector,
                                                     max_length);
  return RandEquivalent(fst1, fst2, npath, opts, delta, seed, error);
}

}  // namespace fst

#endif  // FST_RANDEQUIVALENT_H_
