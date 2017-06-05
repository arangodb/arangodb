// project.h

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
// Functions and classes to project an Fst on to its domain or range.

#ifndef FST_LIB_PROJECT_H__
#define FST_LIB_PROJECT_H__

#include <fst/arc-map.h>
#include <fst/mutable-fst.h>


namespace fst {

// This specifies whether to project on input or output.
enum ProjectType { PROJECT_INPUT = 1, PROJECT_OUTPUT = 2 };


// Mapper to implement projection per arc.
template <class A> class ProjectMapper {
 public:
  explicit ProjectMapper(ProjectType project_type)
      : project_type_(project_type) {}

  A operator()(const A &arc) {
    typename A::Label label = project_type_ == PROJECT_INPUT
                              ? arc.ilabel : arc.olabel;
    return A(label, label, arc.weight, arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const {
    return project_type_ == PROJECT_INPUT ? MAP_COPY_SYMBOLS :
        MAP_CLEAR_SYMBOLS;
  }

  MapSymbolsAction OutputSymbolsAction() const {
    return project_type_ == PROJECT_OUTPUT ? MAP_COPY_SYMBOLS :
        MAP_CLEAR_SYMBOLS;
  }

  uint64 Properties(uint64 props) {
    return ProjectProperties(props, project_type_ == PROJECT_INPUT);
  }


 private:
  ProjectType project_type_;
};


// Projects an FST onto its domain or range by either copying each arcs'
// input label to the output label or vice versa. This version modifies
// its input.
//
// Complexity:
// - Time: O(V + E)
// - Space: O(1)
// where V = # of states and E = # of arcs.
template<class Arc> inline
void Project(MutableFst<Arc> *fst, ProjectType project_type) {
  ArcMap(fst, ProjectMapper<Arc>(project_type));
  if (project_type == PROJECT_INPUT)
    fst->SetOutputSymbols(fst->InputSymbols());
  if (project_type == PROJECT_OUTPUT)
    fst->SetInputSymbols(fst->OutputSymbols());
}


// Projects an FST onto its domain or range by either copying each arc's
// input label to the output label or vice versa. This version is a delayed
// Fst.
//
// Complexity:
// - Time: O(v + e)
// - Space: O(1)
// where v = # of states visited, e = # of arcs visited. Constant
// time and to visit an input state or arc is assumed and exclusive
// of caching.
template <class A>
class ProjectFst : public ArcMapFst<A, A, ProjectMapper<A> > {
 public:
  typedef A Arc;
  typedef ProjectMapper<A> C;
  typedef ArcMapFstImpl< A, A, ProjectMapper<A> > Impl;
  using ImplToFst<Impl>::GetImpl;

  ProjectFst(const Fst<A> &fst, ProjectType project_type)
      : ArcMapFst<A, A, C>(fst, C(project_type)) {
    if (project_type == PROJECT_INPUT)
      GetImpl()->SetOutputSymbols(fst.InputSymbols());
    if (project_type == PROJECT_OUTPUT)
      GetImpl()->SetInputSymbols(fst.OutputSymbols());
  }

  // See Fst<>::Copy() for doc.
  ProjectFst(const ProjectFst<A> &fst, bool safe = false)
      : ArcMapFst<A, A, C>(fst, safe) {}

  // Get a copy of this ProjectFst. See Fst<>::Copy() for further doc.
  virtual ProjectFst<A> *Copy(bool safe = false) const {
    return new ProjectFst(*this, safe);
  }
};


// Specialization for ProjectFst.
template <class A>
class StateIterator< ProjectFst<A> >
    : public StateIterator< ArcMapFst<A, A, ProjectMapper<A> > > {
 public:
  explicit StateIterator(const ProjectFst<A> &fst)
      : StateIterator< ArcMapFst<A, A, ProjectMapper<A> > >(fst) {}
};


// Specialization for ProjectFst.
template <class A>
class ArcIterator< ProjectFst<A> >
    : public ArcIterator< ArcMapFst<A, A, ProjectMapper<A> > > {
 public:
  ArcIterator(const ProjectFst<A> &fst, typename A::StateId s)
      : ArcIterator< ArcMapFst<A, A, ProjectMapper<A> > >(fst, s) {}
};


// Useful alias when using StdArc.
typedef ProjectFst<StdArc> StdProjectFst;

}  // namespace fst

#endif  // FST_LIB_PROJECT_H__
