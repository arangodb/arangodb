////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_INDEXES_INDEX_RESULT_H
#define ARANGODB_INDEXES_INDEX_RESULT_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Indexes/Index.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
class IndexResult : public Result {
 public:
  IndexResult() : Result() {}

  IndexResult(int errorNumber, std::string const& errorMessage)
      : Result(errorNumber, errorMessage) {}

  IndexResult(int errorNumber, std::string&& errorMessage)
      : Result(errorNumber, std::move(errorMessage)) {}

  IndexResult(int errorNumber, Index const* index) : Result(errorNumber) {
    if (errorNumber != TRI_ERROR_NO_ERROR && index != nullptr) {
      // now provide more context based on index
      std::string msg = errorMessage();
      msg.append(" - in index ");
      msg.append(std::to_string(index->id()));
      msg.append(" of type ");
      msg.append(index->typeName());

      // build fields string
      VPackBuilder builder;
      index->toVelocyPack(builder, Index::makeFlags());
      VPackSlice fields = builder.slice().get("fields");
      if (!fields.isNone()) {
        msg.append(" over ");
        msg.append(fields.toJson());
      }
      Result::reset(errorNumber, msg);
    }
  }

  IndexResult(int errorNumber, Index const* index, std::string const& key) 
      : Result() {
    IndexResult::reset(errorNumber, index, key);
  }
  
  IndexResult& reset(int errorNumber, Index const* index, std::string const& key) {
    Result::reset(errorNumber);
    if (errorNumber != TRI_ERROR_NO_ERROR && index != nullptr) {
      // now provide more context based on index
      std::string msg = errorMessage();
      msg.append(" - in index ");
      msg.append(std::to_string(index->id()));
      msg.append(" of type ");
      msg.append(index->typeName());
      
      // build fields string
      VPackBuilder builder;
      index->toVelocyPack(builder, Index::makeFlags());
      VPackSlice fields = builder.slice().get("fields");
      if (!fields.isNone()) {
        msg.append(" over ");
        msg.append(fields.toJson());
      }
      
      // provide conflicting key
      if (!key.empty()) {
        msg.append("; conflicting key: ");
        msg.append(key);
      }
      Result::reset(errorNumber, msg);
    }
    return *this;
  }
  
  IndexResult& reset(Result& res, Index const* index) {
    Result::reset(res);
    IndexResult::reset(res.errorNumber(), index, StaticStrings::Empty);
    return *this;
  }
  
  IndexResult& reset(int res, std::string const& msg) {
    Result::reset(res, msg);
    return *this;
  }
  
};
}  // namespace arangodb

#endif
