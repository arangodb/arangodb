// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

package org.rocksdb;

import java.util.Collection;
import java.util.List;

public interface ColumnFamilyOptionsInterface<T extends ColumnFamilyOptionsInterface<T>>
    extends AdvancedColumnFamilyOptionsInterface<T> {
  /**
   * The function recovers options to a previous version. Only 4.6 or later
   * versions are supported.
   *
   * @param majorVersion The major version to recover default values of options
   * @param minorVersion The minor version to recover default values of options
   * @return the instance of the current object.
   */
  T oldDefaults(int majorVersion, int minorVersion);

  /**
   * Use this if your DB is very small (like under 1GB) and you don't want to
   * spend lots of memory for memtables.
   *
   * @return the instance of the current object.
   */
  T optimizeForSmallDb();

  /**
   * Some functions that make it easier to optimize RocksDB
   * Use this if your DB is very small (like under 1GB) and you don't want to
   * spend lots of memory for memtables.
   *
   * @param cache An optional cache object is passed in to be used as the block cache
   * @return the instance of the current object.
   */
  T optimizeForSmallDb(Cache cache);

  /**
   * Use this if you don't need to keep the data sorted, i.e. you'll never use
   * an iterator, only Put() and Get() API calls
   *
   * @param blockCacheSizeMb Block cache size in MB
   * @return the instance of the current object.
   */
  T optimizeForPointLookup(long blockCacheSizeMb);

  /**
   * <p>Default values for some parameters in ColumnFamilyOptions are not
   * optimized for heavy workloads and big datasets, which means you might
   * observe write stalls under some conditions. As a starting point for tuning
   * RocksDB options, use the following for level style compaction.</p>
   *
   * <p>Make sure to also call IncreaseParallelism(), which will provide the
   * biggest performance gains.</p>
   * <p>Note: we might use more memory than memtable_memory_budget during high
   * write rate period</p>
   *
   * @return the instance of the current object.
   */
  T optimizeLevelStyleCompaction();

  /**
   * <p>Default values for some parameters in ColumnFamilyOptions are not
   * optimized for heavy workloads and big datasets, which means you might
   * observe write stalls under some conditions. As a starting point for tuning
   * RocksDB options, use the following for level style compaction.</p>
   *
   * <p>Make sure to also call IncreaseParallelism(), which will provide the
   * biggest performance gains.</p>
   * <p>Note: we might use more memory than memtable_memory_budget during high
   * write rate period</p>
   *
   * @param memtableMemoryBudget memory budget in bytes
   * @return the instance of the current object.
   */
  T optimizeLevelStyleCompaction(
      long memtableMemoryBudget);

  /**
   * <p>Default values for some parameters in ColumnFamilyOptions are not
   * optimized for heavy workloads and big datasets, which means you might
   * observe write stalls under some conditions. As a starting point for tuning
   * RocksDB options, use the following for universal style compaction.</p>
   *
   * <p>Universal style compaction is focused on reducing Write Amplification
   * Factor for big data sets, but increases Space Amplification.</p>
   *
   * <p>Make sure to also call IncreaseParallelism(), which will provide the
   * biggest performance gains.</p>
   *
   * <p>Note: we might use more memory than memtable_memory_budget during high
   * write rate period</p>
   *
   * @return the instance of the current object.
   */
  T optimizeUniversalStyleCompaction();

  /**
   * <p>Default values for some parameters in ColumnFamilyOptions are not
   * optimized for heavy workloads and big datasets, which means you might
   * observe write stalls under some conditions. As a starting point for tuning
   * RocksDB options, use the following for universal style compaction.</p>
   *
   * <p>Universal style compaction is focused on reducing Write Amplification
   * Factor for big data sets, but increases Space Amplification.</p>
   *
   * <p>Make sure to also call IncreaseParallelism(), which will provide the
   * biggest performance gains.</p>
   *
   * <p>Note: we might use more memory than memtable_memory_budget during high
   * write rate period</p>
   *
   * @param memtableMemoryBudget memory budget in bytes
   * @return the instance of the current object.
   */
  T optimizeUniversalStyleCompaction(
      long memtableMemoryBudget);

  /**
   * Set {@link BuiltinComparator} to be used with RocksDB.
   *
   * Note: Comparator can be set once upon database creation.
   *
   * Default: BytewiseComparator.
   * @param builtinComparator a {@link BuiltinComparator} type.
   * @return the instance of the current object.
   */
  T setComparator(
      BuiltinComparator builtinComparator);

  /**
   * Use the specified comparator for key ordering.
   *
   * Comparator should not be disposed before options instances using this comparator is
   * disposed. If dispose() function is not called, then comparator object will be
   * GC'd automatically.
   *
   * Comparator instance can be re-used in multiple options instances.
   *
   * @param comparator java instance.
   * @return the instance of the current object.
   */
  T setComparator(
      AbstractComparator comparator);

  /**
   * <p>Set the merge operator to be used for merging two merge operands
   * of the same key. The merge function is invoked during
   * compaction and at lookup time, if multiple key/value pairs belonging
   * to the same key are found in the database.</p>
   *
   * @param name the name of the merge function, as defined by
   * the MergeOperators factory (see utilities/MergeOperators.h)
   * The merge function is specified by name and must be one of the
   * standard merge operators provided by RocksDB. The available
   * operators are "put", "uint64add", "stringappend" and "stringappendtest".
   * @return the instance of the current object.
   */
  T setMergeOperatorName(String name);

  /**
   * <p>Set the merge operator to be used for merging two different key/value
   * pairs that share the same key. The merge function is invoked during
   * compaction and at lookup time, if multiple key/value pairs belonging
   * to the same key are found in the database.</p>
   *
   * @param mergeOperator {@link MergeOperator} instance.
   * @return the instance of the current object.
   */
  T setMergeOperator(MergeOperator mergeOperator);

  /**
   * A single CompactionFilter instance to call into during compaction.
   * Allows an application to modify/delete a key-value during background
   * compaction.
   *
   * If the client requires a new compaction filter to be used for different
   * compaction runs, it can specify call
   * {@link #setCompactionFilterFactory(AbstractCompactionFilterFactory)}
   * instead.
   *
   * The client should specify only set one of the two.
   * {@link #setCompactionFilter(AbstractCompactionFilter)} takes precedence
   * over {@link #setCompactionFilterFactory(AbstractCompactionFilterFactory)}
   * if the client specifies both.
   *
   * If multithreaded compaction is being used, the supplied CompactionFilter
   * instance may be used from different threads concurrently and so should be thread-safe.
   *
   * @param compactionFilter {@link AbstractCompactionFilter} instance.
   * @return the instance of the current object.
   */
  T setCompactionFilter(
          final AbstractCompactionFilter<? extends AbstractSlice<?>> compactionFilter);

  /**
   * Accessor for the CompactionFilter instance in use.
   *
   * @return  Reference to the CompactionFilter, or null if one hasn't been set.
   */
  AbstractCompactionFilter<? extends AbstractSlice<?>> compactionFilter();

  /**
   * This is a factory that provides {@link AbstractCompactionFilter} objects
   * which allow an application to modify/delete a key-value during background
   * compaction.
   *
   * A new filter will be created on each compaction run.  If multithreaded
   * compaction is being used, each created CompactionFilter will only be used
   * from a single thread and so does not need to be thread-safe.
   *
   * @param compactionFilterFactory {@link AbstractCompactionFilterFactory} instance.
   * @return the instance of the current object.
   */
  T setCompactionFilterFactory(
          final AbstractCompactionFilterFactory<? extends AbstractCompactionFilter<?>>
                  compactionFilterFactory);

  /**
   * Accessor for the CompactionFilterFactory instance in use.
   *
   * @return  Reference to the CompactionFilterFactory, or null if one hasn't been set.
   */
  AbstractCompactionFilterFactory<? extends AbstractCompactionFilter<?>> compactionFilterFactory();

  /**
   * This prefix-extractor uses the first n bytes of a key as its prefix.
   *
   * In some hash-based memtable representation such as HashLinkedList
   * and HashSkipList, prefixes are used to partition the keys into
   * several buckets.  Prefix extractor is used to specify how to
   * extract the prefix given a key.
   *
   * @param n use the first n bytes of a key as its prefix.
   * @return the reference to the current option.
   */
  T useFixedLengthPrefixExtractor(int n);

  /**
   * Same as fixed length prefix extractor, except that when slice is
   * shorter than the fixed length, it will use the full key.
   *
   * @param n use the first n bytes of a key as its prefix.
   * @return the reference to the current option.
   */
  T useCappedPrefixExtractor(int n);

  /**
   * Number of files to trigger level-0 compaction. A value &lt; 0 means that
   * level-0 compaction will not be triggered by number of files at all.
   * Default: 4
   *
   * @param numFiles the number of files in level-0 to trigger compaction.
   * @return the reference to the current option.
   */
  T setLevelZeroFileNumCompactionTrigger(
      int numFiles);

  /**
   * The number of files in level 0 to trigger compaction from level-0 to
   * level-1.  A value &lt; 0 means that level-0 compaction will not be
   * triggered by number of files at all.
   * Default: 4
   *
   * @return the number of files in level 0 to trigger compaction.
   */
  int levelZeroFileNumCompactionTrigger();

  /**
   * Soft limit on number of level-0 files. We start slowing down writes at this
   * point. A value &lt; 0 means that no writing slow down will be triggered by
   * number of files in level-0.
   *
   * @param numFiles soft limit on number of level-0 files.
   * @return the reference to the current option.
   */
  T setLevelZeroSlowdownWritesTrigger(
      int numFiles);

  /**
   * Soft limit on the number of level-0 files. We start slowing down writes
   * at this point. A value &lt; 0 means that no writing slow down will be
   * triggered by number of files in level-0.
   *
   * @return the soft limit on the number of level-0 files.
   */
  int levelZeroSlowdownWritesTrigger();

  /**
   * Maximum number of level-0 files.  We stop writes at this point.
   *
   * @param numFiles the hard limit of the number of level-0 files.
   * @return the reference to the current option.
   */
  T setLevelZeroStopWritesTrigger(int numFiles);

  /**
   * Maximum number of level-0 files.  We stop writes at this point.
   *
   * @return the hard limit of the number of level-0 file.
   */
  int levelZeroStopWritesTrigger();

  /**
   * The ratio between the total size of level-(L+1) files and the total
   * size of level-L files for all L.
   * DEFAULT: 10
   *
   * @param multiplier the ratio between the total size of level-(L+1)
   *     files and the total size of level-L files for all L.
   * @return the reference to the current option.
   */
  T setMaxBytesForLevelMultiplier(
      double multiplier);

  /**
   * The ratio between the total size of level-(L+1) files and the total
   * size of level-L files for all L.
   * DEFAULT: 10
   *
   * @return the ratio between the total size of level-(L+1) files and
   *     the total size of level-L files for all L.
   */
  double maxBytesForLevelMultiplier();

  /**
   * FIFO compaction option.
   * The oldest table file will be deleted
   * once the sum of table files reaches this size.
   * The default value is 1GB (1 * 1024 * 1024 * 1024).
   *
   * @param maxTableFilesSize the size limit of the total sum of table files.
   * @return the instance of the current object.
   */
  T setMaxTableFilesSizeFIFO(
      long maxTableFilesSize);

  /**
   * FIFO compaction option.
   * The oldest table file will be deleted
   * once the sum of table files reaches this size.
   * The default value is 1GB (1 * 1024 * 1024 * 1024).
   *
   * @return the size limit of the total sum of table files.
   */
  long maxTableFilesSizeFIFO();

  /**
   * Get the config for mem-table.
   *
   * @return the mem-table config.
   */
  MemTableConfig memTableConfig();

  /**
   * Set the config for mem-table.
   *
   * @param memTableConfig the mem-table config.
   * @return the instance of the current object.
   * @throws java.lang.IllegalArgumentException thrown on 32-Bit platforms
   *   while overflowing the underlying platform specific value.
   */
  T setMemTableConfig(MemTableConfig memTableConfig);

  /**
   * Returns the name of the current mem table representation.
   * Memtable format can be set using setTableFormatConfig.
   *
   * @return the name of the currently-used memtable factory.
   * @see #setTableFormatConfig(org.rocksdb.TableFormatConfig)
   */
  String memTableFactoryName();

  /**
   * Get the config for table format.
   *
   * @return the table format config.
   */
  TableFormatConfig tableFormatConfig();

  /**
   * Set the config for table format.
   *
   * @param config the table format config.
   * @return the reference of the current options.
   */
  T setTableFormatConfig(TableFormatConfig config);

  /**
   * @return the name of the currently used table factory.
   */
  String tableFactoryName();

  /**
   * A list of paths where SST files for this column family
   * can be put into, with its target size. Similar to db_paths,
   * newer data is placed into paths specified earlier in the
   * vector while older data gradually moves to paths specified
   * later in the vector.
   * Note that, if a path is supplied to multiple column
   * families, it would have files and total size from all
   * the column families combined. User should provision for the
   * total size(from all the column families) in such cases.
   *
   * If left empty, db_paths will be used.
   * Default: empty
   *
   * @param paths collection of paths for SST files.
   * @return the reference of the current options.
   */
  T setCfPaths(final Collection<DbPath> paths);

  /**
   * @return collection of paths for SST files.
   */
  List<DbPath> cfPaths();

  /**
   * Compression algorithm that will be used for the bottommost level that
   * contain files. If level-compaction is used, this option will only affect
   * levels after base level.
   *
   * Default: {@link CompressionType#DISABLE_COMPRESSION_OPTION}
   *
   * @param bottommostCompressionType  The compression type to use for the
   *     bottommost level
   *
   * @return the reference of the current options.
   */
  T setBottommostCompressionType(
      final CompressionType bottommostCompressionType);

  /**
   * Compression algorithm that will be used for the bottommost level that
   * contain files. If level-compaction is used, this option will only affect
   * levels after base level.
   *
   * Default: {@link CompressionType#DISABLE_COMPRESSION_OPTION}
   *
   * @return The compression type used for the bottommost level
   */
  CompressionType bottommostCompressionType();

  /**
   * Set the options for compression algorithms used by
   * {@link #bottommostCompressionType()} if it is enabled.
   *
   * To enable it, please see the definition of
   * {@link CompressionOptions}.
   *
   * @param compressionOptions the bottom most compression options.
   *
   * @return the reference of the current options.
   */
  T setBottommostCompressionOptions(
      final CompressionOptions compressionOptions);

  /**
   * Get the bottom most compression options.
   *
   * See {@link #setBottommostCompressionOptions(CompressionOptions)}.
   *
   * @return the bottom most compression options.
   */
  CompressionOptions bottommostCompressionOptions();

  /**
   * Set the different options for compression algorithms
   *
   * @param compressionOptions The compression options
   *
   * @return the reference of the current options.
   */
  T setCompressionOptions(
      CompressionOptions compressionOptions);

  /**
   * Get the different options for compression algorithms
   *
   * @return The compression options
   */
  CompressionOptions compressionOptions();

  /**
   * If non-nullptr, use the specified factory for a function to determine the
   * partitioning of sst files. This helps compaction to split the files
   * on interesting boundaries (key prefixes) to make propagation of sst
   * files less write amplifying (covering the whole key space).
   *
   * Default: nullptr
   *
   * @param factory The factory reference
   * @return the reference of the current options.
   */
  @Experimental("Caution: this option is experimental")
  T setSstPartitionerFactory(SstPartitionerFactory factory);

  /**
   * Get SST partitioner factory
   *
   * @return SST partitioner factory
   */
  @Experimental("Caution: this option is experimental")
  SstPartitionerFactory sstPartitionerFactory();

  /**
   * Compaction concurrent thread limiter for the column family.
   * If non-nullptr, use given concurrent thread limiter to control
   * the max outstanding compaction tasks. Limiter can be shared with
   * multiple column families across db instances.
   *
   * @param concurrentTaskLimiter The compaction thread limiter.
   * @return the reference of the current options.
   */
  T setCompactionThreadLimiter(ConcurrentTaskLimiter concurrentTaskLimiter);

  /**
   * Get compaction thread limiter
   *
   * @return Compaction thread limiter
   */
  ConcurrentTaskLimiter compactionThreadLimiter();

  /**
   * Default memtable memory budget used with the following methods:
   *
   * <ol>
   *   <li>{@link #optimizeLevelStyleCompaction()}</li>
   *   <li>{@link #optimizeUniversalStyleCompaction()}</li>
   * </ol>
   */
  long DEFAULT_COMPACTION_MEMTABLE_MEMORY_BUDGET = 512 * 1024 * 1024;
}
