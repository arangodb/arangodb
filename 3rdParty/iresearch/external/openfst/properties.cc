// properties.cc

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
// Functions for updating property bits for various FST operations and
// string names of the properties.

#include <fst/properties.h>

#include <stddef.h>
#include <vector>
using std::vector;

namespace fst {

// These functions determine the properties associated with the FST
// result of various finite-state operations. The property arguments
// correspond to the operation's FST arguments. The properties
// returned assume the operation modifies its first argument.
// Bitwise-and this result with kCopyProperties for the case when a
// new (possibly delayed) FST is instead constructed.

// Properties for a concatenatively-closed FST.
uint64 ClosureProperties(uint64 inprops, bool star, bool delayed) {
  uint64 outprops = (kError | kAcceptor | kUnweighted | kAccessible) & inprops;
  if (!delayed)
       outprops |= (kExpanded | kMutable | kCoAccessible |
                    kNotTopSorted | kNotString) & inprops;
  if (!delayed || inprops & kAccessible)
    outprops |= (kNotAcceptor | kNonIDeterministic | kNonODeterministic |
                 kNotILabelSorted | kNotOLabelSorted | kWeighted |
                 kNotAccessible | kNotCoAccessible) & inprops;
  return outprops;
}

// Properties for a complemented FST.
uint64 ComplementProperties(uint64 inprops) {
  uint64 outprops = kAcceptor | kUnweighted | kNoEpsilons |
                    kNoIEpsilons | kNoOEpsilons |
                    kIDeterministic | kODeterministic | kAccessible;
  outprops |= (kError | kILabelSorted | kOLabelSorted | kInitialCyclic) &
      inprops;
  if (inprops & kAccessible)
    outprops |= kNotILabelSorted | kNotOLabelSorted | kCyclic;
  return outprops;
}

// Properties for a composed FST.
uint64 ComposeProperties(uint64 inprops1, uint64 inprops2) {
  uint64 outprops = kError & (inprops1 | inprops2);
  if (inprops1 & kAcceptor && inprops2 & kAcceptor) {
    outprops |= kAcceptor | kAccessible;
    outprops |= (kNoEpsilons | kNoIEpsilons | kNoOEpsilons | kAcyclic |
                 kInitialAcyclic) & inprops1 & inprops2;
    if (kNoIEpsilons & inprops1 & inprops2)
      outprops |= (kIDeterministic | kODeterministic) & inprops1 & inprops2;
  } else {
    outprops |= kAccessible;
    outprops |= (kAcceptor | kNoIEpsilons | kAcyclic | kInitialAcyclic) &
        inprops1 & inprops2;
    if (kNoIEpsilons & inprops1 & inprops2)
      outprops |= kIDeterministic & inprops1 & inprops2;
  }
  return outprops;
}

// Properties for a concatenated FST.
uint64 ConcatProperties(uint64 inprops1, uint64 inprops2, bool delayed) {
  uint64 outprops =
    (kAcceptor | kUnweighted | kAcyclic) & inprops1 & inprops2;
  outprops |= kError & (inprops1 | inprops2);

  bool empty1 = delayed;  // Can fst1 be the empty machine?
  bool empty2 = delayed;  // Can fst2 be the empty machine?

  if (!delayed) {
    outprops |= (kExpanded | kMutable | kNotTopSorted | kNotString) & inprops1;
    outprops |= (kNotTopSorted | kNotString) & inprops2;
  }
  if (!empty1)
    outprops |= (kInitialAcyclic | kInitialCyclic) & inprops1;
  if (!delayed || inprops1 & kAccessible)
    outprops |= (kNotAcceptor | kNonIDeterministic | kNonODeterministic |
                 kEpsilons | kIEpsilons | kOEpsilons | kNotILabelSorted |
                 kNotOLabelSorted | kWeighted | kCyclic |
                 kNotAccessible | kNotCoAccessible) & inprops1;
  if ((inprops1 & (kAccessible | kCoAccessible)) ==
      (kAccessible | kCoAccessible) && !empty1) {
    outprops |= kAccessible & inprops2;
    if (!empty2)
      outprops |= kCoAccessible & inprops2;
    if (!delayed || inprops2 & kAccessible)
      outprops |= (kNotAcceptor | kNonIDeterministic | kNonODeterministic |
                   kEpsilons | kIEpsilons | kOEpsilons | kNotILabelSorted |
                   kNotOLabelSorted | kWeighted | kCyclic |
                   kNotAccessible | kNotCoAccessible) & inprops2;
  }
  return outprops;
}

// Properties for a determinized FST.
uint64 DeterminizeProperties(uint64 inprops, bool has_subsequential_label,
                             bool distinct_psubsequential_labels) {
  uint64 outprops = kAccessible;
  if ((kAcceptor & inprops) ||
      ((kNoIEpsilons & inprops) && distinct_psubsequential_labels) ||
      (has_subsequential_label && distinct_psubsequential_labels)) {
    outprops |= kIDeterministic;
  }
  outprops |= (kError | kAcceptor | kAcyclic |
               kInitialAcyclic | kCoAccessible | kString) & inprops;
  if ((inprops & kNoIEpsilons) && distinct_psubsequential_labels)
    outprops |= kNoEpsilons & inprops;
  if (inprops & kAccessible)
    outprops |= (kIEpsilons | kOEpsilons | kCyclic) & inprops;
  if (inprops & kAcceptor)
    outprops |= (kNoIEpsilons | kNoOEpsilons) & inprops;
  if ((inprops & kNoIEpsilons) && has_subsequential_label)
    outprops |= kNoIEpsilons;
  return outprops;
}

// Properties for factored weight FST.
uint64 FactorWeightProperties(uint64 inprops) {
  uint64 outprops = (kExpanded | kMutable | kError | kAcceptor |
                     kAcyclic | kAccessible | kCoAccessible) & inprops;
  if (inprops & kAccessible)
    outprops |= (kNotAcceptor | kNonIDeterministic | kNonODeterministic |
                 kEpsilons | kIEpsilons | kOEpsilons | kCyclic |
                 kNotILabelSorted | kNotOLabelSorted)
        & inprops;
  return outprops;
}

// Properties for an inverted FST.
uint64 InvertProperties(uint64 inprops) {
  uint64 outprops = (kExpanded | kMutable | kError | kAcceptor | kNotAcceptor |
                     kEpsilons | kNoEpsilons | kWeighted | kUnweighted |
                     kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic |
                     kTopSorted | kNotTopSorted |
                     kAccessible | kNotAccessible |
                     kCoAccessible | kNotCoAccessible |
                     kString | kNotString) & inprops;
  if (kIDeterministic & inprops)
    outprops |= kODeterministic;
  if (kNonIDeterministic & inprops)
    outprops |= kNonODeterministic;
  if (kODeterministic & inprops)
    outprops |= kIDeterministic;
  if (kNonODeterministic & inprops)
    outprops |= kNonIDeterministic;

  if (kIEpsilons & inprops)
    outprops |= kOEpsilons;
  if (kNoIEpsilons & inprops)
    outprops |= kNoOEpsilons;
  if (kOEpsilons & inprops)
    outprops |= kIEpsilons;
  if (kNoOEpsilons & inprops)
    outprops |= kNoIEpsilons;

  if (kILabelSorted & inprops)
    outprops |= kOLabelSorted;
  if (kNotILabelSorted & inprops)
    outprops |= kNotOLabelSorted;
  if (kOLabelSorted & inprops)
    outprops |= kILabelSorted;
  if (kNotOLabelSorted & inprops)
    outprops |= kNotILabelSorted;
  return outprops;
}

// Properties for a projected FST.
uint64 ProjectProperties(uint64 inprops, bool project_input) {
  uint64 outprops = kAcceptor;
  outprops |= (kExpanded | kMutable | kError | kWeighted | kUnweighted |
               kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic |
               kTopSorted | kNotTopSorted | kAccessible | kNotAccessible |
               kCoAccessible | kNotCoAccessible |
               kString | kNotString) & inprops;
  if (project_input) {
    outprops |= (kIDeterministic | kNonIDeterministic |
                 kIEpsilons | kNoIEpsilons |
                 kILabelSorted | kNotILabelSorted) & inprops;

    if (kIDeterministic & inprops)
      outprops |= kODeterministic;
    if (kNonIDeterministic & inprops)
      outprops |= kNonODeterministic;

    if (kIEpsilons & inprops)
      outprops |= kOEpsilons | kEpsilons;
    if (kNoIEpsilons & inprops)
      outprops |= kNoOEpsilons | kNoEpsilons;

    if (kILabelSorted & inprops)
      outprops |= kOLabelSorted;
    if (kNotILabelSorted & inprops)
      outprops |= kNotOLabelSorted;
  } else {
    outprops |= (kODeterministic | kNonODeterministic |
                 kOEpsilons | kNoOEpsilons |
                 kOLabelSorted | kNotOLabelSorted) & inprops;

    if (kODeterministic & inprops)
      outprops |= kIDeterministic;
    if (kNonODeterministic & inprops)
      outprops |= kNonIDeterministic;

    if (kOEpsilons & inprops)
      outprops |= kIEpsilons | kEpsilons;
    if (kNoOEpsilons & inprops)
      outprops |= kNoIEpsilons | kNoEpsilons;

    if (kOLabelSorted & inprops)
      outprops |= kILabelSorted;
    if (kNotOLabelSorted & inprops)
      outprops |= kNotILabelSorted;
  }
  return outprops;
}

// Properties for a randgen FST.
uint64 RandGenProperties(uint64 inprops, bool weighted) {
  uint64 outprops = kAcyclic | kInitialAcyclic | kAccessible;
  outprops |= inprops & kError;
  if (weighted) {
    outprops |= kTopSorted;
    outprops |= (kAcceptor | kNoEpsilons |
                 kNoIEpsilons | kNoOEpsilons |
                 kIDeterministic | kODeterministic |
                 kILabelSorted | kOLabelSorted) & inprops;
  } else {
    outprops |= kUnweighted;
    outprops |= (kAcceptor | kILabelSorted | kOLabelSorted) & inprops;
  }
  return outprops;
}

// Properties for a replace FST.
uint64 ReplaceProperties(const vector<uint64>& inprops,
                         ssize_t root,
                         bool epsilon_on_call,
                         bool epsilon_on_return,
                         bool replace_transducer,
                         bool no_empty_fsts) {
  if (inprops.size() == 0)
    return kNullProperties;
  uint64 outprops = 0;
  for (size_t i = 0; i < inprops.size(); ++i)
    outprops |= kError & inprops[i];
  uint64 access_props = no_empty_fsts ? kAccessible | kCoAccessible : 0;
  for (size_t i = 0; i < inprops.size(); ++i)
    access_props &= (inprops[i] & (kAccessible | kCoAccessible));
  if (access_props == (kAccessible | kCoAccessible)) {
    outprops |= access_props;
    if (inprops[root] & kInitialCyclic)
      outprops |= kInitialCyclic;
    uint64 props =  0;
    bool string = true;
    for (size_t i = 0; i < inprops.size(); ++i) {
      if (replace_transducer)
        props |= kNotAcceptor & inprops[i];
      props |= (kNonIDeterministic | kNonODeterministic | kEpsilons |
                kIEpsilons | kOEpsilons | kWeighted | kCyclic |
                kNotTopSorted | kNotString) & inprops[i];
      if (!(inprops[i] & kString))
        string = false;
    }
    outprops |= props;
    if (string)
      outprops |= kString;
  }
  bool acceptor = !replace_transducer;
  bool ideterministic = !epsilon_on_call && epsilon_on_return;
  bool no_iepsilons = !epsilon_on_call && !epsilon_on_return;
  bool acyclic = true;
  bool unweighted = true;
  for (size_t i = 0; i < inprops.size(); ++i) {
    if (!(inprops[i] & kAcceptor))
      acceptor = false;
    if (!(inprops[i] & kIDeterministic))
      ideterministic = false;
    if (!(inprops[i] & kNoIEpsilons))
      no_iepsilons = false;
    if (!(inprops[i] & kAcyclic))
      acyclic = false;
    if (!(inprops[i] & kUnweighted))
      unweighted = false;
    if (i != root && !(inprops[i] & kNoIEpsilons))
      ideterministic = false;
  }
  if (acceptor)
    outprops |= kAcceptor;
  if (ideterministic)
    outprops |= kIDeterministic;
  if (no_iepsilons)
    outprops |= kNoIEpsilons;
  if (acyclic)
    outprops |= kAcyclic;
  if (unweighted)
    outprops |= kUnweighted;
  if (inprops[root] & kInitialAcyclic)
      outprops |= kInitialAcyclic;
  return outprops;
}

// Properties for a relabeled FST.
uint64 RelabelProperties(uint64 inprops) {
  uint64 outprops = (kExpanded | kMutable | kError |
                     kWeighted | kUnweighted |
                     kCyclic | kAcyclic |
                     kInitialCyclic | kInitialAcyclic |
                     kTopSorted | kNotTopSorted |
                     kAccessible | kNotAccessible |
                     kCoAccessible | kNotCoAccessible |
                     kString | kNotString) & inprops;
  return outprops;
}

// Properties for a reversed FST. (the superinitial state limits this set)
uint64 ReverseProperties(uint64 inprops, bool has_superinitial) {
  uint64 outprops =
    (kExpanded | kMutable | kError | kAcceptor | kNotAcceptor | kEpsilons |
     kIEpsilons | kOEpsilons | kUnweighted | kCyclic | kAcyclic) & inprops;
  if (has_superinitial)
    outprops |= kWeighted & inprops;
  return outprops;
}

// Properties for re-weighted FST.
uint64 ReweightProperties(uint64 inprops) {
  uint64 outprops = inprops & kWeightInvariantProperties;
  outprops = outprops & ~kCoAccessible;
  return outprops;
}

// Properties for an epsilon-removed FST.
uint64 RmEpsilonProperties(uint64 inprops, bool delayed) {
  uint64 outprops = kNoEpsilons;
  outprops |= (kError | kAcceptor | kAcyclic | kInitialAcyclic) & inprops;
  if (inprops & kAcceptor)
    outprops |= kNoIEpsilons | kNoOEpsilons;
  if (!delayed) {
    outprops |= kExpanded | kMutable;
    outprops |= kTopSorted & inprops;
  }
  if (!delayed || inprops & kAccessible)
    outprops |= kNotAcceptor & inprops;
  return outprops;
}

// Properties for shortest path. This function computes how the properties
// of the output of shortest path need to be updated, given that 'props' is
// already known.
uint64 ShortestPathProperties(uint64 props, bool tree) {
  uint64 outprops = props | kAcyclic | kInitialAcyclic | kAccessible;
  if (!tree)
    outprops |= kCoAccessible;
  return outprops;
}

// Properties for a synchronized FST.
uint64 SynchronizeProperties(uint64 inprops) {
  uint64 outprops = (kError | kAcceptor | kAcyclic | kAccessible |
                     kCoAccessible | kUnweighted) & inprops;
  if (inprops & kAccessible)
    outprops |= (kCyclic | kNotCoAccessible | kWeighted) & inprops;
  return outprops;
}

// Properties for a unioned FST.
uint64 UnionProperties(uint64 inprops1, uint64 inprops2, bool delayed) {
  uint64 outprops = (kAcceptor | kUnweighted | kAcyclic | kAccessible)
                    & inprops1 & inprops2;
  outprops |= kError & (inprops1 | inprops2);
  outprops |= kInitialAcyclic;

  bool empty1 = delayed;  // Can fst1 be the empty machine?
  bool empty2 = delayed;  // Can fst2 be the empty machine?
  if (!delayed) {
    outprops |= (kExpanded | kMutable | kNotTopSorted) & inprops1;
    outprops |= kNotTopSorted & inprops2;
  }
  if (!empty1 && !empty2) {
    outprops |= kEpsilons | kIEpsilons | kOEpsilons;
    outprops |= kCoAccessible & inprops1 & inprops2;
  }
  // Note kNotCoAccessible does not hold because of kInitialAcyclic opt.
  if (!delayed || inprops1 & kAccessible)
    outprops |= (kNotAcceptor | kNonIDeterministic | kNonODeterministic |
                 kEpsilons | kIEpsilons | kOEpsilons | kNotILabelSorted |
                 kNotOLabelSorted | kWeighted | kCyclic |
                 kNotAccessible) & inprops1;
  if (!delayed || inprops2 & kAccessible)
    outprops |= (kNotAcceptor | kNonIDeterministic | kNonODeterministic |
                 kEpsilons | kIEpsilons | kOEpsilons | kNotILabelSorted |
                 kNotOLabelSorted | kWeighted | kCyclic |
                 kNotAccessible | kNotCoAccessible) & inprops2;
  return outprops;
}


// Property string names (indexed by bit position).
const char *PropertyNames[] = {
  // binary
  "expanded", "mutable", "error", "", "", "", "", "",
  "", "", "", "", "", "", "", "",
  // trinary
  "acceptor", "not acceptor",
  "input deterministic", "non input deterministic",
  "output deterministic", "non output deterministic",
  "input/output epsilons", "no input/output epsilons",
  "input epsilons", "no input epsilons",
  "output epsilons", "no output epsilons",
  "input label sorted", "not input label sorted",
  "output label sorted", "not output label sorted",
  "weighted", "unweighted",
  "cyclic", "acyclic",
  "cyclic at initial state", "acyclic at initial state",
  "top sorted", "not top sorted",
  "accessible", "not accessible",
  "coaccessible", "not coaccessible",
  "string", "not string",
};

}  // namespace fst
