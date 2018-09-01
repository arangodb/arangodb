// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

import org.rocksdb.*;

import static java.nio.charset.StandardCharsets.UTF_8;

/**
 * Demonstrates using Transactions on an OptimisticTransactionDB with
 * varying isolation guarantees
 */
public class OptimisticTransactionSample {
  private static final String dbPath = "/tmp/rocksdb_optimistic_transaction_example";

  public static final void main(final String args[]) throws RocksDBException {

    try(final Options options = new Options()
        .setCreateIfMissing(true);
        final OptimisticTransactionDB txnDb =
            OptimisticTransactionDB.open(options, dbPath)) {

      try (final WriteOptions writeOptions = new WriteOptions();
           final ReadOptions readOptions = new ReadOptions()) {

        ////////////////////////////////////////////////////////
        //
        // Simple OptimisticTransaction Example ("Read Committed")
        //
        ////////////////////////////////////////////////////////
        readCommitted(txnDb, writeOptions, readOptions);


        ////////////////////////////////////////////////////////
        //
        // "Repeatable Read" (Snapshot Isolation) Example
        //   -- Using a single Snapshot
        //
        ////////////////////////////////////////////////////////
        repeatableRead(txnDb, writeOptions, readOptions);


        ////////////////////////////////////////////////////////
        //
        // "Read Committed" (Monotonic Atomic Views) Example
        //   --Using multiple Snapshots
        //
        ////////////////////////////////////////////////////////
        readCommitted_monotonicAtomicViews(txnDb, writeOptions, readOptions);
      }
    }
  }

  /**
   * Demonstrates "Read Committed" isolation
   */
  private static void readCommitted(final OptimisticTransactionDB txnDb,
      final WriteOptions writeOptions, final ReadOptions readOptions)
      throws RocksDBException {
    final byte key1[] = "abc".getBytes(UTF_8);
    final byte value1[] = "def".getBytes(UTF_8);

    final byte key2[] = "xyz".getBytes(UTF_8);
    final byte value2[] = "zzz".getBytes(UTF_8);

    // Start a transaction
    try(final Transaction txn = txnDb.beginTransaction(writeOptions)) {
      // Read a key in this transaction
      byte[] value = txn.get(readOptions, key1);
      assert(value == null);

      // Write a key in this transaction
      txn.put(key1, value1);

      // Read a key OUTSIDE this transaction. Does not affect txn.
      value = txnDb.get(readOptions, key1);
      assert(value == null);

      // Write a key OUTSIDE of this transaction.
      // Does not affect txn since this is an unrelated key.
      // If we wrote key 'abc' here, the transaction would fail to commit.
      txnDb.put(writeOptions, key2, value2);

      // Commit transaction
      txn.commit();
    }
  }

  /**
   * Demonstrates "Repeatable Read" (Snapshot Isolation) isolation
   */
  private static void repeatableRead(final OptimisticTransactionDB txnDb,
      final WriteOptions writeOptions, final ReadOptions readOptions)
      throws RocksDBException {

    final byte key1[] = "ghi".getBytes(UTF_8);
    final byte value1[] = "jkl".getBytes(UTF_8);

    // Set a snapshot at start of transaction by setting setSnapshot(true)
    try(final OptimisticTransactionOptions txnOptions =
            new OptimisticTransactionOptions().setSetSnapshot(true);
        final Transaction txn =
            txnDb.beginTransaction(writeOptions, txnOptions)) {

      final Snapshot snapshot = txn.getSnapshot();

      // Write a key OUTSIDE of transaction
      txnDb.put(writeOptions, key1, value1);

      // Read a key using the snapshot.
      readOptions.setSnapshot(snapshot);
      final byte[] value = txn.getForUpdate(readOptions, key1, true);
      assert(value == value1);

      try {
        // Attempt to commit transaction
        txn.commit();
        throw new IllegalStateException();
      } catch(final RocksDBException e) {
        // Transaction could not commit since the write outside of the txn
        // conflicted with the read!
        assert(e.getStatus().getCode() == Status.Code.Busy);
      }

      txn.rollback();
    } finally {
      // Clear snapshot from read options since it is no longer valid
      readOptions.setSnapshot(null);
    }
  }

  /**
   * Demonstrates "Read Committed" (Monotonic Atomic Views) isolation
   *
   * In this example, we set the snapshot multiple times.  This is probably
   * only necessary if you have very strict isolation requirements to
   * implement.
   */
  private static void readCommitted_monotonicAtomicViews(
      final OptimisticTransactionDB txnDb, final WriteOptions writeOptions,
      final ReadOptions readOptions) throws RocksDBException {

    final byte keyX[] = "x".getBytes(UTF_8);
    final byte valueX[] = "x".getBytes(UTF_8);

    final byte keyY[] = "y".getBytes(UTF_8);
    final byte valueY[] = "y".getBytes(UTF_8);

    try (final OptimisticTransactionOptions txnOptions =
             new OptimisticTransactionOptions().setSetSnapshot(true);
         final Transaction txn =
             txnDb.beginTransaction(writeOptions, txnOptions)) {

      // Do some reads and writes to key "x"
      Snapshot snapshot = txnDb.getSnapshot();
      readOptions.setSnapshot(snapshot);
      byte[] value = txn.get(readOptions, keyX);
      txn.put(valueX, valueX);

      // Do a write outside of the transaction to key "y"
      txnDb.put(writeOptions, keyY, valueY);

      // Set a new snapshot in the transaction
      txn.setSnapshot();
      snapshot = txnDb.getSnapshot();
      readOptions.setSnapshot(snapshot);

      // Do some reads and writes to key "y"
      // Since the snapshot was advanced, the write done outside of the
      // transaction does not conflict.
      value = txn.getForUpdate(readOptions, keyY, true);
      txn.put(keyY, valueY);

      // Commit.  Since the snapshot was advanced, the write done outside of the
      // transaction does not prevent this transaction from Committing.
      txn.commit();

    } finally {
      // Clear snapshot from read options since it is no longer valid
      readOptions.setSnapshot(null);
    }
  }
}
