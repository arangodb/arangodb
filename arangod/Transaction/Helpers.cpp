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

#include "Helpers.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/encoding.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "Transaction/Context.h"

#include <velocypack/Builder.h>

using namespace arangodb;

/// @brief quick access to the _key attribute in a database document
/// the document must have at least two attributes, and _key is supposed to
/// be the first one
VPackSlice transaction::helpers::extractKeyFromDocument(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  TRI_ASSERT(slice.isObject());

  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // a regular document must have at least the three attributes
  // _key, _id and _rev (in this order). _key must be the first attribute
  // however this method may also be called for remove markers, which only
  // have _key and _rev. therefore the only assertion that we can make
  // here is that the document at least has two attributes

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());

  if (*p == basics::VelocyPackHelper::KeyAttribute) {
    // the + 1 is required so that we can skip over the attribute name
    // and point to the attribute value
    return VPackSlice(p + 1);
  }

  // fall back to the regular lookup method
  return slice.get(StaticStrings::KeyString);
}

/// @brief extract the _key attribute from a slice
StringRef transaction::helpers::extractKeyPart(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }

  // extract _key
  if (slice.isObject()) {
    VPackSlice k = slice.get(StaticStrings::KeyString);
    if (!k.isString()) {
      return StringRef(); // fail
    }
    return StringRef(k);
  } 
  if (slice.isString()) {
    StringRef key(slice);
    size_t pos = key.find('/');
    if (pos == std::string::npos) {
      return key;
    }
    return key.substr(pos + 1);
  } 
  return StringRef();
}

/// @brief extract the _id attribute from a slice, and convert it into a 
/// string, static method
std::string transaction::helpers::extractIdString(CollectionNameResolver const* resolver,
                                         VPackSlice slice,
                                         VPackSlice const& base) {
  VPackSlice id;

  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
   
  if (slice.isObject()) {
    // extract id attribute from object
    uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
    if (*p == basics::VelocyPackHelper::KeyAttribute) {
      // skip over attribute name
      ++p;
      VPackSlice key = VPackSlice(p);
      // skip over attribute value
      p += key.byteSize();
      
      if (*p == basics::VelocyPackHelper::IdAttribute) {
        id = VPackSlice(p + 1);
        if (id.isCustom()) {
          // we should be pointing to a custom value now
          TRI_ASSERT(id.head() == 0xf3);
 
          return makeIdFromCustom(resolver, id, key);
        }
        if (id.isString()) {
          return id.copyString();
        }
      }
    }

    // in case the quick access above did not work out, use the slow path... 
    id = slice.get(StaticStrings::IdString);
  } else {
    id = slice;
  }
  
  if (id.isString()) {
    // already a string...
    return id.copyString();
  }
  if (!id.isCustom() || id.head() != 0xf3) {
    // invalid type for _id
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // we now need to extract the _key attribute
  VPackSlice key;
  if (slice.isObject()) {
    key = slice.get(StaticStrings::KeyString);
  } else if (base.isObject()) {
    key = extractKeyFromDocument(base);
  } else if (base.isExternal()) {
    key = base.resolveExternal().get(StaticStrings::KeyString);
  }

  if (!key.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
        
  return makeIdFromCustom(resolver, id, key);
}

/// @brief quick access to the _id attribute in a database document
/// the document must have at least two attributes, and _id is supposed to
/// be the second one
/// note that this may return a Slice of type Custom!
VPackSlice transaction::helpers::extractIdFromDocument(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  TRI_ASSERT(slice.isObject());
  
  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // a regular document must have at least the three attributes 
  // _key, _id and _rev (in this order). _id must be the second attribute

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());

  if (*p == basics::VelocyPackHelper::KeyAttribute) {
    // skip over _key 
    ++p;
    // skip over _key value
    p += VPackSlice(p).byteSize();
    if (*p == basics::VelocyPackHelper::IdAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value 
      return VPackSlice(p + 1); 
    }
  }
  
  // fall back to the regular lookup method
  return slice.get(StaticStrings::IdString); 
}

/// @brief quick access to the _from attribute in a database document
/// the document must have at least five attributes: _key, _id, _from, _to
/// and _rev (in this order)
VPackSlice transaction::helpers::extractFromFromDocument(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  TRI_ASSERT(slice.isObject());
  
  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // this method must only be called on edges
  // this means we must have at least the attributes  _key, _id, _from, _to and _rev

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::FromAttribute && ++count <= 3) {
    if (*p == basics::VelocyPackHelper::FromAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value 
      return VPackSlice(p + 1);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to the regular lookup method
  return slice.get(StaticStrings::FromString); 
}

/// @brief quick access to the _to attribute in a database document
/// the document must have at least five attributes: _key, _id, _from, _to
/// and _rev (in this order)
VPackSlice transaction::helpers::extractToFromDocument(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  
  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // this method must only be called on edges
  // this means we must have at least the attributes  _key, _id, _from, _to and _rev

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 4) {
    if (*p == basics::VelocyPackHelper::ToAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value 
      return VPackSlice(p + 1);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }
  
  // fall back to the regular lookup method
  return slice.get(StaticStrings::ToString); 
}

/// @brief extract _key and _rev from a document, in one go
/// this is an optimized version used when loading collections, WAL 
/// collection and compaction
void transaction::helpers::extractKeyAndRevFromDocument(VPackSlice slice, 
                                               VPackSlice& keySlice, 
                                               TRI_voc_rid_t& revisionId) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.length() >= 2); 

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;
  bool foundKey = false;
  bool foundRev = false;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 5) {
    if (*p == basics::VelocyPackHelper::KeyAttribute) {
      keySlice = VPackSlice(p + 1);
      if (foundRev) {
        return;
      }
      foundKey = true;
    } else if (*p == basics::VelocyPackHelper::RevAttribute) {
      VPackSlice revSlice(p + 1);
      if (revSlice.isString()) {
        VPackValueLength l;
        char const* p = revSlice.getString(l);
        revisionId = TRI_StringToRid(p, l, false);
      } else if (revSlice.isNumber()) {
        revisionId = revSlice.getNumericValue<TRI_voc_rid_t>();
      }
      if (foundKey) {
        return;
      }
      foundRev = true;
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to regular lookup
  {
    keySlice = slice.get(StaticStrings::KeyString);    
    VPackValueLength l;
    char const* p = slice.get(StaticStrings::RevString).getString(l);
    revisionId = TRI_StringToRid(p, l, false);
  }
}

/// @brief extract _rev from a database document
TRI_voc_rid_t transaction::helpers::extractRevFromDocument(VPackSlice slice) {
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.length() >= 2); 

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 5) {
    if (*p == basics::VelocyPackHelper::RevAttribute) {
      VPackSlice revSlice(p + 1);
      if (revSlice.isString()) {
        VPackValueLength l;
        char const* p = revSlice.getString(l);
        return TRI_StringToRid(p, l, false);
      } else if (revSlice.isNumber()) {
        return revSlice.getNumericValue<TRI_voc_rid_t>();
      }
      // invalid type for revision id
      return 0;
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }
  
  // fall back to regular lookup 
  { 
    VPackValueLength l;
    char const* p = slice.get(StaticStrings::RevString).getString(l);
    return TRI_StringToRid(p, l, false);
  }
}

