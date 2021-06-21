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

#ifndef IRESEARCH_ANALYZER_H
#define IRESEARCH_ANALYZER_H

#include "analysis/token_stream.hpp"
#include "utils/type_info.hpp"

namespace iresearch {
namespace analysis {

class IRESEARCH_API analyzer : public token_stream {
 public:
  using ptr = std::shared_ptr<analyzer>;

  explicit analyzer(const type_info& type) noexcept;

  virtual bool reset(const string_ref& data) = 0;

  constexpr type_info::type_id type() const noexcept { return type_; }

 private:
  type_info::type_id type_;
};

} // analysis
} // ROOT

#endif
