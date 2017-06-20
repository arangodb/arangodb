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

#ifndef IRESEARCH_COLLECTOR_H
#define IRESEARCH_COLLECTOR_H

#include "shared.hpp"
#include "index/iterators.hpp"

NS_ROOT

struct IRESEARCH_API collector {
  typedef std::function<void(collector&, const attribute_store&)> prepare_f;
  typedef std::function<void(collector&, doc_id_t)> collect_f;

  static void empty_prepare(collector&, const attribute_store&) {}
  static void empty_collect(collector&, doc_id_t) {}

  prepare_f prepare = &empty_prepare;
  collect_f collect = &empty_collect;
};

NS_END

MSVC_ONLY(template class IRESEARCH_API std::function<void(irs::collector&, const irs::attribute_store&)>); // collector::prepare_f
MSVC_ONLY(template class IRESEARCH_API std::function<void(iresearch::collector&, iresearch::doc_id_t)>); // collector::collect_f

NS_ROOT

/* SELECT COUNT(*) FROM index WHERE <query> */
struct IRESEARCH_API counting_collector : collector {
  counting_collector();
  uint64_t count;
};

NS_END

#endif