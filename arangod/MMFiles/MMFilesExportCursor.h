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

#ifndef ARANGOD_MMFILES_MMFILES_EXPORT_CURSOR_H
#define ARANGOD_MMFILES_MMFILES_EXPORT_CURSOR_H 1

#include "Basics/Common.h"
#include "Utils/Cursor.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class MMFilesCollectionExport;

class MMFilesExportCursor final : public Cursor {
 public:
  MMFilesExportCursor(TRI_vocbase_t& vocbase, CursorId id,
                      std::unique_ptr<arangodb::MMFilesCollectionExport> ex, 
                      size_t batchSize, double ttl, bool hasCount);

  ~MMFilesExportCursor();

  CursorType type() const override final { return CURSOR_EXPORT; }

  bool hasNext();

  arangodb::velocypack::Slice next();

  size_t count() const override final;

  std::pair<arangodb::aql::ExecutionState, Result> dump(velocypack::Builder& result,
                                                        std::function<void()> const& ch) override final;

  Result dumpSync(velocypack::Builder& result) override final;

  std::shared_ptr<transaction::Context> context() const override final;

 private:
  DatabaseGuard _guard;
  std::unique_ptr<arangodb::MMFilesCollectionExport> _ex;
  size_t _position;
  size_t const _size;
};

}  // namespace arangodb

#endif
