////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FST_DECL_H
#define IRESEARCH_FST_DECL_H

#include <fst/fst-decl.h>

namespace fst {

template<typename Label>
class StringLeftWeight;

namespace fstext {

template<typename Label>
class StringRefWeight;

template<typename W, typename L>
struct ILabelArc;

template<typename Arc>
class ImmutableFst;

} // fstext
} // fst

namespace iresearch {

using byte_weight = fst::StringLeftWeight<byte_type>;
using byte_arc = fst::ArcTpl<byte_weight>;
using vector_byte_fst = fst::VectorFst<byte_arc>;

using byte_ref_weight = fst::fstext::StringRefWeight<byte_type>;
using byte_ref_arc = fst::fstext::ILabelArc<byte_ref_weight, int32_t>;
using immutable_byte_fst = fst::fstext::ImmutableFst<byte_ref_arc>;

template<typename Key, typename Weight, typename Stats>
class fst_builder;

}

#endif
