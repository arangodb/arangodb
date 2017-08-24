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

#include "filter_test_case_base.hpp"

NS_BEGIN(tests)
NS_BEGIN(sort)

DEFINE_SORT_TYPE(boost);
DEFINE_FACTORY_SINGLETON(boost);

DEFINE_SORT_TYPE(custom_sort);
DEFINE_FACTORY_DEFAULT(custom_sort);

DEFINE_SORT_TYPE(frequency_sort);
DEFINE_FACTORY_SINGLETON(frequency_sort);

DEFINE_ATTRIBUTE_TYPE(frequency_sort::prepared::count);
DEFINE_FACTORY_DEFAULT(frequency_sort::prepared::count);

NS_END // NS_BEGIN(sort)
NS_END // NS_BEGIN(tests)

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------