# Vector Index Replication Bug: `replicationFactor` Increase vs Initial Creation

## Overview

There are two fundamentally different code paths for how a follower DBServer obtains a vector index in a cluster. One works correctly; the other results in an **untrained vector index** on the new follower.

---

## Path A: Create Collection First, Then Create Vector Index

This is the "normal" path when a collection already exists with `replicationFactor > 1` and a user creates a vector index afterwards.

### Flow

1. **Collection already exists** on all replicas (leader + followers).
2. **Documents are inserted** — leader replicates via the replicated log, followers apply inserts. All followers now have documents locally.
3. **User creates a vector index** — coordinator creates an `ENSURE_INDEX` maintenance action, routed to the **leader only**.
4. **Leader creates index locally**:
   - `EnsureIndex::ensureIndexReplication2()` → `DocumentLeaderState::createIndex()`
   - Builds the replication callback using the **original `sharedIndexInfo`** from the coordinator (**without `trainedData`**):
     ```cpp
     // DocumentLeaderState.cpp:736-737
     auto op = ReplicatedOperation::buildCreateIndexOperation(
         shard, sharedIndexInfo, progress);
     ```
   - Calls `RocksDBCollection::createIndex()`:
     - **Step 2**: Instantiates `RocksDBVectorIndex` (constructor creates fresh FAISS index)
     - **`beforeCreate()`**: Calls `prepareIndex()` → **trains** from local documents
     - **`fillIndex()`**: **Populates** the index with all existing documents
   - After local success, invokes the callback → replicates `CreateIndex` operation (with the **original** info, no `trainedData`) to the replicated log

5. **Follower receives `CreateIndex` via replicated log**:
   - `ApplyEntriesHandler::applyDataDefinitionEntry(CreateIndex)`
   - → `DocumentStateTransactionHandler::applyOp(CreateIndex)`
   - → `DocumentStateShardHandler::ensureIndex()`
   - → `MaintenanceActionExecutor::executeCreateIndex()`
   - → `RocksDBCollection::createIndex()`:
     - **Step 2**: Instantiates `RocksDBVectorIndex` without `trainedData`
     - **`beforeCreate()`** → `prepareIndex()` → **trains from local documents** (which already exist)
     - **`fillIndex()`** → **populates** the index
   - ✅ **This works** because documents arrived before the index creation operation.

### Key Files (Path A)

| File | Function | Role |
|------|----------|------|
| `arangod/Cluster/EnsureIndex.cpp` | `ensureIndexReplication2()` | Entry point for leader index creation |
| `arangod/Replication2/.../DocumentLeaderState.cpp` | `createIndex()` | Leader creates locally + replicates |
| `arangod/RocksDBEngine/RocksDBCollection.cpp` | `createIndex()` | Core creation: instantiate → train → fill → replicate |
| `arangod/Replication2/.../DocumentStateTransactionHandler.cpp` | `applyOp(CreateIndex)` | Follower receives from log |
| `arangod/Replication2/.../MaintenanceActionExecutor.cpp` | `executeCreateIndex()` | Follower executes creation |
| `arangod/RocksDBEngine/RocksDBVectorIndex.cpp` | `prepareIndex()` | Trains the FAISS index from documents |

---

## Path B: Increase `replicationFactor` (New Follower Joins via Snapshot)

This is the **buggy path** when a new DBServer is added as a follower (e.g., `replicationFactor` changed from 1→2 or 2→3).

### Flow

1. **Supervision detects `replicationFactor` mismatch**:
   - `CollectionGroupSupervision::checkAssociatedReplicatedLogs()` compares `target.replicationFactor` with `log.target.participants.size()`
   - Creates `AddParticipantToLog` action → adds participant to Plan

2. **Leader updates participant config**:
   - `LogLeader::updateParticipantsConfig()` adds a new `FollowerInfo`
   - Triggers async replication to the new follower

3. **New follower acquires a snapshot** (`DocumentFollowerState::acquireSnapshot()`):
   - Drops all local shards
   - Requests snapshot from leader
   - Receives batches of operations

4. **Snapshot serializes shard with indexes using `Serialization::Inventory`**:
   ```cpp
   // DocumentStateSnapshot.cpp:278-285
   properties = shard->toVelocyPackIgnore(
       { StaticStrings::DataSourceId, StaticStrings::DataSourceName,
         StaticStrings::DataSourceGuid, StaticStrings::ObjectId },
       LogicalDataSource::Serialization::Inventory);
   ```

5. **`Inventory` serialization does NOT include `trainedData`**:
   In `LogicalCollection::appendVPack()`, the `Inventory` context results in empty index flags:
   ```cpp
   // LogicalCollection.cpp:787-796
   auto indexFlags = Index::makeFlags();  // ← empty, no Internals flag
   if (forPersistence) {
     indexFlags = Index::makeFlags(Index::Serialize::Internals);
   }
   if (forMaintance) {
     indexFlags = Index::makeFlags(Index::Serialize::Internals,
                                   Index::Serialize::Maintenance);
   }
   ```
   And `RocksDBVectorIndex::toVelocyPack` requires `Internals` to emit `trainedData`:
   ```cpp
   // RocksDBVectorIndex.cpp:202-206
   if (_trainedData && Index::hasFlag(flags, Index::Serialize::Internals) &&
       !Index::hasFlag(flags, Index::Serialize::Maintenance)) {
     builder.add(VPackValue("trainedData"));
     velocypack::serialize(builder, *_trainedData);
   }
   ```
   **Result: `trainedData` is stripped out of the snapshot.**

