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

#ifndef IRESEARCH_BM25_H
#define IRESEARCH_BM25_H

#include "scorers.hpp"

NS_ROOT

class bm25_sort : public sort {
 public:
  DECLARE_SORT_TYPE();

  // for use with irs::order::add<T>() and default args (static build)
  DECLARE_FACTORY_DEFAULT();

  // for use with irs::order::add(...) (dynamic build) or jSON args (static build)
  DECLARE_FACTORY_DEFAULT(const string_ref& args);

  typedef float_t score_t;

  bm25_sort(float_t k = 1.2f, float_t b = 0.75f);

  float_t k() const { return k_; }
  void k(float_t k) { k_ = k; }
  
  float_t b() const { return b_; }
  void b(float_t b) { b_ = b; }

  virtual sort::prepared::ptr prepare() const;

 private:
  float_t k_; // [1.2 .. 2.0]
  float_t b_; // 0.75
}; // bm25_sort

NS_END

#endif