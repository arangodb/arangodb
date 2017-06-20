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

#ifndef IRESEARCH_FILE_NAMES_H
#define IRESEARCH_FILE_NAMES_H

#include "utils/string.hpp"

#include <memory>

NS_ROOT

// returns string in the following format : _{gen}
std::string file_name(uint64_t gen);

// returns string in the following format : {prefix}{gen}
std::string IRESEARCH_API file_name(const string_ref& prefix, uint64_t gen);

// returns string in the following format : {name}.{ext}
std::string IRESEARCH_API file_name(const string_ref& name, const string_ref& ext);
void IRESEARCH_API file_name(std::string& out, const string_ref& name, const string_ref& ext);

// returns string in the following format : {name}.{gen}.{ext}
std::string IRESEARCH_API file_name(const string_ref& name, uint64_t gen, const string_ref& ext);

NS_END

#endif
