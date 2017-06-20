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

#ifndef IRESEARCH_NETWORK_UTILS_H
#define IRESEARCH_NETWORK_UTILS_H

NS_ROOT

IRESEARCH_API int get_host_name(char* name, size_t size);
bool is_same_hostname(const char* rhs, size_t size);

NS_END // ROOT

#endif // IRESEARCH_NETWORK_UTILS_H
