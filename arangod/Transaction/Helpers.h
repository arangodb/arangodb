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
#include "Basics/StringRef.h"
#include "Utils/OperationResult.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
class CollectionNameResolver;

namespace basics {
class StringBuffer;
}

namespace velocypack {
class Builder;
}

namespace transaction {
class Context;
class Methods;

namespace helpers {
  /// @brief extract the _key attribute from a slice
  StringRef extractKeyPart(VPackSlice);
  
  std::string extractIdString(CollectionNameResolver const*, 
                              VPackSlice, VPackSlice const&);

  /// @brief quick access to the _key attribute in a database document
  /// the document must have at least two attributes, and _key is supposed to
  /// be the first one
  VPackSlice extractKeyFromDocument(VPackSlice);
  
  /// @brief quick access to the _id attribute in a database document
  /// the document must have at least two attributes, and _id is supposed to
  /// be the second one
  /// note that this may return a Slice of type Custom!
  VPackSlice extractIdFromDocument(VPackSlice);
  
  /// @brief quick access to the _from attribute in a database document
  /// the document must have at least five attributes: _key, _id, _from, _to
  /// and _rev (in this order)
  VPackSlice extractFromFromDocument(VPackSlice);

  /// @brief quick access to the _to attribute in a database document
  /// the document must have at least five attributes: _key, _id, _from, _to
  /// and _rev (in this order)
  VPackSlice extractToFromDocument(VPackSlice);
  
  /// @brief extract _key and _rev from a document, in one go
  /// this is an optimized version used when loading collections, WAL 
  /// collection and compaction
  void extractKeyAndRevFromDocument(VPackSlice slice,
                                    VPackSlice& keySlice,
                                    TRI_voc_rid_t& revisionId);
  
  /// @brief extract _rev from a database document
  TRI_voc_rid_t extractRevFromDocument(VPackSlice slice);
  VPackSlice extractRevSliceFromDocument(VPackSlice slice);


  OperationResult buildCountResult(std::vector<std::pair<std::string, uint64_t>> const& count, bool aggregate);
  
  /// @brief creates an id string from a custom _id value and the _key string
  std::string makeIdFromCustom(CollectionNameResolver const* resolver,
                               VPackSlice const& idPart, 
                               VPackSlice const& keyPart);
};

class StringBufferLeaser {
 public:
  explicit StringBufferLeaser(Methods*); 
  explicit StringBufferLeaser(transaction::Context*); 
  ~StringBufferLeaser();
  arangodb::basics::StringBuffer* stringBuffer() const { return _stringBuffer; }
  arangodb::basics::StringBuffer* operator->() const { return _stringBuffer; }
  arangodb::basics::StringBuffer* get() const { return _stringBuffer; }
 private:
  transaction::Context* _transactionContext;
  arangodb::basics::StringBuffer* _stringBuffer;
};

class BuilderLeaser {
 public:
  explicit BuilderLeaser(transaction::Methods*); 
  explicit BuilderLeaser(transaction::Context*); 
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
  transaction::Context* _transactionContext;
  arangodb::velocypack::Builder* _builder;
};

}
}

#endif
