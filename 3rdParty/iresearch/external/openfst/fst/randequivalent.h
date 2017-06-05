// randequivalent.h

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
// Author: allauzen@google.com (Cyril Allauzen)
//
// \file
// Tests if two FSTS are equivalent by checking if random
// strings from one FST are transduced the same by both FSTs.

#ifndef FST_LIB_RANDEQUIVALENT_H_
#define FST_LIB_RANDEQUIVALENT_H_

#include <fst/arcsort.h>
#include <fst/compose.h>
#include <fst/project.h>
#include <fst/randgen.h>
#include <fst/shortest-distance.h>
#include <fst/vector-fst.h>


namespace fst {

// Test if two FSTs are equivalent by randomly generating 'num_paths'
// paths (as specified by the RandGenOptions 'opts') in these FSTs.
//
// For each randomly generated path, the algorithm computes for each
// of the two FSTs the sum of the weights of all the successful paths
// sharing the same input and output labels as the considered randomly
// generated path and checks that these two values are within
// 'delta'. Returns optional error value (when FLAGS_error_fatal = false).
template<class Arc, class ArcSelector>
bool RandEquivalent(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
                    ssize_t num_paths, float delta,
                    const RandGenOptions<ArcSelector> &opts,
                    bool *error = 0) {
  typedef typename Arc::Weight Weight;
  if (error) *error = false;

  // Check that the symbol table are compatible
  if (!CompatSymbols(fst1.InputSymbols(), fst2.InputSymbols()) ||
      !CompatSymbols(fst1.OutputSymbols(), fst2.OutputSymbols())) {
    FSTERROR() << "RandEquivalent: input/output symbol tables of 1st "
               << "argument do not match input/output symbol tables of 2nd "
               << "argument";
    if (error) *error = true;
    return false;
  }

  ILabelCompare<Arc> icomp;
  OLabelCompare<Arc> ocomp;
  VectorFst<Arc> sfst1(fst1);
  VectorFst<Arc> sfst2(fst2);
  Connect(&sfst1);
  Connect(&sfst2);
  ArcSort(&sfst1, icomp);
  ArcSort(&sfst2, icomp);

  bool ret = true;
  for (ssize_t n = 0; n < num_paths; ++n) {
    VectorFst<Arc> path;
    const Fst<Arc> &fst = rand() % 2 ? sfst1 : sfst2;
    RandGen(fst, &path, opts);

    VectorFst<Arc> ipath(path);
    VectorFst<Arc> opath(path);
    Project(&ipath, PROJECT_INPUT);
    Project(&opath, PROJECT_OUTPUT);

    VectorFst<Arc> cfst1, pfst1;
    Compose(ipath, sfst1, &cfst1);
    ArcSort(&cfst1, ocomp);
    Compose(cfst1, opath, &pfst1);
    // Give up if there are epsilon cycles in a non-idempotent semiring
    if (!(Weight::Properties() & kIdempotent) &&
        pfst1.Properties(kCyclic, true))
      continue;
    Weight sum1 = ShortestDistance(pfst1);

    VectorFst<Arc> cfst2, pfst2;
    Compose(ipath, sfst2, &cfst2);
    ArcSort(&cfst2, ocomp);
    Compose(cfst2, opath, &pfst2);
    // Give up if there are epsilon cycles in a non-idempotent semiring
    if (!(Weight::Properties() & kIdempotent) &&
        pfst2.Properties(kCyclic, true))
      continue;
    Weight sum2 = ShortestDistance(pfst2);

    if (!ApproxEqual(sum1, sum2, delta)) {
        VLOG(1) << "Sum1 = " << sum1;
        VLOG(1) << "Sum2 = " << sum2;
        ret = false;
        break;
    }
  }

  if (fst1.Properties(kError, false) || fst2.Properties(kError, false)) {
    if (error) *error = true;
    return false;
  }

  return ret;
}


// Test if two FSTs are equivalent by randomly generating 'num_paths' paths
// of length no more than 'path_length' using the seed 'seed' in these FSTs.
// Returns optional error value (when FLAGS_error_fatal = false).
template <class Arc>
bool RandEquivalent(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
                    ssize_t num_paths, float delta = kDelta,
                    int seed = time(0), int path_length = INT_MAX,
                    bool *error = 0) {
  UniformArcSelector<Arc> uniform_selector(seed);
  RandGenOptions< UniformArcSelector<Arc> >
      opts(uniform_selector, path_length);
  return RandEquivalent(fst1, fst2, num_paths, delta, opts, error);
}


}  // namespace fst

#endif  // FST_LIB_RANDEQUIVALENT_H_
