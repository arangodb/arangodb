// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

package org.rocksdb;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.nio.charset.StandardCharsets;

import static org.assertj.core.api.Assertions.assertThat;

public class BlockBasedTableConfigTest {

  @ClassRule
  public static final RocksMemoryResource rocksMemoryResource =
      new RocksMemoryResource();

  @Rule public TemporaryFolder dbFolder = new TemporaryFolder();

  @Test
  public void noBlockCache() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setNoBlockCache(true);
    assertThat(blockBasedTableConfig.noBlockCache()).isTrue();
  }

  @Test
  public void blockCacheSize() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setBlockCacheSize(8 * 1024);
    assertThat(blockBasedTableConfig.blockCacheSize()).
        isEqualTo(8 * 1024);
  }

  @Test
  public void sharedBlockCache() throws RocksDBException {
    try (final Cache cache = new LRUCache(8 * 1024 * 1024);
         final Statistics statistics = new Statistics()) {
      for (int shard = 0; shard < 8; shard++) {
        try (final Options options =
                 new Options()
                     .setCreateIfMissing(true)
                     .setStatistics(statistics)
                     .setTableFormatConfig(new BlockBasedTableConfig().setBlockCache(cache));
             final RocksDB db =
                 RocksDB.open(options, dbFolder.getRoot().getAbsolutePath() + "/" + shard)) {
          final byte[] key = "some-key".getBytes(StandardCharsets.UTF_8);
          final byte[] value = "some-value".getBytes(StandardCharsets.UTF_8);

          db.put(key, value);
          db.flush(new FlushOptions());
          db.get(key);

          assertThat(statistics.getTickerCount(TickerType.BLOCK_CACHE_ADD)).isEqualTo(shard + 1);
        }
      }
    }
  }

  @Test
  public void blockSizeDeviation() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setBlockSizeDeviation(12);
    assertThat(blockBasedTableConfig.blockSizeDeviation()).
        isEqualTo(12);
  }

  @Test
  public void blockRestartInterval() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setBlockRestartInterval(15);
    assertThat(blockBasedTableConfig.blockRestartInterval()).
        isEqualTo(15);
  }

  @Test
  public void wholeKeyFiltering() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setWholeKeyFiltering(false);
    assertThat(blockBasedTableConfig.wholeKeyFiltering()).
        isFalse();
  }

  @Test
  public void cacheIndexAndFilterBlocks() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setCacheIndexAndFilterBlocks(true);
    assertThat(blockBasedTableConfig.cacheIndexAndFilterBlocks()).
        isTrue();

  }

  @Test
  public void cacheIndexAndFilterBlocksWithHighPriority() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setCacheIndexAndFilterBlocksWithHighPriority(true);
    assertThat(blockBasedTableConfig.cacheIndexAndFilterBlocksWithHighPriority()).
            isTrue();
  }

  @Test
  public void pinL0FilterAndIndexBlocksInCache() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setPinL0FilterAndIndexBlocksInCache(true);
    assertThat(blockBasedTableConfig.pinL0FilterAndIndexBlocksInCache()).
            isTrue();
  }

  @Test
  public void partitionFilters() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setPartitionFilters(true);
    assertThat(blockBasedTableConfig.partitionFilters()).
            isTrue();
  }

  @Test
  public void metadataBlockSize() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setMetadataBlockSize(1024);
    assertThat(blockBasedTableConfig.metadataBlockSize()).
            isEqualTo(1024);
  }

  @Test
  public void pinTopLevelIndexAndFilter() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setPinTopLevelIndexAndFilter(false);
    assertThat(blockBasedTableConfig.pinTopLevelIndexAndFilter()).
            isFalse();
  }

  @Test
  public void hashIndexAllowCollision() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setHashIndexAllowCollision(false);
    assertThat(blockBasedTableConfig.hashIndexAllowCollision()).
        isFalse();
  }

  @Test
  public void blockCacheCompressedSize() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setBlockCacheCompressedSize(40);
    assertThat(blockBasedTableConfig.blockCacheCompressedSize()).
        isEqualTo(40);
  }

  @Test
  public void checksumType() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    assertThat(ChecksumType.values().length).isEqualTo(3);
    assertThat(ChecksumType.valueOf("kxxHash")).
        isEqualTo(ChecksumType.kxxHash);
    blockBasedTableConfig.setChecksumType(ChecksumType.kNoChecksum);
    blockBasedTableConfig.setChecksumType(ChecksumType.kxxHash);
    assertThat(blockBasedTableConfig.checksumType().equals(
        ChecksumType.kxxHash));
  }

  @Test
  public void indexType() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    assertThat(IndexType.values().length).isEqualTo(3);
    blockBasedTableConfig.setIndexType(IndexType.kHashSearch);
    assertThat(blockBasedTableConfig.indexType().equals(
        IndexType.kHashSearch));
    assertThat(IndexType.valueOf("kBinarySearch")).isNotNull();
    blockBasedTableConfig.setIndexType(IndexType.valueOf("kBinarySearch"));
    assertThat(blockBasedTableConfig.indexType().equals(
        IndexType.kBinarySearch));
  }

  @Test
  public void blockCacheCompressedNumShardBits() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setBlockCacheCompressedNumShardBits(4);
    assertThat(blockBasedTableConfig.blockCacheCompressedNumShardBits()).
        isEqualTo(4);
  }

  @Test
  public void cacheNumShardBits() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setCacheNumShardBits(5);
    assertThat(blockBasedTableConfig.cacheNumShardBits()).
        isEqualTo(5);
  }

  @Test
  public void blockSize() {
    BlockBasedTableConfig blockBasedTableConfig = new BlockBasedTableConfig();
    blockBasedTableConfig.setBlockSize(10);
    assertThat(blockBasedTableConfig.blockSize()).isEqualTo(10);
  }


  @Test
  public void blockBasedTableWithFilter() {
    try(final Options options = new Options()
        .setTableFormatConfig(new BlockBasedTableConfig()
        .setFilter(new BloomFilter(10)))) {
      assertThat(options.tableFactoryName()).
          isEqualTo("BlockBasedTable");
    }
  }

  @Test
  public void blockBasedTableWithoutFilter() {
    try(final Options options = new Options().setTableFormatConfig(
        new BlockBasedTableConfig().setFilter(null))) {
      assertThat(options.tableFactoryName()).
          isEqualTo("BlockBasedTable");
    }
  }

  @Test
  public void blockBasedTableWithBlockCache() {
    try (final Options options = new Options().setTableFormatConfig(
             new BlockBasedTableConfig().setBlockCache(new LRUCache(17 * 1024 * 1024)))) {
      assertThat(options.tableFactoryName()).isEqualTo("BlockBasedTable");
    }
  }

  @Test
  public void blockBasedTableFormatVersion() {
    BlockBasedTableConfig config = new BlockBasedTableConfig();
    for (int version=0; version<=2; version++) {
      config.setFormatVersion(version);
      assertThat(config.formatVersion()).isEqualTo(version);
    }
  }

  @Test(expected = AssertionError.class)
  public void blockBasedTableFormatVersionFailNegative() {
    BlockBasedTableConfig config = new BlockBasedTableConfig();
    config.setFormatVersion(-1);
  }

  @Test(expected = AssertionError.class)
  public void blockBasedTableFormatVersionFailIllegalVersion() {
    BlockBasedTableConfig config = new BlockBasedTableConfig();
    config.setFormatVersion(3);
  }
}
