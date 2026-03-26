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
// Classes for registering derived FST for generic reading.

#ifndef FST_REGISTER_H_
#define FST_REGISTER_H_

#include <string>
#include <type_traits>


#include <fst/compat.h>
#include <fst/generic-register.h>
#include <fst/util.h>


#include <fst/log.h>
#include <string_view>

namespace fst {

template <class Arc>
class Fst;

struct FstReadOptions;

// This class represents a single entry in a FstRegister
template <class Arc>
struct FstRegisterEntry {
  using Reader = Fst<Arc> *(*)(std::istream &istrm, const FstReadOptions &opts);
  using Converter = Fst<Arc> *(*)(const Fst<Arc> &fst);

  Reader reader;
  Converter converter;

  explicit FstRegisterEntry(Reader reader = nullptr,
                            Converter converter = nullptr)
      : reader(reader), converter(converter) {}
};

// This class maintains the correspondence between a string describing
// an FST type, and its reader and converter.
template <class Arc>
class FstRegister : public GenericRegister<std::string, FstRegisterEntry<Arc>,
                                           FstRegister<Arc>> {
 public:
  using Reader = typename FstRegisterEntry<Arc>::Reader;
  using Converter = typename FstRegisterEntry<Arc>::Converter;

  const Reader GetReader(std::string_view type) const {
    return this->GetEntry(type).reader;
  }

  const Converter GetConverter(std::string_view type) const {
    return this->GetEntry(type).converter;
  }

 protected:
  std::string ConvertKeyToSoFilename(std::string_view key) const override {
    std::string legal_type(key);
    ConvertToLegalCSymbol(&legal_type);
    legal_type.append("-fst.so");
    return legal_type;
  }
};

// This class registers an FST type for generic reading and creating.
// The type must have a default constructor and a copy constructor from
// Fst<Arc>.
template <class FST>
class FstRegisterer : public GenericRegisterer<FstRegister<typename FST::Arc>> {
 public:
  using Arc = typename FST::Arc;
  using Entry = typename FstRegister<Arc>::Entry;
  using Reader = typename FstRegister<Arc>::Reader;

  FstRegisterer()
      : GenericRegisterer<FstRegister<typename FST::Arc>>(FST().Type(),
                                                          BuildEntry()) {}

 private:
  static Fst<Arc> *ReadGeneric(std::istream &strm, const FstReadOptions &opts) {
    static_assert(std::is_base_of_v<Fst<Arc>, FST>,
                  "FST class does not inherit from Fst<Arc>");
    return FST::Read(strm, opts);
  }

  static Entry BuildEntry() {
    return Entry(&ReadGeneric, &FstRegisterer<FST>::Convert);
  }

  static Fst<Arc> *Convert(const Fst<Arc> &fst) { return new FST(fst); }
};

// Convenience macro to generate a static FstRegisterer instance.
// `FST` and `Arc` must be identifiers (that is, not a qualified type).
// Users SHOULD NOT register within the fst namespace. To register an
// FST for StdArc, for example, use:
// namespace example {
// using fst::StdArc;
// REGISTER_FST(MyFst, StdArc);
// }  // namespace example
#define REGISTER_FST(FST, Arc) \
  static fst::FstRegisterer<FST<Arc>> FST##_##Arc##_registerer

// Converts an FST to the specified type.
template <class Arc>
Fst<Arc> *Convert(const Fst<Arc> &fst, std::string_view fst_type) {
  auto *reg = FstRegister<Arc>::GetRegister();
  const auto converter = reg->GetConverter(fst_type);
  if (!converter) {
    FSTERROR() << "Fst::Convert: Unknown FST type " << fst_type << " (arc type "
               << Arc::Type() << ")";
    return nullptr;
  }
  return converter(fst);
}

}  // namespace fst

#endif  // FST_REGISTER_H_
