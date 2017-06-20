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

#ifndef IRESEARCH_ATTRIBUTES_PROVIDER_H
#define IRESEARCH_ATTRIBUTES_PROVIDER_H

#include "shared.hpp"

NS_ROOT

class attribute_store;
class attribute_view;

NS_BEGIN(util)

//////////////////////////////////////////////////////////////////////////////
/// @class const_attribute_store_provider
/// @brief base class for all objects with externally visible attribute_store
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API const_attribute_store_provider {
 public:
  virtual ~const_attribute_store_provider() {}
  virtual const irs::attribute_store& attributes() const NOEXCEPT = 0;
};

//////////////////////////////////////////////////////////////////////////////
/// @class attribute_store_provider
/// @brief base class for all objects with externally visible attribute-store
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API attribute_store_provider: public const_attribute_store_provider {
 public:
  virtual ~attribute_store_provider() {}
  virtual irs::attribute_store& attributes() NOEXCEPT = 0;
  virtual const irs::attribute_store& attributes() const NOEXCEPT override final {
    return const_cast<attribute_store_provider*>(this)->attributes();
  };
};

//////////////////////////////////////////////////////////////////////////////
/// @class const_attribute_view_provider
/// @brief base class for all objects with externally visible attribute_view
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API const_attribute_view_provider {
 public:
  virtual ~const_attribute_view_provider() {}
  virtual const irs::attribute_view& attributes() const NOEXCEPT = 0;
};

//////////////////////////////////////////////////////////////////////////////
/// @class attributes_provider
/// @brief base class for all objects with externally visible attribute_view
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API attribute_view_provider: public const_attribute_view_provider {
 public:
  virtual ~attribute_view_provider() {}
  virtual irs::attribute_view& attributes() NOEXCEPT = 0;
  virtual const irs::attribute_view& attributes() const NOEXCEPT override final {
    return const_cast<attribute_view_provider*>(this)->attributes();
  };
};

NS_END
NS_END

#endif
