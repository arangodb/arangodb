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

#ifndef IRESEARCH_MERGE_WRITER_H
#define IRESEARCH_MERGE_WRITER_H

#include <vector>

#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"

NS_ROOT

struct directory;
class format;
typedef std::shared_ptr<format> format_ptr;
struct segment_meta;
struct sub_reader;

class IRESEARCH_API merge_writer: public util::noncopyable {
 public:
  DECLARE_PTR(merge_writer);
  merge_writer(directory& dir, const string_ref& seg_name) NOEXCEPT;
  void add(const sub_reader& reader);
  bool flush(std::string& filename, segment_meta& meta); // return merge successful

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  directory& dir_;
  string_ref name_;
  std::vector<const iresearch::sub_reader*> readers_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_END

#endif