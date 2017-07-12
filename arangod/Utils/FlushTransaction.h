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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_FLUSH_TRANSACTION_H
#define ARANGOD_UTILS_FLUSH_TRANSACTION_H 1

#include "Basics/Common.h"

namespace arangodb {

class FlushTransaction {
 public:
  FlushTransaction(FlushTransaction const&) = delete;
  FlushTransaction& operator=(FlushTransaction const&) = delete;

  virtual ~FlushTransaction();
  
  // return the type name of the flush transaction
  // this is used when logging error messages about failed flush commits
  // so users know what exactly went wrong
  virtual char const* name() {
    return "unknown";
  }

  // finally commit the prepared flush transaction 
  // should return an int with the error code (as in errors.dat). 
  // - int == 0 (TRI_ERROR_NO_ERROR) means "success"
  // - int != 0 means "failure"
  virtual int commit() = 0;
  
};

}

#endif
