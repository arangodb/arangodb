////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILS_WORK_ITEM
#define ARANGODB_UTILS_WORK_ITEM 1

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief allow unique_ptr to call protected destructor
////////////////////////////////////////////////////////////////////////////////

class WorkItem {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief unique_ptr_deleter
  //////////////////////////////////////////////////////////////////////////////

  struct deleter {
    deleter() = default;

    void operator()(WorkItem* ptr) const { delete ptr; }
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unique_ptr
  //////////////////////////////////////////////////////////////////////////////

  template <typename X>
  using uptr = std::unique_ptr<X, deleter>;

 protected:
  virtual ~WorkItem() {}
};
}

#endif
