////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

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