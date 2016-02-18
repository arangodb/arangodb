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

#ifndef ARANGOD_UTILS_CURSOR_H
#define ARANGOD_UTILS_CURSOR_H 1

#include "Basics/Common.h"
#include "Basics/StringBuffer.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

class CollectionExport;

typedef TRI_voc_tick_t CursorId;

class Cursor {
 public:
  Cursor(Cursor const&) = delete;
  Cursor& operator=(Cursor const&) = delete;

  Cursor(CursorId, size_t, std::shared_ptr<arangodb::velocypack::Builder>, double, bool);

  virtual ~Cursor();

 public:
  CursorId id() const { return _id; }

  size_t batchSize() const { return _batchSize; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a slice to read the extra values.
  /// Make sure the Cursor Object is not destroyed while reading this slice.
  /// If no extras are set this will return a NONE slice.
  //////////////////////////////////////////////////////////////////////////////
  
  arangodb::velocypack::Slice extra() const;

  bool hasCount() const { return _hasCount; }

  double ttl() const { return _ttl; }

  double expires() const { return _expires; }

  bool isUsed() const { return _isUsed; }

  bool isDeleted() const { return _isDeleted; }

  void deleted() { _isDeleted = true; }

  void use() {
    TRI_ASSERT(!_isDeleted);
    TRI_ASSERT(!_isUsed);

    _isUsed = true;
    _expires = TRI_microtime() + _ttl;
  }

  void release() {
    TRI_ASSERT(_isUsed);
    _isUsed = false;
  }

  virtual bool hasNext() = 0;

  virtual arangodb::velocypack::Slice next() = 0;

  virtual size_t count() const = 0;

  virtual void dump(arangodb::basics::StringBuffer&) = 0;

 protected:
  CursorId const _id;
  size_t const _batchSize;
  size_t _position;
  std::shared_ptr<arangodb::velocypack::Builder> _extra;
  double _ttl;
  double _expires;
  bool const _hasCount;
  bool _isDeleted;
  bool _isUsed;
};

class JsonCursor : public Cursor {
 public:
  JsonCursor(TRI_vocbase_t*, CursorId,
             std::shared_ptr<arangodb::velocypack::Builder>, size_t,
             std::shared_ptr<arangodb::velocypack::Builder>, double, bool,
             bool);

  ~JsonCursor();

 public:
  bool hasNext() override final;

  arangodb::velocypack::Slice next() override final;

  size_t count() const override final;

  void dump(arangodb::basics::StringBuffer&) override final;

 private:
  void freeJson();

 private:
  TRI_vocbase_t* _vocbase;
  std::shared_ptr<arangodb::velocypack::Builder> _json;
  size_t const _size;
  bool _cached;
};

class ExportCursor : public Cursor {
 public:
  ExportCursor(TRI_vocbase_t*, CursorId, arangodb::CollectionExport*, size_t,
               double, bool);

  ~ExportCursor();

 public:
  bool hasNext() override final;

  arangodb::velocypack::Slice next() override final;

  size_t count() const override final;

  void dump(arangodb::basics::StringBuffer&) override final;

 private:
  TRI_vocbase_t* _vocbase;
  arangodb::CollectionExport* _ex;
  size_t const _size;
};
}

#endif
