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

#ifndef ARANGODB_BASICS_WORK_ITEM_H
#define ARANGODB_BASICS_WORK_ITEM_H 1

namespace arangodb {
class WorkItem {
  friend class WorkMonitor;

 public:
  struct deleter {
    deleter() = default;
    void operator()(WorkItem* ptr) const { delete ptr; }
  };

  template <typename X>
  using uptr = std::unique_ptr<X, deleter>;

 protected:
  virtual ~WorkItem() {}
};
}

#endif
