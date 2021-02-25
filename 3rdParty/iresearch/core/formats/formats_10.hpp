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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FORMATS_10_H
#define IRESEARCH_FORMATS_10_H

#include "formats.hpp"

namespace iresearch {
namespace version10 {

void init();

//////////////////////////////////////////////////////////////////////////////
/// @class format
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_PLUGIN format : public irs::format {
 public:
  virtual postings_writer::ptr get_postings_writer(bool volatile_state) const = 0;
  virtual postings_reader::ptr get_postings_reader() const = 0;

 protected:
  explicit format(const type_info& type) noexcept;
}; // format

} // version10
} // ROOT

#endif
