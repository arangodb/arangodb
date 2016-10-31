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

#include "IndexElement.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/IndexLookupContext.h"

using namespace arangodb;

HashIndexElement::HashIndexElement(TRI_voc_rid_t revisionId, std::vector<std::pair<VPackSlice, uint32_t>> const& values) 
    : _revisionId(revisionId), _hash(static_cast<uint32_t>(hash(values))) {
   
  for (size_t i = 0; i < values.size(); ++i) {
    subObject(i)->fill(values[i].first, values[i].second);
  }
}

HashIndexElement* HashIndexElement::create(TRI_voc_rid_t revisionId, std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values) {
  TRI_ASSERT(!values.empty());
  void* space = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, baseMemoryUsage(values.size()), false);

  if (space == nullptr) {
    return nullptr;
  }

  try {
    return new (space) HashIndexElement(revisionId, values);
  } catch (...) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, space);
    return nullptr;
  }
}

void HashIndexElement::free() {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, this);
}
  
/// @brief velocypack sub-object (for indexes, as part of IndexElement, 
/// if offset is non-zero, then it is an offset into the VelocyPack data in
/// the datafile or WAL file. If offset is 0, then data contains the actual data
/// in place.
arangodb::velocypack::Slice HashIndexElement::slice(IndexLookupContext* context, size_t position) const {
  IndexElementValue const* sub = subObject(position);
  
  if (sub->isInline()) {
    return arangodb::velocypack::Slice(&sub->value.data[0]);
  }
  
  uint32_t offset = sub->value.offset;
  if (offset == 0) {
    return basics::VelocyPackHelper::NullValue();
  } 
  uint8_t const* vpack = context->lookup(_revisionId);
  if (vpack == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  } 
  return VPackSlice(vpack + sub->value.offset);
}

uint64_t HashIndexElement::hash(arangodb::velocypack::Slice const& values) {
  uint64_t hash = 0x0123456789abcdef;
  size_t const n = values.length();
 
  for (size_t i = 0; i < n; ++i) {
    // must use normalized hash here, to normalize different representations 
    // of arrays/objects/numbers
    hash = values.at(i).normalizedHash(hash);
  }
  
  return hash & 0x00000000FFFFFFFFULL;
}

uint64_t HashIndexElement::hash(std::vector<arangodb::velocypack::Slice> const& values) {
  uint64_t hash = 0x0123456789abcdef;
  size_t const n = values.size();
 
  for (size_t i = 0; i < n; ++i) {
    // must use normalized hash here, to normalize different representations 
    // of arrays/objects/numbers
    hash = values[i].normalizedHash(hash);
  }
  
  return hash & 0x00000000FFFFFFFFULL;
}

uint64_t HashIndexElement::hash(std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values) {
  uint64_t hash = 0x0123456789abcdef;
  size_t const n = values.size();
 
  for (size_t i = 0; i < n; ++i) {
    // must use normalized hash here, to normalize different representations 
    // of arrays/objects/numbers
    hash = values[i].first.normalizedHash(hash);
  }
  
  return hash & 0x00000000FFFFFFFFULL;
}



SkiplistIndexElement::SkiplistIndexElement(TRI_voc_rid_t revisionId, std::vector<std::pair<VPackSlice, uint32_t>> const& values) 
    : _revisionId(revisionId) {
   
  for (size_t i = 0; i < values.size(); ++i) {
    subObject(i)->fill(values[i].first, values[i].second);
  }
}

SkiplistIndexElement* SkiplistIndexElement::create(TRI_voc_rid_t revisionId, std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values) {
  TRI_ASSERT(!values.empty());
  void* space = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, baseMemoryUsage(values.size()), false);

  if (space == nullptr) {
    return nullptr;
  }

  try {
    return new (space) SkiplistIndexElement(revisionId, values);
  } catch (...) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, space);
    return nullptr;
  }
}

void SkiplistIndexElement::free() {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, this);
}
  
/// @brief velocypack sub-object (for indexes, as part of IndexElement, 
/// if offset is non-zero, then it is an offset into the VelocyPack data in
/// the datafile or WAL file. If offset is 0, then data contains the actual data
/// in place.
arangodb::velocypack::Slice SkiplistIndexElement::slice(IndexLookupContext* context, size_t position) const {
  IndexElementValue const* sub = subObject(position);
  
  if (sub->isInline()) {
    return arangodb::velocypack::Slice(&sub->value.data[0]);
  }
  
  uint32_t offset = sub->value.offset;
  if (offset == 0) {
    return basics::VelocyPackHelper::NullValue();
  } 
  uint8_t const* vpack = context->lookup(_revisionId);
  if (vpack == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  } 
  return VPackSlice(vpack + sub->value.offset);
}


SimpleIndexElement::SimpleIndexElement(TRI_voc_rid_t revisionId, arangodb::velocypack::Slice const& value, uint32_t offset)
    : _revisionId(revisionId), _hashAndOffset(hash(value) | (static_cast<uint64_t>(offset) << 32)) {} 
  
uint64_t SimpleIndexElement::hash(arangodb::velocypack::Slice const& value) {
  TRI_ASSERT(value.isString());
  return value.hashString() & 0x00000000FFFFFFFFULL;
}

VPackSlice SimpleIndexElement::slice(IndexLookupContext* context) const {
  uint8_t const* vpack = context->lookup(_revisionId);
  if (vpack == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  } 
  return VPackSlice(vpack + offset());
}

