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

#ifndef IRESEARCH_TFIDF_H
#define IRESEARCH_TFIDF_H

#include "scorers.hpp"

NS_ROOT

class tfidf_sort : public sort {
public:
  DECLARE_SORT_TYPE();

  // for use with irs::order::add<T>() and default args (static build)
  DECLARE_FACTORY_DEFAULT();

  // for use with irs::order::add(...) (dynamic build) or jSON args (static build)
  DECLARE_FACTORY_DEFAULT(const string_ref& args);

  typedef float_t score_t;

  explicit tfidf_sort(bool normalize = false);

  virtual sort::prepared::ptr prepare() const;

private:
  bool normalize_;
}; // tfidf_sort

NS_END

#endif