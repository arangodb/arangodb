//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "db/blob/blob_file_meta.h"

#include <ostream>
#include <sstream>

#include "db/blob/blob_log_format.h"
#include "rocksdb/slice.h"

namespace ROCKSDB_NAMESPACE {
uint64_t SharedBlobFileMetaData::GetBlobFileSize() const {
  return BlobLogHeader::kSize + total_blob_bytes_ + BlobLogFooter::kSize;
}

std::string SharedBlobFileMetaData::DebugString() const {
  std::ostringstream oss;
  oss << (*this);

  return oss.str();
}

std::ostream& operator<<(std::ostream& os,
                         const SharedBlobFileMetaData& shared_meta) {
  os << "blob_file_number: " << shared_meta.GetBlobFileNumber()
     << " total_blob_count: " << shared_meta.GetTotalBlobCount()
     << " total_blob_bytes: " << shared_meta.GetTotalBlobBytes()
     << " checksum_method: " << shared_meta.GetChecksumMethod()
     << " checksum_value: "
     << Slice(shared_meta.GetChecksumValue()).ToString(/* hex */ true);

  return os;
}

std::string BlobFileMetaData::DebugString() const {
  std::ostringstream oss;
  oss << (*this);

  return oss.str();
}

std::ostream& operator<<(std::ostream& os, const BlobFileMetaData& meta) {
  const auto& shared_meta = meta.GetSharedMeta();
  assert(shared_meta);
  os << (*shared_meta);

  os << " linked_ssts: {";
  for (uint64_t file_number : meta.GetLinkedSsts()) {
    os << ' ' << file_number;
  }
  os << " }";

  os << " garbage_blob_count: " << meta.GetGarbageBlobCount()
     << " garbage_blob_bytes: " << meta.GetGarbageBlobBytes();

  return os;
}

}  // namespace ROCKSDB_NAMESPACE
