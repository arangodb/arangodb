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

#ifndef ARANGOD_VOC_BASE_MASTER_POINTER_H
#define ARANGOD_VOC_BASE_MASTER_POINTER_H 1

#include "Basics/Common.h"
#include "Basics/StaticStrings.h"
#include "Utils/Transaction.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief master pointer
////////////////////////////////////////////////////////////////////////////////

struct TRI_doc_mptr_t {
 private:
  // this is the datafile identifier
  TRI_voc_fid_t _fid;   
  // the pre-calculated hash value of the key
  uint64_t _hash;       
  // this is the pointer to the beginning of the raw marker
  void const* _dataptr; 
    
  static_assert(sizeof(TRI_voc_fid_t) == sizeof(uint64_t), "invalid fid size");

 public:
  TRI_doc_mptr_t()
      : _fid(0),
        _hash(0),
        _dataptr(nullptr) {}

  // do NOT add virtual methods
  ~TRI_doc_mptr_t() {}

  void clear() {
    _fid = 0;
    _hash = 0;
    setVPack(nullptr);
  }

  // This is for cases where we explicitly have to copy originals!
  void copy(TRI_doc_mptr_t const& that) {
    _fid = that._fid;
    _dataptr = that._dataptr;
    _hash = that._hash;
  }
  
  // return the hash value for the primary key encapsulated by this
  // master pointer
  inline uint64_t getHash() const { return _hash; }
  
  // sets the hash value for the primary key encapsulated by this
  // master pointer
  inline void setHash(uint64_t hash) { _hash = hash; }

  // return the datafile id.
  inline TRI_voc_fid_t getFid() const { 
    // unmask the WAL bit
    return (_fid & ~arangodb::DatafileHelper::WalFileBitmask());
  }

  // sets datafile id. note that the highest bit of the file id must
  // not be set. the high bit will be used internally to distinguish
  // between WAL files and datafiles. if the highest bit is set, the
  // master pointer points into the WAL, and if not, it points into
  // a datafile
  inline void setFid(TRI_voc_fid_t fid, bool isWal) {
    // set the WAL bit if required
    _fid = fid;
    if (isWal) {
      _fid |= arangodb::DatafileHelper::WalFileBitmask();
    }
  }

  // set the pointer to the beginning of the VPack memory
  inline void setVPackFromMarker(TRI_df_marker_t const* value) { 
    _dataptr = reinterpret_cast<char const*>(value) + arangodb::DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT);
  }
  
  // set the pointer to the beginning of the VPack memory
  inline void setVPack(void const* value) { _dataptr = value; }

  // return a pointer to the beginning of the VPack  
  // note: this is *NOT* the beginning of the marker
  inline uint8_t const* vpack() const noexcept { 
    TRI_ASSERT(_dataptr != nullptr);
    return reinterpret_cast<uint8_t const*>(_dataptr);
  }
 
  // return the size of the stored VPack 
  inline uint32_t vpackSize() const { 
    return static_cast<uint32_t>(VPackSlice(vpack()).byteSize());
  }
  
  // return a pointer to the beginning of the Vpack  
  inline void const* dataptr() const noexcept { 
    return _dataptr;
  }
  
  // return the size of the marker 
  inline uint32_t markerSize() const { 
    return static_cast<uint32_t>(arangodb::DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT) + vpackSize());
  }
  
  inline uint32_t alignedMarkerSize() const { 
    return arangodb::DatafileHelper::AlignedSize<uint32_t>(markerSize());
  }
  
  // return a pointer to the beginning of the marker
  // this is only safe to call if pointsToWal() is false
  inline TRI_df_marker_t const* getMarkerPtr() const { 
    return reinterpret_cast<TRI_df_marker_t const*>(vpack() - arangodb::DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
  }
  
  // whether or not the master pointer points into the WAL
  // the master pointer points into the WAL if the highest bit of
  // the _fid value is set, and to a datafile otherwise
  inline bool pointsToWal() const {
    // check whether the WAL bit is set
    return ((_fid & arangodb::DatafileHelper::WalFileBitmask()) == 1);
  }

  // return the marker's revision id
  VPackSlice revisionIdAsSlice() const {
    return VPackSlice(vpack()).get(arangodb::StaticStrings::RevString);
  }

  // return the marker's revision id as string slice or None slice if not there
  TRI_voc_rid_t revisionId() const {
    return arangodb::Transaction::extractRevFromDocument(VPackSlice(vpack()));
  }
};

#endif
