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

NS_BEGIN(fst)

template<typename Label> class StringLeftWeight;

NS_BEGIN(fsa)

struct Transition;

NS_END // fsa
NS_END // fst

NS_ROOT

typedef fst::StringLeftWeight<byte_type> byte_weight;
typedef fst::ArcTpl<byte_weight> byte_arc;
typedef fst::VectorFst<byte_arc> vector_byte_fst;

template<typename Key, typename Weight> class fst_builder;
typedef fst_builder<byte_type, vector_byte_fst> fst_byte_builder;

NS_END

#endif
