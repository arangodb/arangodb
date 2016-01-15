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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Basics/VelocyPackHelper.h"
#include "Basics/Exceptions.h"
#include "Basics/logging.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Basics/VPackStringBufferAdapter.h"

#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

using VelocyPackHelper = triagens::basics::VelocyPackHelper;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a default if it is does not exist
////////////////////////////////////////////////////////////////////////////////

bool VelocyPackHelper::getBooleanValue(VPackSlice const& slice,
                                       char const* name, bool defaultValue) {
  VPackSlice const& sub = slice.get(name);

  if (sub.isBoolean()) {
    return sub.getBool();
  }

  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or throws if <name> does not exist
/// or it is not a string
////////////////////////////////////////////////////////////////////////////////

std::string VelocyPackHelper::checkAndGetStringValue(VPackSlice const& slice,
                                                     char const* name) {
  TRI_ASSERT(slice.isObject());
  VPackSlice const sub = slice.get(name);
  if (!sub.isString()) {
    std::string msg = "The attribute '" + std::string(name) +
                      "' was not found or is not a string.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }
  return sub.copyString();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or the default value if it does not
/// exist
/// or it is not a string
////////////////////////////////////////////////////////////////////////////////

std::string VelocyPackHelper::getStringValue(VPackSlice const& slice,
                                             char const* name,
                                             std::string const& defaultValue) {
  TRI_ASSERT(slice.isObject());
  VPackSlice const sub = slice.get(name);
  if (!sub.isString()) {
    return defaultValue;
  }
  return sub.copyString();
}

TRI_json_t* VelocyPackHelper::velocyPackToJson(VPackSlice const& slice) {
  return JsonHelper::fromString(slice.toJson());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json file to VelocyPack
//////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> VelocyPackHelper::velocyPackFromFile(
    std::string const& path) {
  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, path.c_str(), &length);
  if (content != nullptr) {
    // The Parser might THROW
    std::shared_ptr<VPackBuilder> b;
    try {
      auto b = VPackParser::fromJson(reinterpret_cast<uint8_t const*>(content),
                                     length);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, content);
      return b;
    } catch (...) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, content);
      throw;
    }
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

static bool PrintVelocyPack(int fd, VPackSlice const& slice,
                            bool appendNewline) {
  if (slice.isNone()) {
    // sanity check
    return false;
  }

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  triagens::basics::VPackStringBufferAdapter bufferAdapter(&buffer);
  try {
    VPackDumper dumper(&bufferAdapter);
    dumper.dump(slice);
  } catch (...) {
    // Writing failed
    TRI_AnnihilateStringBuffer(&buffer);
    return false;
  }

  if (TRI_LengthStringBuffer(&buffer) == 0) {
    // should not happen
    return false;
  }

  if (appendNewline) {
    // add the newline here so we only need one write operation in the ideal
    // case
    TRI_AppendCharStringBuffer(&buffer, '\n');
  }

  char const* p = TRI_BeginStringBuffer(&buffer);
  size_t n = TRI_LengthStringBuffer(&buffer);

  while (0 < n) {
    ssize_t m = TRI_WRITE(fd, p, (TRI_write_t)n);

    if (m <= 0) {
      TRI_AnnihilateStringBuffer(&buffer);
      return false;
    }

    n -= m;
    p += m;
  }

  TRI_AnnihilateStringBuffer(&buffer);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief writes a VelocyPack to a file
//////////////////////////////////////////////////////////////////////////////

bool VelocyPackHelper::velocyPackToFile(char const* filename,
                                        VPackSlice const& slice,
                                        bool syncFile) {
  char* tmp = TRI_Concatenate2String(filename, ".tmp");

  if (tmp == nullptr) {
    return false;
  }

  // remove a potentially existing temporary file
  if (TRI_ExistsFile(tmp)) {
    TRI_UnlinkFile(tmp);
  }

  int fd = TRI_CREATE(tmp, O_CREAT | O_TRUNC | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_ERROR("cannot create json file '%s': %s", tmp, TRI_LAST_ERROR_STR);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  if (!PrintVelocyPack(fd, slice, true)) {
    TRI_CLOSE(fd);
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_ERROR("cannot write to json file '%s': %s", tmp, TRI_LAST_ERROR_STR);
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  if (syncFile) {
    LOG_TRACE("syncing tmp file '%s'", tmp);

    if (!TRI_fsync(fd)) {
      TRI_CLOSE(fd);
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_ERROR("cannot sync saved json '%s': %s", tmp, TRI_LAST_ERROR_STR);
      TRI_UnlinkFile(tmp);
      TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
      return false;
    }
  }

  int res = TRI_CLOSE(fd);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_ERROR("cannot close saved file '%s': %s", tmp, TRI_LAST_ERROR_STR);
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);
    return false;
  }

  res = TRI_RenameFile(tmp, filename);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    LOG_ERROR("cannot rename saved file '%s' to '%s': %s", tmp, filename,
              TRI_LAST_ERROR_STR);
    TRI_UnlinkFile(tmp);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

    return false;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

  return true;
}
