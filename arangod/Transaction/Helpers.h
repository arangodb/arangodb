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

#ifndef ARANGOD_TRANSACTION_HELPERS_H
#define ARANGOD_TRANSACTION_HELPERS_H 1

#include "Basics/Common.h"

namespace arangodb {
class TransactionContext;

namespace basics {
class StringBuffer;
}

namespace velocypack {
class Builder;
}

namespace transaction {
class Methods;

class StringBufferLeaser {
 public:
  explicit StringBufferLeaser(Methods*); 
  explicit StringBufferLeaser(TransactionContext*); 
  ~StringBufferLeaser();
  arangodb::basics::StringBuffer* stringBuffer() const { return _stringBuffer; }
  arangodb::basics::StringBuffer* operator->() const { return _stringBuffer; }
  arangodb::basics::StringBuffer* get() const { return _stringBuffer; }
 private:
  TransactionContext* _transactionContext;
  arangodb::basics::StringBuffer* _stringBuffer;
};

class BuilderLeaser {
 public:
  explicit BuilderLeaser(transaction::Methods*); 
  explicit BuilderLeaser(TransactionContext*); 
  ~BuilderLeaser();
  inline arangodb::velocypack::Builder* builder() const { return _builder; }
  inline arangodb::velocypack::Builder* operator->() const { return _builder; }
  inline arangodb::velocypack::Builder* get() const { return _builder; }
  inline arangodb::velocypack::Builder* steal() { 
    arangodb::velocypack::Builder* res = _builder;
    _builder = nullptr;
    return res;
  }
 private:
  TransactionContext* _transactionContext;
  arangodb::velocypack::Builder* _builder;
};

}
}

#endif
