////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FILTER_VISITOR_H
#define IRESEARCH_FILTER_VISITOR_H

#include "shared.hpp"

namespace iresearch {

struct sub_reader;
struct term_reader;
struct seek_term_iterator;

//////////////////////////////////////////////////////////////////////////////
/// @class filter_visitor
/// @brief base filter visitor interface
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API filter_visitor {
  virtual ~filter_visitor() = default;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief makes preparations for a visitor
  //////////////////////////////////////////////////////////////////////////////
  virtual void prepare(const sub_reader& segment,
                       const term_reader& field,
                       const seek_term_iterator& terms) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief applies actions to a current term iterator
  //////////////////////////////////////////////////////////////////////////////
  virtual void visit(boost_t boost) = 0;
}; // filter_visitor

}

#endif // IRESEARCH_FILTER_VISITOR_H
