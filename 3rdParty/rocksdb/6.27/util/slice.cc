//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "rocksdb/slice.h"

#include <stdio.h>

#include <algorithm>

#include "rocksdb/convenience.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/utilities/object_registry.h"
#include "rocksdb/utilities/options_type.h"
#include "util/string_util.h"

namespace ROCKSDB_NAMESPACE {

namespace {
static std::unordered_map<std::string, OptionTypeInfo>
    slice_transform_length_info = {
#ifndef ROCKSDB_LITE
        {"length",
         {0, OptionType::kSizeT, OptionVerificationType::kNormal,
          OptionTypeFlags::kDontSerialize | OptionTypeFlags::kCompareNever}},
#endif  // ROCKSDB_LITE
};

class FixedPrefixTransform : public SliceTransform {
 private:
  size_t prefix_len_;

 public:
  explicit FixedPrefixTransform(size_t prefix_len) : prefix_len_(prefix_len) {
    RegisterOptions(Name(), &prefix_len_, &slice_transform_length_info);
  }

  static const char* kClassName() { return "rocksdb.FixedPrefix"; }
  static const char* kNickName() { return "fixed"; }
  const char* Name() const override { return kClassName(); }
  const char* NickName() const override { return kNickName(); }

  std::string GetId() const override {
    return std::string(Name()) + "." + ROCKSDB_NAMESPACE::ToString(prefix_len_);
  }

  Slice Transform(const Slice& src) const override {
    assert(InDomain(src));
    return Slice(src.data(), prefix_len_);
  }

  bool InDomain(const Slice& src) const override {
    return (src.size() >= prefix_len_);
  }

  bool InRange(const Slice& dst) const override {
    return (dst.size() == prefix_len_);
  }

  bool FullLengthEnabled(size_t* len) const override {
    *len = prefix_len_;
    return true;
  }

  bool SameResultWhenAppended(const Slice& prefix) const override {
    return InDomain(prefix);
  }
};

class CappedPrefixTransform : public SliceTransform {
 private:
  size_t cap_len_;

 public:
  explicit CappedPrefixTransform(size_t cap_len) : cap_len_(cap_len) {
    RegisterOptions(Name(), &cap_len_, &slice_transform_length_info);
  }

  static const char* kClassName() { return "rocksdb.CappedPrefix"; }
  static const char* kNickName() { return "capped"; }
  const char* Name() const override { return kClassName(); }
  const char* NickName() const override { return kNickName(); }
  std::string GetId() const override {
    return std::string(Name()) + "." + ROCKSDB_NAMESPACE::ToString(cap_len_);
  }

  Slice Transform(const Slice& src) const override {
    assert(InDomain(src));
    return Slice(src.data(), std::min(cap_len_, src.size()));
  }

  bool InDomain(const Slice& /*src*/) const override { return true; }

  bool InRange(const Slice& dst) const override {
    return (dst.size() <= cap_len_);
  }

  bool FullLengthEnabled(size_t* len) const override {
    *len = cap_len_;
    return true;
  }

  bool SameResultWhenAppended(const Slice& prefix) const override {
    return prefix.size() >= cap_len_;
  }
};

class NoopTransform : public SliceTransform {
 public:
  explicit NoopTransform() { }

  static const char* kClassName() { return "rocksdb.Noop"; }
  const char* Name() const override { return kClassName(); }

  Slice Transform(const Slice& src) const override { return src; }

  bool InDomain(const Slice& /*src*/) const override { return true; }

  bool InRange(const Slice& /*dst*/) const override { return true; }

