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

#ifndef IRESEARCH_NONCOPYABLE_H
#define IRESEARCH_NONCOPYABLE_H

#include "shared.hpp"

NS_ROOT
NS_BEGIN( util )

NS_BEGIN( noncopyable__detail__ )

struct noncopyable {
  noncopyable() = default;
  ~noncopyable() = default;

  noncopyable( const noncopyable& ) = delete;
  noncopyable& operator= ( const noncopyable& ) = delete;
};

NS_END // noncopyable__detail__ 

using noncopyable = noncopyable__detail__::noncopyable;

NS_END
NS_END

#endif
