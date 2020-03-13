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

#ifndef ARANGOD_MMFILES_MMFILES_DOCUMENT_POSITION_H
#define ARANGOD_MMFILES_MMFILES_DOCUMENT_POSITION_H 1

#include "Basics/Common.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "VocBase/Identifiers/FileId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class MMFilesDocumentPosition {
 public:
  constexpr MMFilesDocumentPosition()
      : _localDocumentId(), _fid(0), _dataptr(nullptr) {}

  MMFilesDocumentPosition(LocalDocumentId const& documentId,
                          void const* dataptr, FileId fid, bool isWal) noexcept
      : _localDocumentId(documentId), _fid(fid), _dataptr(dataptr) {
    if (isWal) {
      _fid = FileId{_fid.id() | MMFilesDatafileHelper::WalFileBitmask()};
    }
  }

  MMFilesDocumentPosition(MMFilesDocumentPosition const& other) noexcept
      : _localDocumentId(other._localDocumentId),
        _fid(other._fid),
        _dataptr(other._dataptr) {}

  MMFilesDocumentPosition& operator=(MMFilesDocumentPosition const& other) noexcept {
    _localDocumentId = other._localDocumentId;
    _fid = other._fid;
    _dataptr = other._dataptr;
    return *this;
  }

  MMFilesDocumentPosition(MMFilesDocumentPosition&& other) noexcept
      : _localDocumentId(other._localDocumentId),
        _fid(other._fid),
        _dataptr(other._dataptr) {}

  MMFilesDocumentPosition& operator=(MMFilesDocumentPosition&& other) noexcept {
    _localDocumentId = other._localDocumentId;
    _fid = other._fid;
    _dataptr = other._dataptr;
    return *this;
  }

  ~MMFilesDocumentPosition() = default;

  inline void clear() noexcept {
    _localDocumentId.clear();
    _fid.clear();
    _dataptr = nullptr;
  }

  inline LocalDocumentId localDocumentId() const noexcept {
    return _localDocumentId;
  }

  inline LocalDocumentId::BaseType localDocumentIdValue() const noexcept {
    return _localDocumentId.id();
  }

  // return the datafile id.
  inline FileId fid() const noexcept {
    // unmask the WAL bit
    return FileId{_fid.id() & ~MMFilesDatafileHelper::WalFileBitmask()};
  }

  // sets datafile id. note that the highest bit of the file id must
  // not be set. the high bit will be used internally to distinguish
  // between WAL files and datafiles. if the highest bit is set, the
  // master pointer points into the WAL, and if not, it points into
  // a datafile
  inline void fid(FileId fid, bool isWal) {
    // set the WAL bit if required
    _fid = fid;
    if (isWal) {
      _fid = FileId{_fid.id() | MMFilesDatafileHelper::WalFileBitmask()};
    }
  }

  // return a pointer to the beginning of the Vpack
  inline void const* dataptr() const noexcept { return _dataptr; }

  // set the pointer to the beginning of the VPack memory
  inline void dataptr(void const* value) { _dataptr = value; }

  // whether or not the master pointer points into the WAL
  // the master pointer points into the WAL if the highest bit of
  // the _fid value is set, and to a datafile otherwise
  inline bool pointsToWal() const noexcept {
    // check whether the WAL bit is set
    return FileId{_fid.id() & MMFilesDatafileHelper::WalFileBitmask()}.isSet();
  }

  inline operator bool() const noexcept {
    return (_localDocumentId.isSet() && _dataptr != nullptr);
  }

  inline bool operator==(MMFilesDocumentPosition const& other) const noexcept {
    return (_localDocumentId == other._localDocumentId && _fid == other._fid &&
            _dataptr == other._dataptr);
  }

 private:
  LocalDocumentId _localDocumentId;
  // this is the datafile identifier
  FileId _fid;
  // this is the pointer to the beginning of the vpack
  void const* _dataptr;

  static_assert(sizeof(FileId) == sizeof(uint64_t), "invalid fid size");
};

}  // namespace arangodb

#endif