VPackSlice transaction::helpers::extractRevSliceFromDocument(VPackSlice slice) {
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.length() >= 2); 

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 5) {
    if (*p == basics::VelocyPackHelper::RevAttribute) {
      return VPackSlice(p + 1);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to regular lookup 
  return slice.get(StaticStrings::RevString);
}
 
OperationResult transaction::helpers::buildCountResult(std::vector<std::pair<std::string, uint64_t>> const& count, bool aggregate) {
  VPackBuilder resultBuilder;

  if (aggregate) {
    uint64_t result = 0;
    for (auto const& it : count) {
      result += it.second;
    }
    resultBuilder.add(VPackValue(result));
  } else {
    resultBuilder.openObject();
    for (auto const& it : count) {
      resultBuilder.add(it.first, VPackValue(it.second));
    }
    resultBuilder.close();
  }
  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR, false);
}

/// @brief creates an id string from a custom _id value and the _key string
std::string transaction::helpers::makeIdFromCustom(CollectionNameResolver const* resolver,
                                          VPackSlice const& id, 
                                          VPackSlice const& key) {
  TRI_ASSERT(id.isCustom() && id.head() == 0xf3);
  TRI_ASSERT(key.isString());

  uint64_t cid = encoding::readNumber<uint64_t>(id.begin() + 1, sizeof(uint64_t));
  
  std::string resolved = resolver->getCollectionNameCluster(cid);
#ifdef USE_ENTERPRISE
  if (resolved.compare(0, 7, "_local_") == 0) {
    resolved.erase(0, 7);
  } else if (resolved.compare(0, 6, "_from_") == 0) {
    resolved.erase(0, 6);
  } else if (resolved.compare(0, 4, "_to_") == 0) {
    resolved.erase(0,4);
  }
#endif
  VPackValueLength keyLength;
  char const* p = key.getString(keyLength);
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid _key value");
  }
  resolved.reserve(resolved.size() + 1 + keyLength);
  resolved.push_back('/');
  resolved.append(p, static_cast<size_t>(keyLength));
  return resolved;
}

/// @brief constructor, leases a StringBuffer
transaction::StringBufferLeaser::StringBufferLeaser(transaction::Methods* trx) 
      : _transactionContext(trx->transactionContextPtr()),
        _stringBuffer(_transactionContext->leaseStringBuffer(32)) {
}

/// @brief constructor, leases a StringBuffer
transaction::StringBufferLeaser::StringBufferLeaser(transaction::Context* transactionContext) 
      : _transactionContext(transactionContext), 
        _stringBuffer(_transactionContext->leaseStringBuffer(32)) {
}

/// @brief destructor
transaction::StringBufferLeaser::~StringBufferLeaser() { 
  _transactionContext->returnStringBuffer(_stringBuffer);
}
  
/// @brief constructor, leases a builder
transaction::BuilderLeaser::BuilderLeaser(transaction::Methods* trx) 
      : _transactionContext(trx->transactionContextPtr()), 
        _builder(_transactionContext->leaseBuilder()) {
  TRI_ASSERT(_builder != nullptr);
}

/// @brief constructor, leases a builder
transaction::BuilderLeaser::BuilderLeaser(transaction::Context* transactionContext) 
      : _transactionContext(transactionContext), 
        _builder(_transactionContext->leaseBuilder()) {
  TRI_ASSERT(_builder != nullptr);
}

/// @brief destructor
transaction::BuilderLeaser::~BuilderLeaser() { 
  if (_builder != nullptr) {
    _transactionContext->returnBuilder(_builder); 
  }
}

