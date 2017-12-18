// arcfilter.h

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
// Function objects to restrict which arcs are traversed in an FST.

#ifndef FST_LIB_ARCFILTER_H__
#define FST_LIB_ARCFILTER_H__


#include <fst/fst.h>
#include <fst/util.h>


namespace fst {

// True for all arcs.
template <class A>
class AnyArcFilter {
public:
  bool operator()(const A &arc) const { return true; }
};


// True for (input/output) epsilon arcs.
template <class A>
class EpsilonArcFilter {
public:
  bool operator()(const A &arc) const {
    return arc.ilabel == 0 && arc.olabel == 0;
  }
};


// True for input epsilon arcs.
template <class A>
class InputEpsilonArcFilter {
public:
  bool operator()(const A &arc) const {
    return arc.ilabel == 0;
  }
};


// True for output epsilon arcs.
template <class A>
class OutputEpsilonArcFilter {
public:
  bool operator()(const A &arc) const {
    return arc.olabel == 0;
  }
};


// True if specified labels match (don't match) when keep_match is
// true (false).
template <class A>
class MultiLabelArcFilter {
public:
  typedef typename A::Label Label;

  MultiLabelArcFilter(bool match_input = true, bool keep_match = true)
    :  match_input_(match_input),
       keep_match_(keep_match) {}


  bool operator()(const A &arc) const {
    Label label = match_input_ ? arc.ilabel : arc.olabel;
    bool match = labels_.Find(label) != labels_.End();
    return keep_match_ ? match : !match;
  }

  void AddLabel(Label label) {
    labels_.Insert(label);
  }

private:
  CompactSet<Label, kNoLabel> labels_;
  bool match_input_;
  bool keep_match_;
};

}  // namespace fst

#endif  // FST_LIB_ARCFILTER_H__
