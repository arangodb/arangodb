////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/LoggerContext.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"

#include <cstdint>
#include <memory>
#include <optional>

struct TRI_vocbase_t;
namespace arangodb {
class LogicalCollection;
class ReplicationIterator;

namespace transaction {
class Context;
}  // namespace transaction
}  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {

struct SnapshotTransaction : transaction::Methods {
  explicit SnapshotTransaction(std::shared_ptr<transaction::Context> ctx);
  static auto options() -> transaction::Options;
  void addCollection(LogicalCollection const& collection);
};

/*
 * Used to read raw collection data in batches.
 * There is no guarantee that it will always give you the latest documents, as
 * it is supposed to fetch data from a snapshot of the current collection.
 */
struct ICollectionReader {
  virtual ~ICollectionReader() = default;

  /* Indicates if there's any more data to read from the current reader. */
  [[nodiscard]] virtual bool hasMore() = 0;

  /* Returns the total number of documents in the collection. */
  [[nodiscard]] virtual std::optional<uint64_t> getDocCount() = 0;

  /* Populates the builder with the next batch of documents.
   * The soft limit is a size limit, at least one document will be retrieved,
   * hence the builder may be larger than the given limit.
   * If there are no more documents to read, the builder will not be affected.
   */
  virtual void read(VPackBuilder& builder, std::size_t softLimit) = 0;
};

class CollectionReader : public ICollectionReader {
 public:
  explicit CollectionReader(
      std::shared_ptr<LogicalCollection> logicalCollection,
      SnapshotTransaction& trx);

  [[nodiscard]] bool hasMore() override;
  [[nodiscard]] std::optional<uint64_t> getDocCount() override;
  void read(VPackBuilder& builder, std::size_t softLimit) override;

 private:
  std::shared_ptr<LogicalCollection> _logicalCollection;
  std::unique_ptr<ReplicationIterator> _it;
  std::optional<uint64_t> _totalDocs{std::nullopt};
};

struct IDatabaseSnapshot {
  virtual ~IDatabaseSnapshot() = default;
  [[nodiscard]] virtual auto createCollectionReader(
      std::string_view collectionName)
      -> std::unique_ptr<ICollectionReader> = 0;
  virtual auto resetTransaction() -> Result = 0;
};

struct DatabaseSnapshot : IDatabaseSnapshot {
  explicit DatabaseSnapshot(TRI_vocbase_t& vocbase);
  [[nodiscard]] auto createCollectionReader(std::string_view collectionName)
      -> std::unique_ptr<ICollectionReader> override;
  auto resetTransaction() -> Result override;

 private:
  TRI_vocbase_t& _vocbase;
  std::shared_ptr<transaction::Context> _ctx;
  std::unique_ptr<SnapshotTransaction> _trx;
};

/*
 * Used to abstract away the underlying storage engine.
 */
struct IDatabaseSnapshotFactory {
  virtual ~IDatabaseSnapshotFactory() = default;
  virtual auto createSnapshot() -> std::unique_ptr<IDatabaseSnapshot> = 0;
};

class DatabaseSnapshotFactory : public IDatabaseSnapshotFactory {
 public:
  explicit DatabaseSnapshotFactory(TRI_vocbase_t& vocbase);

  auto createSnapshot() -> std::unique_ptr<IDatabaseSnapshot> override;

 private:
  TRI_vocbase_t& _vocbase;
};

}  // namespace arangodb::replication2::replicated_state::document
