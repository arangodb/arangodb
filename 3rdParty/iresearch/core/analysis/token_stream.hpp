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

#ifndef IRESEARCH_TOKEN_STREAM_H
#define IRESEARCH_TOKEN_STREAM_H

#include "utils/attributes.hpp"
#include "utils/attributes_provider.hpp"

NS_ROOT

class IRESEARCH_API token_stream: public util::const_attribute_store_provider {
 public:
  DECLARE_PTR(token_stream);
  virtual ~token_stream();
  virtual bool next() = 0;
};

NS_END

#endif