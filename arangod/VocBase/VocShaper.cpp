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
/// @author Martin Schoenert
////////////////////////////////////////////////////////////////////////////////

#include "VocShaper.h"
#include "Basics/Exceptions.h"
#include "Basics/fasthash.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"
#include "Basics/associative.h"
#include "Basics/hashes.h"
#include "Logger/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/Utf8Helper.h"
#include "VocBase/document-collection.h"
#include "Wal/LogfileManager.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an attribute id from a marker
/// TODO: remove
////////////////////////////////////////////////////////////////////////////////

static inline TRI_shape_aid_t GetAttributeId(void const* marker) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts an attribute name from a marker
/// TODO: remove
////////////////////////////////////////////////////////////////////////////////

static inline char const* GetAttributeName(void const* marker) {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a shaper
////////////////////////////////////////////////////////////////////////////////

VocShaper::VocShaper(TRI_memory_zone_t* memoryZone,
                     TRI_document_collection_t* document)
    : Shaper(),
      _memoryZone(memoryZone),
      _collection(document),
      _nextPid(1),
      _nextAid(1),  // id of next attribute to hand out
      _nextSid(Shaper::firstCustomShapeId()) {  // id of next shape to hand out

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a shaper
////////////////////////////////////////////////////////////////////////////////

VocShaper::~VocShaper() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape by identifier
/// TODO remove
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* VocShaper::lookupShapeId(TRI_shape_sid_t sid) {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute name by identifier
/// TODO remove
////////////////////////////////////////////////////////////////////////////////

char const* VocShaper::lookupAttributeId(TRI_shape_aid_t aid) {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
/// TODO remove
////////////////////////////////////////////////////////////////////////////////

TRI_shape_path_t const* VocShaper::lookupAttributePathByPid(
    TRI_shape_pid_t pid) {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an attribute path by identifier
/// TODO remove
////////////////////////////////////////////////////////////////////////////////

TRI_shape_pid_t VocShaper::findOrCreateAttributePathByName(char const* name) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute path by identifier
/// TODO remove
////////////////////////////////////////////////////////////////////////////////

TRI_shape_pid_t VocShaper::lookupAttributePathByName(char const* name) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the attribute name for an attribute path
/// TODO remove
////////////////////////////////////////////////////////////////////////////////

char const* VocShaper::attributeNameShapePid(TRI_shape_pid_t pid) {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute identifier by name
/// TODO: remove
////////////////////////////////////////////////////////////////////////////////

TRI_shape_aid_t VocShaper::lookupAttributeByName(char const* name) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds or creates an attribute identifier by name
/// TODO: remove
////////////////////////////////////////////////////////////////////////////////

TRI_shape_aid_t VocShaper::findOrCreateAttributeByName(char const* name) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a shape
/// if the function returns non-nullptr, the return value is a pointer to an
/// already existing shape and the value must not be freed
/// if the function returns nullptr, it has not found the shape and was not able
/// to create it. The value must then be freed by the caller
/// TODO: remove
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* VocShaper::findShape(TRI_shape_t* shape, bool create) {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an accessor
/// TODO: remove
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t const* VocShaper::findAccessor(TRI_shape_sid_t sid,
                                                  TRI_shape_pid_t pid) {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a sub-shape
/// TODO: remove
////////////////////////////////////////////////////////////////////////////////

bool VocShaper::extractShapedJson(TRI_shaped_json_t const* document,
                                  TRI_shape_sid_t sid, TRI_shape_pid_t pid,
                                  TRI_shaped_json_t* result,
                                  TRI_shape_t const** shape) {
  TRI_ASSERT(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape path by identifier
////////////////////////////////////////////////////////////////////////////////

TRI_shape_path_t const* VocShaper::findShapePathByName(char const* name,
                                                       bool create) {
  return nullptr;
}