  bool SameResultWhenAppended(const Slice& /*prefix*/) const override {
    return false;
  }
};

}  // end namespace

const SliceTransform* NewFixedPrefixTransform(size_t prefix_len) {
  return new FixedPrefixTransform(prefix_len);
}

const SliceTransform* NewCappedPrefixTransform(size_t cap_len) {
  return new CappedPrefixTransform(cap_len);
}

const SliceTransform* NewNoopTransform() { return new NoopTransform; }

#ifndef ROCKSDB_LITE
static int RegisterBuiltinSliceTransform(ObjectLibrary& library,
                                         const std::string& /*arg*/) {
  library.Register<const SliceTransform>(
      NoopTransform::kClassName(),
      [](const std::string& /*uri*/,
         std::unique_ptr<const SliceTransform>* guard,
         std::string* /*errmsg*/) {
        guard->reset(NewNoopTransform());
        return guard->get();
      });
  library.Register<const SliceTransform>(
      std::string(FixedPrefixTransform::kNickName()) + ":[0-9]+",
      [](const std::string& uri, std::unique_ptr<const SliceTransform>* guard,
         std::string* /*errmsg*/) {
        auto colon = uri.find(":");
        auto len = ParseSizeT(uri.substr(colon + 1));
        guard->reset(NewFixedPrefixTransform(len));
        return guard->get();
      });
  library.Register<const SliceTransform>(
      FixedPrefixTransform::kClassName(),
      [](const std::string& /*uri*/,
         std::unique_ptr<const SliceTransform>* guard,
         std::string* /*errmsg*/) {
        guard->reset(NewFixedPrefixTransform(0));
        return guard->get();
      });
  library.Register<const SliceTransform>(
      std::string(FixedPrefixTransform::kClassName()) + "\\.[0-9]+",
      [](const std::string& uri, std::unique_ptr<const SliceTransform>* guard,
         std::string* /*errmsg*/) {
        auto len = ParseSizeT(
            uri.substr(strlen(FixedPrefixTransform::kClassName()) + 1));
        guard->reset(NewFixedPrefixTransform(len));
        return guard->get();
      });
  library.Register<const SliceTransform>(
      std::string(CappedPrefixTransform::kNickName()) + ":[0-9]+",
      [](const std::string& uri, std::unique_ptr<const SliceTransform>* guard,
         std::string* /*errmsg*/) {
        auto colon = uri.find(":");
        auto len = ParseSizeT(uri.substr(colon + 1));
        guard->reset(NewCappedPrefixTransform(len));
        return guard->get();
      });
  library.Register<const SliceTransform>(
      std::string(CappedPrefixTransform::kClassName()) + "(\\.[0-9]+)?",
      [](const std::string& uri, std::unique_ptr<const SliceTransform>* guard,
         std::string* /*errmsg*/) {
        if (uri == CappedPrefixTransform::kClassName()) {
          guard->reset(NewCappedPrefixTransform(0));
        } else {  // Length + "."
          auto len = ParseSizeT(
              uri.substr(strlen(CappedPrefixTransform::kClassName()) + 1));
          guard->reset(NewCappedPrefixTransform(len));
        }
        return guard->get();
      });
  return 5;
}
#endif  // ROCKSDB_LITE

Status SliceTransform::CreateFromString(
    const ConfigOptions& config_options, const std::string& value,
    std::shared_ptr<const SliceTransform>* result) {
#ifndef ROCKSDB_LITE
  static std::once_flag once;
  std::call_once(once, [&]() {
    RegisterBuiltinSliceTransform(*(ObjectLibrary::Default().get()), "");
  });
#endif  // ROCKSDB_LITE
  std::string id;
  std::unordered_map<std::string, std::string> opt_map;
  Status status = Customizable::GetOptionsMap(config_options, result->get(),
                                              value, &id, &opt_map);
  if (!status.ok()) {  // GetOptionsMap failed
    return status;
  } else if (id.empty() && opt_map.empty()) {
    result->reset();
  } else {
#ifndef ROCKSDB_LITE
    status = config_options.registry->NewSharedObject(id, result);
#else
    auto Matches = [](const std::string& input, size_t size,
                      const char* pattern, char sep) {
      auto plen = strlen(pattern);
      return (size > plen + 2 && input[plen] == sep &&
              StartsWith(input, pattern));
    };

    auto size = id.size();
    if (id == NoopTransform::kClassName()) {
      result->reset(NewNoopTransform());
    } else if (Matches(id, size, FixedPrefixTransform::kNickName(), ':')) {
      auto fixed = strlen(FixedPrefixTransform::kNickName());
      auto len = ParseSizeT(id.substr(fixed + 1));
      result->reset(NewFixedPrefixTransform(len));
    } else if (Matches(id, size, CappedPrefixTransform::kNickName(), ':')) {
      auto capped = strlen(CappedPrefixTransform::kNickName());
      auto len = ParseSizeT(id.substr(capped + 1));
      result->reset(NewCappedPrefixTransform(len));
    } else if (Matches(id, size, CappedPrefixTransform::kClassName(), '.')) {
      auto capped = strlen(CappedPrefixTransform::kClassName());
      auto len = ParseSizeT(id.substr(capped + 1));
      result->reset(NewCappedPrefixTransform(len));
    } else if (Matches(id, size, FixedPrefixTransform::kClassName(), '.')) {
      auto fixed = strlen(FixedPrefixTransform::kClassName());
      auto len = ParseSizeT(id.substr(fixed + 1));
      result->reset(NewFixedPrefixTransform(len));
    } else {
      status = Status::NotSupported("Cannot load object in LITE mode ", id);
    }
#endif  // ROCKSDB_LITE
    if (config_options.ignore_unsupported_options && status.IsNotSupported()) {
      return Status::OK();
    } else if (status.ok()) {
      SliceTransform* transform = const_cast<SliceTransform*>(result->get());
      status =
          Customizable::ConfigureNewObject(config_options, transform, opt_map);
    }
  }
  return status;
}  // namespace ROCKSDB_NAMESPACE

std::string SliceTransform::AsString() const {
#ifndef ROCKSDB_LITE
  ConfigOptions config_options;
  config_options.delimiter = ";";
  return ToString(config_options);
#else
  return GetId();
#endif  // ROCKSDB_LITE
}

// 2 small internal utility functions, for efficient hex conversions
// and no need for snprintf, toupper etc...
// Originally from wdt/util/EncryptionUtils.cpp - for ToString(true)/DecodeHex:
char toHex(unsigned char v) {
  if (v <= 9) {
    return '0' + v;
  }
  return 'A' + v - 10;
}
// most of the code is for validation/error check
int fromHex(char c) {
  // toupper:
  if (c >= 'a' && c <= 'f') {
    c -= ('a' - 'A');  // aka 0x20
  }
  // validation
  if (c < '0' || (c > '9' && (c < 'A' || c > 'F'))) {
    return -1;  // invalid not 0-9A-F hex char
  }
  if (c <= '9') {
    return c - '0';
  }
  return c - 'A' + 10;
}

Slice::Slice(const SliceParts& parts, std::string* buf) {
  size_t length = 0;
  for (int i = 0; i < parts.num_parts; ++i) {
    length += parts.parts[i].size();
  }
  buf->reserve(length);

  for (int i = 0; i < parts.num_parts; ++i) {
    buf->append(parts.parts[i].data(), parts.parts[i].size());
  }
  data_ = buf->data();
  size_ = buf->size();
}

// Return a string that contains the copy of the referenced data.
std::string Slice::ToString(bool hex) const {
  std::string result;  // RVO/NRVO/move
  if (hex) {
    result.reserve(2 * size_);
    for (size_t i = 0; i < size_; ++i) {
      unsigned char c = data_[i];
      result.push_back(toHex(c >> 4));
      result.push_back(toHex(c & 0xf));
    }
    return result;
  } else {
    result.assign(data_, size_);
    return result;
  }
}

// Originally from rocksdb/utilities/ldb_cmd.h
bool Slice::DecodeHex(std::string* result) const {
  std::string::size_type len = size_;
  if (len % 2) {
    // Hex string must be even number of hex digits to get complete bytes back
    return false;
  }
  if (!result) {
    return false;
  }
  result->clear();
  result->reserve(len / 2);

  for (size_t i = 0; i < len;) {
    int h1 = fromHex(data_[i++]);
    if (h1 < 0) {
      return false;
    }
    int h2 = fromHex(data_[i++]);
    if (h2 < 0) {
      return false;
    }
    result->push_back(static_cast<char>((h1 << 4) | h2));
  }
  return true;
}

PinnableSlice::PinnableSlice(PinnableSlice&& other) {
  *this = std::move(other);
}

PinnableSlice& PinnableSlice::operator=(PinnableSlice&& other) {
  if (this != &other) {
    Cleanable::Reset();
    Cleanable::operator=(std::move(other));
    size_ = other.size_;
    pinned_ = other.pinned_;
    if (pinned_) {
      data_ = other.data_;
      // When it's pinned, buf should no longer be of use.
    } else {
      if (other.buf_ == &other.self_space_) {
        self_space_ = std::move(other.self_space_);
        buf_ = &self_space_;
        data_ = buf_->data();
      } else {
        buf_ = other.buf_;
        data_ = other.data_;
      }
    }
    other.self_space_.clear();
    other.buf_ = &other.self_space_;
    other.pinned_ = false;
    other.PinSelf();
  }
  return *this;
}

}  // namespace ROCKSDB_NAMESPACE
