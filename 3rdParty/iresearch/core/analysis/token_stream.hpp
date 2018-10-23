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

#ifndef IRESEARCH_TOKEN_STREAM_H
#define IRESEARCH_TOKEN_STREAM_H

#include "utils/attributes.hpp"
#include "utils/attributes_provider.hpp"

NS_ROOT

class IRESEARCH_API token_stream: public util::const_attribute_view_provider {
 public:
  DECLARE_UNIQUE_PTR(token_stream);
  virtual ~token_stream();
  virtual bool next() = 0;
};

NS_END

#endif
