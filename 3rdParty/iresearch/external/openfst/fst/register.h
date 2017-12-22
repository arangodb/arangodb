// register.h

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
// Author: riley@google.com (Michael Riley), jpr@google.com (Jake Ratkiewicz)
//
// \file
// Classes for registering derived Fsts for generic reading
//

#ifndef FST_LIB_REGISTER_H__
#define FST_LIB_REGISTER_H__

#include <string>


#include <fst/compat.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <fst/util.h>
#include <fst/generic-register.h>


#include <fst/types.h>

namespace fst {

template <class A> class Fst;
struct FstReadOptions;

// This class represents a single entry in a FstRegister
template<class A>
struct FstRegisterEntry {
  typedef Fst<A> *(*Reader)(istream &strm, const FstReadOptions &opts);
  typedef Fst<A> *(*Converter)(const Fst<A> &fst);

  Reader reader;
  Converter converter;
  FstRegisterEntry() : reader(0), converter(0) {}
  FstRegisterEntry(Reader r, Converter c) : reader(r), converter(c) { }
};

// This class maintains the correspondence between a string describing
// an FST type, and its reader and converter.
template<class A>
class FstRegister : public GenericRegister<string, FstRegisterEntry<A>,
                                           FstRegister<A> > {
 public:
  typedef typename FstRegisterEntry<A>::Reader Reader;
  typedef typename FstRegisterEntry<A>::Converter Converter;

  const Reader GetReader(const string &type) const {
    return this->GetEntry(type).reader;
  }

  const Converter GetConverter(const string &type) const {
    return this->GetEntry(type).converter;
  }

 protected:
  virtual string ConvertKeyToSoFilename(const string& key) const {
    string legal_type(key);

    ConvertToLegalCSymbol(&legal_type);

    return legal_type + "-fst.so";
  }
};


// This class registers an Fst type for generic reading and creating.
// The Fst type must have a default constructor and a copy constructor
// from 'Fst<Arc>' for this to work.
template <class F>
class FstRegisterer
  : public GenericRegisterer<FstRegister<typename F::Arc> > {
 public:
  typedef typename F::Arc Arc;
  typedef typename FstRegister<Arc>::Entry Entry;
  typedef typename FstRegister<Arc>::Reader Reader;

  FstRegisterer() :
      GenericRegisterer<FstRegister<typename F::Arc> >(
          F().Type(), BuildEntry()) {  }

 private:
  static Entry BuildEntry() {
    F *(*reader)(istream &strm,
                 const FstReadOptions &opts) = &F::Read;

    return Entry(reinterpret_cast<Reader>(reader),
                 &FstRegisterer<F>::Convert);
  }

  static Fst<Arc> *Convert(const Fst<Arc> &fst) { return new F(fst); }
};


// Convenience macro to generate static FstRegisterer instance.
#define REGISTER_FST(F, A) \
static fst::FstRegisterer< F<A> > F ## _ ## A ## _registerer


// Converts an fst to type 'type'.
template <class A>
Fst<A> *Convert(const Fst<A> &fst, const string &ftype) {
  FstRegister<A> *registr = FstRegister<A>::GetRegister();
  const typename FstRegister<A>::Converter
      converter = registr->GetConverter(ftype);
  if (!converter) {
    string atype = A::Type();
    LOG(ERROR) << "Fst::Convert: Unknown FST type \"" << ftype
               << "\" (arc type = \"" << atype << "\")";
    return 0;
  }
  return converter(fst);
}

}  // namespace fst

#endif  // FST_LIB_REGISTER_H__
