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

#include "hash_utils.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// hashed_basic_string_ref
// -----------------------------------------------------------------------------

#if defined(_MSC_VER) && defined(IRESEARCH_DLL)

template class IRESEARCH_API hashed_basic_string_ref<char>;
template class IRESEARCH_API hashed_basic_string_ref<byte_type>;

#endif

NS_END