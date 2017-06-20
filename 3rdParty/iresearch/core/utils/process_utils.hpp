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

#ifndef IRESEARCH_PROCESS_UTILS_H
#define IRESEARCH_PROCESS_UTILS_H

#ifdef _WIN32
  typedef int pid_t;
#endif

NS_ROOT

IRESEARCH_API pid_t get_pid();
bool is_running(pid_t pid);
bool is_valid_pid(const char* buf);

NS_END

#endif // IRESEARCH_PROCESS_UTILS_H