6. **Follower creates shard from snapshot** (`CreateShard` operation):
   - `MaintenanceActionExecutor::executeCreateCollection()` → `Collections::createShard()` → `vocbase.createCollections()` → `LogicalCollection` constructor → **`prepareIndexes()`**
   - `RocksDBIndexFactory::prepareIndexes()` calls `prepareIndexFromSlice()` → `VectorIndexFactory::instantiate()` → `RocksDBVectorIndex` constructor
   - No `trainedData` in slice → creates a **fresh, untrained** FAISS index
   - **`prepareIndexes()` does NOT call `beforeCreate()` or `fillIndex()`** — it only instantiates index objects

7. **Documents arrive in subsequent snapshot batches**:
   - Applied via `transactionHandler->applyEntry(Insert)`
   - Each insert calls `RocksDBVectorIndex::insert()` which does:
     ```cpp
     // RocksDBVectorIndex.cpp:361-362
     TRI_ASSERT(_faissIndex->quantizer != nullptr);
     _faissIndex->quantizer->assign(1, input.data(), &listId);
     ```
   - The quantizer on an **untrained** IVF index has **zero centroids**
   - ❌ **This will crash or produce garbage results**

8. **After snapshot, replicated log replay cannot fix this**:
   - If the original `CreateIndex` operation is already committed (before the snapshot point), it is **not replayed** — it's covered by the snapshot.
   - If it IS replayed, `RocksDBCollection::createIndex()` Step 1 **finds the existing index** (created via `prepareIndexes()`) and **returns early without training or filling**:
     ```cpp
     // RocksDBCollection.cpp:485-499
     if (auto existingIdx = findIndex(info, _indexes); existingIdx != nullptr) {
       created = false;
       co_return existingIdx;  // ← returns untrained index as-is
     }
     ```

### Key Files (Path B)

| File | Function | Role |
|------|----------|------|
| `arangod/Replication2/Supervision/CollectionGroupSupervision.cpp` | `checkAssociatedReplicatedLogs()` | Detects replicationFactor mismatch |
| `arangod/Replication2/.../DocumentFollowerState.cpp` | `acquireSnapshot()` | New follower requests snapshot |
| `arangod/Replication2/.../DocumentStateSnapshot.cpp` | `generateBatch()` | Leader serializes shard with `Inventory` flags |
| `arangod/VocBase/LogicalCollection.cpp` | `appendVPack()` | Index serialization — `Inventory` omits `trainedData` |
| `arangod/RocksDBEngine/RocksDBVectorIndex.cpp` | `toVelocyPack()` | `trainedData` gated behind `Serialize::Internals` |
| `arangod/RocksDBEngine/RocksDBIndexFactory.cpp` | `prepareIndexes()` | Instantiates indexes — no training, no filling |
| `arangod/VocBase/Methods/Collections.cpp` | `createShard()` | Creates shard from snapshot properties |

---

## Summary Comparison

| Aspect | Path A (create index later) | Path B (increase replicationFactor) |
|--------|----------------------------|--------------------------------------|
| How follower gets index | `CreateIndex` replicated log entry | `CreateShard` in snapshot (includes index defs) |
| Index creation function | `RocksDBCollection::createIndex()` | `prepareIndexes()` → `prepareIndexFromSlice()` |
| `trainedData` included? | No (original coordinator info) | No (`Inventory` serialization) |
| `beforeCreate()` called? | **Yes** → trains from local docs | **No** |
| `fillIndex()` called? | **Yes** → populates index | **No** |
| Documents present at creation? | Yes (arrived before CreateIndex) | Arrive **after** index instantiation |
| Result | ✅ Index trained + filled correctly | ❌ **Untrained index, crashes on insert** |

---

## Root Cause

The snapshot path creates the vector index via `prepareIndexes()` (which only instantiates the index object) rather than through `createIndex()` (which also trains and fills it). Additionally, the `Serialization::Inventory` mode used for snapshots does not include `trainedData`, so the follower cannot reconstruct the trained state of the FAISS index.

## Possible Fix Directions

1. **Include `trainedData` in snapshot serialization**: Change snapshot creation to use `Serialization::Persistence` (or at minimum set the `Serialize::Internals` flag) so that `trainedData` is included. This would allow the follower to reconstruct a trained vector index from the snapshot.

2. **Trigger training after snapshot**: After snapshot transfer completes, detect untrained vector indexes and run `beforeCreate()` + `fillIndex()` on them.

3. **Don't include vector indexes in `CreateShard` snapshot properties**: Instead, let the follower discover and create them via replicated log replay or a separate maintenance action that goes through the full `createIndex()` path.
