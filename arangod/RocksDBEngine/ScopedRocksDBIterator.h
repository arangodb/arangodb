#pragma once

#include "infra/RocksDBMonitors.h"

#include <rocksdb/iterator.h>

#include <memory>
#include <string>

namespace arangodb {
class ScopedRocksDBIterator final : public rocksdb::Iterator {
 public:
  explicit ScopedRocksDBIterator(std::unique_ptr<rocksdb::Iterator> iterator)
      : _it(std::move(iterator)) {}

  void SeekToFirst() override {
    auto memoryScope = infra::rocksdbScope();
    _it->SeekToFirst();
  }

  void SeekToLast() override {
    auto memoryScope = infra::rocksdbScope();
    _it->SeekToLast();
  }

  void Seek(rocksdb::Slice const& target) override {
    auto memoryScope = infra::rocksdbScope();
    _it->Seek(target);
  }

  void SeekForPrev(rocksdb::Slice const& target) override {
    auto memoryScope = infra::rocksdbScope();
    _it->SeekForPrev(target);
  }

  void Next() override {
    auto memoryScope = infra::rocksdbScope();
    _it->Next();
  }

  void Prev() override {
    auto memoryScope = infra::rocksdbScope();
    _it->Prev();
  }

  rocksdb::Status Refresh() override {
    auto memoryScope = infra::rocksdbScope();
    return _it->Refresh();
  }

  rocksdb::Status Refresh(rocksdb::Snapshot const* snapshot) override {
    auto memoryScope = infra::rocksdbScope();
    return _it->Refresh(snapshot);
  }

  rocksdb::Slice value() const override {
    auto memoryScope = infra::rocksdbScope();
    return _it->value();
  }

  bool Valid() const override { return _it->Valid(); }
  rocksdb::Slice key() const override { return _it->key(); }
  rocksdb::Status status() const override { return _it->status(); }
  rocksdb::WideColumns const& columns() const override {
    return _it->columns();
  }
  rocksdb::Slice timestamp() const override { return _it->timestamp(); }
  rocksdb::Status GetProperty(std::string propName,
                              std::string* prop) override {
    return _it->GetProperty(std::move(propName), prop);
  }

 private:
  std::unique_ptr<rocksdb::Iterator> _it;
};
}  // namespace arangodb