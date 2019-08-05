////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_COLUMN_INFO_H
#define IRESEARCH_COLUMN_INFO_H

#include "utils/string.hpp"

#include <functional>

NS_ROOT
NS_BEGIN(compression)
class type_id;
NS_END

////////////////////////////////////////////////////////////////////////////////
/// @class column_info
////////////////////////////////////////////////////////////////////////////////
class column_info {
 public:
  column_info(const compression::type_id& compression, bool encryption) NOEXCEPT
    : compression_(&compression), encryption_(encryption) {
  }

  const compression::type_id& compression() const NOEXCEPT { return *compression_; }
  bool encryption() const NOEXCEPT { return encryption_; }

 private:
  const compression::type_id* compression_;
  bool encryption_;
}; // column_info

typedef std::function<column_info(const string_ref)> column_info_provider_t;

NS_END

#endif // IRESEARCH_COLUMN_INFO_H

