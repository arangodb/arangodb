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

#ifndef IRESEARCH_ANALYZER_H
#define IRESEARCH_ANALYZER_H

#include "analysis/token_stream.hpp"
#include "utils/type_id.hpp"

NS_ROOT
NS_BEGIN(analysis)

class IRESEARCH_API analyzer: public token_stream {
 public:
  DECLARE_SPTR(analyzer);

  //////////////////////////////////////////////////////////////////////////////
  /// @class type_id
  //////////////////////////////////////////////////////////////////////////////
  class type_id: public iresearch::type_id, util::noncopyable {
   public:
    type_id(const string_ref& name): name_(name) {}
    operator const type_id*() const { return this; }
    const string_ref& name() const { return name_; }

   private:
    string_ref name_;
  };

  explicit analyzer(const type_id& id);

  virtual bool reset(const string_ref& data) = 0;

  const type_id& type() const { return *type_; }

 private:
  const type_id* type_;
};

NS_END // analysis
NS_END // ROOT

#endif
