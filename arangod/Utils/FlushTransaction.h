////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_FLUSH_TRANSACTION_H
#define ARANGOD_UTILS_FLUSH_TRANSACTION_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"

namespace arangodb {

class FlushTransaction {
 public:
  FlushTransaction(FlushTransaction const&) = delete;
  FlushTransaction& operator=(FlushTransaction const&) = delete;

  explicit FlushTransaction(std::string const& name) : _name(name) {}

  virtual ~FlushTransaction() = default;

  // return the type name of the flush transaction
  // this is used when logging error messages about failed flush commits
  // so users know what exactly went wrong
  std::string const& name() { return _name; }

  // finally commit the prepared flush transaction
  virtual Result commit() = 0;

 private:
  // the name of the flush transaction. used for error logging and
  // diagnostics
  std::string const _name;
};

}  // namespace arangodb

#endif
