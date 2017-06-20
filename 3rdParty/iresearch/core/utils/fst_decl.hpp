//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_FST_DECL_H
#define IRESEARCH_FST_DECL_H

#include <fst/fst-decl.h>

NS_BEGIN(fst)

template<typename Label> class StringLeftWeight;

NS_END

NS_ROOT

typedef fst::StringLeftWeight<byte_type> byte_weight;
typedef fst::ArcTpl<byte_weight> byte_arc;
typedef fst::VectorFst<byte_arc> vector_byte_fst;

template<typename Key, typename Weight> class fst_builder;
typedef fst_builder<byte_type, vector_byte_fst> fst_byte_builder;

NS_END

#endif
