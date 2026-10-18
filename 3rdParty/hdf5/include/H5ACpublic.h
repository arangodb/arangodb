/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*-------------------------------------------------------------------------
 *
 * Created:             H5ACpublic.h
 *
 * Purpose:             Public include file for cache functions
 *
 *-------------------------------------------------------------------------
 */
#ifndef H5ACpublic_H
#define H5ACpublic_H

#include "H5public.h"  /* Generic Functions                        */
#include "H5Cpublic.h" /* Cache                                    */

/****************************************************************************
 *
 * structure H5AC_cache_config_t
 *
 * H5AC_cache_config_t is a public structure intended for use in public APIs.
 * At least in its initial incarnation, it is basically a copy of struct
 * H5C_auto_size_ctl_t, minus the report_fcn field, and plus the
 * dirty_bytes_threshold field.
 *
 * The report_fcn field is omitted, as including it would require us to
 * make H5C_t structure public.
 *
 * The dirty_bytes_threshold field does not appear in H5C_auto_size_ctl_t,
 * as synchronization between caches on different processes is handled at
 * the H5AC level, not at the level of H5C.  Note however that there is
 * considerable interaction between this value and the other fields in this
 * structure.
 *
 * Similarly, the open_trace_file, close_trace_file, and trace_file_name
 * fields do not appear in H5C_auto_size_ctl_t, as most trace file
 * issues are handled at the H5AC level.  The one exception is storage of
 * the pointer to the trace file, which is handled by H5C.
 *
 * The structure is in H5ACpublic.h as we may wish to allow different
 * configuration options for metadata and raw data caches.
 *
 * The fields of the structure are discussed individually below:
 *
 * version: Integer field containing the version number of this version
 *      of the H5AC_cache_config_t structure.  Any instance of
 *      H5AC_cache_config_t passed to the cache must have a known
 *      version number, or an error will be flagged.
 *
 * rpt_fcn_enabled: Boolean field used to enable and disable the default
 *    reporting function.  This function is invoked every time the
 *    automatic cache resize code is run, and reports on its activities.
 *
 *    This is a debugging function, and should normally be turned off.
 *
 * open_trace_file: Boolean field indicating whether the trace_file_name
 *     field should be used to open a trace file for the cache.
 *
 *      *** DEPRECATED *** Use H5Fstart/stop logging functions instead
 *
 *     The trace file is a debugging feature that allow the capture of
 *     top level metadata cache requests for purposes of debugging and/or
 *     optimization.  This field should normally be set to false, as
 *     trace file collection imposes considerable overhead.
 *
 *     This field should only be set to true when the trace_file_name
 *     contains the full path of the desired trace file, and either
 *     there is no open trace file on the cache, or the close_trace_file
 *     field is also true.
 *
 * close_trace_file: Boolean field indicating whether the current trace
 *     file (if any) should be closed.
 *
 *      *** DEPRECATED *** Use H5Fstart/stop logging functions instead
 *
 *     See the above comments on the open_trace_file field.  This field
 *     should be set to false unless there is an open trace file on the
 *     cache that you wish to close.
 *
 * trace_file_name: Full path of the trace file to be opened if the
 *     open_trace_file field is true.
 *
 *      *** DEPRECATED *** Use H5Fstart/stop logging functions instead
 *
 *     In the parallel case, an ascii representation of the mpi rank of
 *     the process will be appended to the file name to yield a unique
 *     trace file name for each process.
 *
 *     The length of the path must not exceed H5AC__MAX_TRACE_FILE_NAME_LEN
 *     characters.
 *
 * evictions_enabled:  Boolean field used to either report the current
 *     evictions enabled status of the cache, or to set the cache's
 *    evictions enabled status.
 *
 *     In general, the metadata cache should always be allowed to
 *     evict entries.  However, in some cases it is advantageous to
 *     disable evictions briefly, and thereby postpone metadata
 *     writes.  However, this must be done with care, as the cache
 *     can grow quickly.  If you do this, re-enable evictions as
 *     soon as possible and monitor cache size.
 *
 *     At present, evictions can only be disabled if automatic
 *     cache resizing is also disabled (that is, ( incr_mode ==
 *    H5C_incr__off ) && ( decr_mode == H5C_decr__off )).  There
 *    is no logical reason why this should be so, but it simplifies
 *    implementation and testing, and I can't think of any reason
 *    why it would be desirable.  If you can think of one, I'll
 *    revisit the issue.
 *
 * set_initial_size: Boolean flag indicating whether the size of the
 *      initial size of the cache is to be set to the value given in
 *      the initial_size field.  If set_initial_size is false, the
 *      initial_size field is ignored.
 *
 * initial_size: If enabled, this field contain the size the cache is
 *      to be set to upon receipt of this structure.  Needless to say,
 *      initial_size must lie in the closed interval [min_size, max_size].
 *
 * min_clean_fraction: double in the range 0 to 1 indicating the fraction
 *      of the cache that is to be kept clean.  This field is only used
 *      in parallel mode.  Typical values are 0.1 to 0.5.
 *
 * max_size: Maximum size to which the cache can be adjusted.  The
 *      supplied value must fall in the closed interval
 *      [MIN_MAX_CACHE_SIZE, MAX_MAX_CACHE_SIZE].  Also, max_size must
 *      be greater than or equal to min_size.
 *
 * min_size: Minimum size to which the cache can be adjusted.  The
 *      supplied value must fall in the closed interval
 *      [H5C__MIN_MAX_CACHE_SIZE, H5C__MAX_MAX_CACHE_SIZE].  Also, min_size
 *      must be less than or equal to max_size.
 *
 * epoch_length: Number of accesses on the cache over which to collect
 *      hit rate stats before running the automatic cache resize code,
 *      if it is enabled.
 *
 *      At the end of an epoch, we discard prior hit rate data and start
 *      collecting afresh.  The epoch_length must lie in the closed
 *      interval [H5C__MIN_AR_EPOCH_LENGTH, H5C__MAX_AR_EPOCH_LENGTH].
 *
 *
 * Cache size increase control fields:
 *
 * incr_mode: Instance of the H5C_cache_incr_mode enumerated type whose
 *      value indicates how we determine whether the cache size should be
 *      increased.  At present there are two possible values:
 *
 *      H5C_incr__off:  Don't attempt to increase the size of the cache
 *              automatically.
 *
 *              When this increment mode is selected, the remaining fields
 *              in the cache size increase section ar ignored.
 *
 *      H5C_incr__threshold: Attempt to increase the size of the cache
 *              whenever the average hit rate over the last epoch drops
 *              below the value supplied in the lower_hr_threshold
 *              field.
 *
 *              Note that this attempt will fail if the cache is already
 *              at its maximum size, or if the cache is not already using
 *              all available space.
 *
 *      Note that you must set decr_mode to H5C_incr__off if you
 *      disable metadata cache entry evictions.
 *
 * lower_hr_threshold: Lower hit rate threshold.  If the increment mode
 *      (incr_mode) is H5C_incr__threshold and the hit rate drops below the
 *      value supplied in this field in an epoch, increment the cache size by
 *      size_increment.  Note that cache size may not be incremented above
 *      max_size, and that the increment may be further restricted by the
 *      max_increment field if it is enabled.
 *
 *      When enabled, this field must contain a value in the range [0.0, 1.0].
 *      Depending on the incr_mode selected, it may also have to be less than
 *      upper_hr_threshold.
 *
 * increment:  Double containing the multiplier used to derive the new
 *      cache size from the old if a cache size increment is triggered.
 *      The increment must be greater than 1.0, and should not exceed 2.0.
 *
 *      The new cache size is obtained my multiplying the current max cache
 *      size by the increment, and then clamping to max_size and to stay
 *      within the max_increment as necessary.
 *
 * apply_max_increment:  Boolean flag indicating whether the max_increment
 *      field should be used to limit the maximum cache size increment.
 *
 * max_increment: If enabled by the apply_max_increment field described
 *      above, this field contains the maximum number of bytes by which the
 *      cache size can be increased in a single re-size.
 *
 * flash_incr_mode:  Instance of the H5C_cache_flash_incr_mode enumerated
 *      type whose value indicates whether and by which algorithm we should
 *      make flash increases in the size of the cache to accommodate insertion
 *      of large entries and large increases in the size of a single entry.
 *
 *      The addition of the flash increment mode was occasioned by performance
 *      problems that appear when a local heap is increased to a size in excess
 *      of the current cache size.  While the existing re-size code dealt with
 *      this eventually, performance was very bad for the remainder of the
 *      epoch.
 *
 *      At present, there are two possible values for the flash_incr_mode:
 *
 *      H5C_flash_incr__off:  Don't perform flash increases in the size of
 *              the cache.
 *
 *      H5C_flash_incr__add_space:  Let x be either the size of a newly
 *              newly inserted entry, or the number of bytes by which the
 *              size of an existing entry has been increased.
 *
 *              If
 *                      x > flash_threshold * current max cache size,
 *
 *              increase the current maximum cache size by x * flash_multiple
 *              less any free space in the cache, and star a new epoch.  For
 *              now at least, pay no attention to the maximum increment.
 *
 *      In both of the above cases, the flash increment pays no attention to
 *      the maximum increment (at least in this first incarnation), but DOES
 *      stay within max_size.
 *
 *      With a little thought, it should be obvious that the above flash
 *      cache size increase algorithm is not sufficient for all circumstances
 *      -- for example, suppose the user round robins through
 *      (1/flash_threshold) +1 groups, adding one data set to each on each
 *      pass.  Then all will increase in size at about the same time, requiring
 *      the max cache size to at least double to maintain acceptable
 *      performance, however the above flash increment algorithm will not be
 *      triggered.
 *
 *      Hopefully, the add space algorithms detailed above will be sufficient
 *      for the performance problems encountered to date.  However, we should
 *      expect to revisit the issue.
 *
 * flash_multiple: Double containing the multiple described above in the
 *      H5C_flash_incr__add_space section of the discussion of the
 *      flash_incr_mode section.  This field is ignored unless flash_incr_mode
 *      is H5C_flash_incr__add_space.
 *
 * flash_threshold: Double containing the factor by which current max cache
 *      size is multiplied to obtain the size threshold for the add_space flash
 *      increment algorithm.  The field is ignored unless flash_incr_mode is
 *      H5C_flash_incr__add_space.
 *
 *
 * Cache size decrease control fields:
 *
 * decr_mode: Instance of the H5C_cache_decr_mode enumerated type whose
 *      value indicates how we determine whether the cache size should be
 *      decreased.  At present there are four possibilities.
 *
 *      H5C_decr__off:  Don't attempt to decrease the size of the cache
 *              automatically.
 *
 *              When this increment mode is selected, the remaining fields
 *              in the cache size decrease section are ignored.
 *
 *      H5C_decr__threshold: Attempt to decrease the size of the cache
 *              whenever the average hit rate over the last epoch rises
 *              above the value supplied in the upper_hr_threshold
 *              field.
 *
 *      H5C_decr__age_out:  At the end of each epoch, search the cache for
 *              entries that have not been accessed for at least the number
 *              of epochs specified in the epochs_before_eviction field, and
 *              evict these entries.  Conceptually, the maximum cache size
 *              is then decreased to match the new actual cache size.  However,
 *              this reduction may be modified by the min_size, the
 *              max_decrement, and/or the empty_reserve.
 *
 *      H5C_decr__age_out_with_threshold:  Same as age_out, but we only
 *              attempt to reduce the cache size when the hit rate observed
 *              over the last epoch exceeds the value provided in the
 *              upper_hr_threshold field.
 *
 *      Note that you must set decr_mode to H5C_decr__off if you
 *      disable metadata cache entry evictions.
 *
 * upper_hr_threshold: Upper hit rate threshold.  The use of this field
 *      varies according to the current decr_mode:
 *
 *      H5C_decr__off or H5C_decr__age_out:  The value of this field is
 *              ignored.
 *
 *      H5C_decr__threshold:  If the hit rate exceeds this threshold in any
 *              epoch, attempt to decrement the cache size by size_decrement.
 *
 *              Note that cache size may not be decremented below min_size.
 *
 *              Note also that if the upper_threshold is 1.0, the cache size
 *              will never be reduced.
 *
 *      H5C_decr__age_out_with_threshold:  If the hit rate exceeds this
 *              threshold in any epoch, attempt to reduce the cache size
 *              by evicting entries that have not been accessed for more
 *              than the specified number of epochs.
 *
 * decrement: This field is only used when the decr_mode is
 *      H5C_decr__threshold.
 *
 *      The field is a double containing the multiplier used to derive the
 *      new cache size from the old if a cache size decrement is triggered.
 *      The decrement must be in the range 0.0 (in which case the cache will
 *      try to contract to its minimum size) to 1.0 (in which case the
 *      cache will never shrink).
 *
 * apply_max_decrement:  Boolean flag used to determine whether decrements
 *      in cache size are to be limited by the max_decrement field.
 *
 * max_decrement: Maximum number of bytes by which the cache size can be
 *      decreased in a single re-size.  Note that decrements may also be
 *      restricted by the min_size of the cache, and (in age out modes) by
 *      the empty_reserve field.
 *
 * epochs_before_eviction:  Integer field used in H5C_decr__age_out and
 *      H5C_decr__age_out_with_threshold decrement modes.
 *
 *      This field contains the number of epochs an entry must remain
 *      unaccessed before it is evicted in an attempt to reduce the
 *      cache size.  If applicable, this field must lie in the range
 *      [1, H5C__MAX_EPOCH_MARKERS].
 *
 * apply_empty_reserve:  Boolean field controlling whether the empty_reserve
 *      field is to be used in computing the new cache size when the
 *      decr_mode is H5C_decr__age_out or H5C_decr__age_out_with_threshold.
 *
 * empty_reserve:  To avoid a constant racheting down of cache size by small
 *      amounts in the H5C_decr__age_out and H5C_decr__age_out_with_threshold
 *      modes, this field allows one to require that any cache size
 *      reductions leave the specified fraction of unused space in the cache.
 *
 *      The value of this field must be in the range [0.0, 1.0].  I would
 *      expect typical values to be in the range of 0.01 to 0.1.
 *
 *
 * Parallel Configuration Fields:
 *
 * In PHDF5, all operations that modify metadata must be executed collectively.
 *
 * We used to think that this was enough to ensure consistency across the
 * metadata caches, but since we allow processes to read metadata individually,
 * the order of dirty entries in the LRU list can vary across processes,
 * which can result in inconsistencies between the caches.
 *
 * PHDF5 uses several strategies to prevent such inconsistencies in metadata,
 * all of which use the fact that the same stream of dirty metadata is seen
 * by all processes for purposes of synchronization.  This is done by
 * having each process count the number of bytes of dirty metadata generated,
 * and then running a "sync point" whenever this count exceeds a user
 * specified threshold (see dirty_bytes_threshold below).
 *
 * The current metadata write strategy is indicated by the
 * metadata_write_strategy field.  The possible values of this field, along
 * with the associated metadata write strategies are discussed below.
 *
 * dirty_bytes_threshold:  Threshold of dirty byte creation used to
 *     synchronize updates between caches. (See above for outline and
 *    motivation.)
 *
 *    This value MUST be consistent across all processes accessing the
 *    file.  This field is ignored unless HDF5 has been compiled for
 *    parallel.
 *
 * metadata_write_strategy: Integer field containing a code indicating the
 *    desired metadata write strategy.  The valid values of this field
 *    are enumerated and discussed below:
 *
 *
 *    H5AC_METADATA_WRITE_STRATEGY__PROCESS_0_ONLY:
 *
 *    When metadata_write_strategy is set to this value, only process
 *    zero is allowed to write dirty metadata to disk.  All other
 *    processes must retain dirty metadata until they are informed at
 *    a sync point that the dirty metadata in question has been written
 *    to disk.
 *
 *    When the sync point is reached (or when there is a user generated
 *    flush), process zero flushes sufficient entries to bring it into
 *    compliance with its min clean size (or flushes all dirty entries in
 *    the case of a user generated flush), broad casts the list of
 *    entries just cleaned to all the other processes, and then exits
 *    the sync point.
 *
 *    Upon receipt of the broadcast, the other processes mark the indicated
 *    entries as clean, and leave the sync point as well.
 *
 *
 *    H5AC_METADATA_WRITE_STRATEGY__DISTRIBUTED:
 *
 *    In the distributed metadata write strategy, process zero still makes
 *    the decisions as to what entries should be flushed, but the actual
 *    flushes are distributed across the processes in the computation to
 *    the extent possible.
 *
 *    In this strategy, when a sync point is triggered (either by dirty
 *    metadata creation or manual flush), all processes enter a barrier.
 *
 *    On the other side of the barrier, process 0 constructs an ordered
 *    list of the entries to be flushed, and then broadcasts this list
 *    to the caches in all the processes.
 *
 *    All processes then scan the list of entries to be flushed, flushing
 *    some, and marking the rest as clean.  The algorithm for this purpose
 *    ensures that each entry in the list is flushed exactly once, and
 *    all are marked clean in each cache.
 *
 *    Note that in the case of a flush of the cache, no message passing
 *    is necessary, as all processes have the same list of dirty entries,
 *    and all of these entries must be flushed.  Thus in this case it is
 *    sufficient for each process to sort its list of dirty entries after
 *    leaving the initial barrier, and use this list as if it had been
 *    received from process zero.
 *
 *    To avoid possible messages from the past/future, all caches must
 *    wait until all caches are done before leaving the sync point.
 *
 ****************************************************************************/

#define H5AC__CURR_CACHE_CONFIG_VERSION 1
#define H5AC__MAX_TRACE_FILE_NAME_LEN   1024

#define H5AC_METADATA_WRITE_STRATEGY__PROCESS_0_ONLY 0
#define H5AC_METADATA_WRITE_STRATEGY__DISTRIBUTED    1

/**
 * H5AC_cache_config_t is a public structure intended for use in public APIs.
 * At least in its initial incarnation, it is basically a copy of \c struct
 * \c H5C_auto_size_ctl_t, minus the \c report_fcn field, and plus the
 * \c dirty_bytes_threshold field.
 *
 * The \c report_fcn field is omitted, as including it would require us to make
 * \c H5C_t structure public.
 *
 * The \c dirty_bytes_threshold field does not appear in \c H5C_auto_size_ctl_t,
 * as synchronization between caches on different processes is handled at the \c
 * H5AC level, not at the level of \c H5C.  Note however that there is
 * considerable interaction between this value and the other fields in this
 * structure.
 *
 * Similarly, the \c open_trace_file, \c close_trace_file, and \c
 * trace_file_name fields do not appear in \c H5C_auto_size_ctl_t, as most trace
 * file issues are handled at the \c H5AC level.  The one exception is storage
 * of the pointer to the trace file, which is handled by \c H5C.
 *
 * The structure is in H5ACpublic.h as we may wish to allow different
 * configuration options for metadata and raw data caches.
 */

//! <!-- [H5AC_cache_config_t_snip] -->
typedef struct H5AC_cache_config_t {
    /* general configuration fields: */
    //! <!-- [H5AC_cache_config_t_general_snip] -->
    int version;
    /**< Integer field indicating the version of the H5AC_cache_config_t
     * in use. This field should be set to #H5AC__CURR_CACHE_CONFIG_VERSION
     * (defined in H5ACpublic.h). */

    hbool_t rpt_fcn_enabled;
    /**< Boolean flag indicating whether the adaptive cache resize report
     * function is enabled. This field should almost always be set to disabled
     * (0). Since resize algorithm activity is reported via stdout, it MUST be
     * set to disabled (0) on Windows machines.\n
     * The report function is not supported code, and can be expected to change
     * between versions of the library. Use it at your own risk. */

    hbool_t open_trace_file;
    /**< Boolean field indicating whether the
     * \ref H5AC_cache_config_t.trace_file_name "trace_file_name"
     * field should be used to open a trace file for the cache.\n
     * The trace file is a debugging feature that allows the capture
     * of top level metadata cache requests for purposes of debugging
     * and/or optimization. This field should normally be set to 0, as
     * trace file collection imposes considerable overhead.\n
     * This field should only be set to 1 when the
     * \ref H5AC_cache_config_t.trace_file_name "trace_file_name"
     * contains the full path of the desired trace file, and either
     * there is no open trace file on the cache, or the
     * \ref H5AC_cache_config_t.close_trace_file "close_trace_file"
     * field is also 1.\n
     * The trace file feature is unsupported unless used at the
     * direction of The HDF Group. It is intended to allow The HDF
     * Group to collect a trace of cache activity in cases of occult
     * failures and/or poor performance seen in the field, so as to aid
     * in reproduction in the lab. If you use it absent the direction
     * of The HDF Group, you are on your own. */

    hbool_t close_trace_file;
    /**< Boolean field indicating whether the current trace file
     *(if any) should be closed.\n
     * See the above comments on the \ref H5AC_cache_config_t.open_trace_file
     * "open_trace_file" field. This field should be set to 0 unless there is
     * an open trace file on the cache that you wish to close.\n
     * The trace file feature is unsupported unless used at the direction of
     * The HDF Group. It is intended to allow The HDF Group to collect a trace
     * of cache activity in cases of occult failures and/or poor performance
     * seen in the field, so as to aid in reproduction in the lab. If you use
     * it absent the direction of The HDF Group, you are on your own. */

    char trace_file_name[H5AC__MAX_TRACE_FILE_NAME_LEN + 1];
    /**< Full path of the trace file to be opened if the
     * \ref H5AC_cache_config_t.open_trace_file "open_trace_file" field is set
     * to 1.\n
     * In the parallel case, an ascii representation of the MPI rank of the
     * process will be appended to the file name to yield a unique trace file
     * name for each process.\n
     * The length of the path must not exceed #H5AC__MAX_TRACE_FILE_NAME_LEN
     * characters.\n
     * The trace file feature is unsupported unless used at the direction of
     * The HDF Group. It is intended to allow The HDF Group to collect a trace
     * of cache activity in cases of occult failures and/or poor performance
     * seen in the field, so as to aid in reproduction in the lab. If you use
     * it absent the direction of The HDF Group, you are on your own. */

    hbool_t evictions_enabled;
    /**< A boolean flag indicating whether evictions from the metadata cache
     * are enabled. This flag is initially set to enabled (1).\n
     * In rare circumstances, the raw data throughput quirements may be so high
     * that the user wishes to postpone metadata writes so as to reserve I/O
     * throughput for raw data. The \p evictions_enabled field exists to allow
     * this. However, this is an extreme step, and you have no business doing
     * it unless you have read the User Guide section on metadata caching, and
     * have considered all other options carefully.\n
     * The \p evictions_enabled field may not be set to disabled (0)
     * unless all adaptive cache resizing code is disabled via the
     * \ref H5AC_cache_config_t.incr_mode "incr_mode",
     * \ref H5AC_cache_config_t.flash_incr_mode "flash_incr_mode",
     * \ref H5AC_cache_config_t.decr_mode "decr_mode" fields.\n
     * When this flag is set to disabled (\c 0), the metadata cache will not
     * attempt to evict entries to make space for new entries, and thus will
     * grow without bound.\n
     * Evictions will be re-enabled when this field is set back to \c 1.
     * This should be done as soon as possible. */

    hbool_t set_initial_size;
    /**< Boolean flag indicating whether the cache should be created
     * with a user specified initial size. */

    size_t initial_size;
    /**< If \ref H5AC_cache_config_t.set_initial_size "set_initial_size"
     * is set to 1, \p initial_size must contain he desired initial size in
     * bytes. This value must lie in the closed interval
     * [ \p min_size, \p max_size ]. (see below) */

    double min_clean_fraction;
    /**< This field specifies the minimum fraction of the cache
     * that must be kept either clean or empty.\n
     * The value must lie in the interval [0.0, 1.0]. 0.01 is a good place to
     * start in the serial case. In the parallel case, a larger value is needed
     * -- see the overview of the metadata cache in the
     * “Metadata Caching in HDF5” section of the -- <em>\ref UG</em>
     * for details. */

    size_t max_size;
    /**< Upper bound (in bytes) on the range of values that the
     * adaptive cache resize code can select as the maximum cache size. */

    size_t min_size;
    /**< Lower bound (in bytes) on the range of values that the
     * adaptive cache resize code can select as the minimum cache * size. */

    long int epoch_length;
    /**< Number of cache accesses between runs of the adaptive cache resize
     * code. 50,000 is a good starting number. */
    //! <!-- [H5AC_cache_config_t_general_snip] -->

    /* size increase control fields: */
    //! <!-- [H5AC_cache_config_t_incr_snip] -->
    enum H5C_cache_incr_mode incr_mode;
    /**< Enumerated value indicating the operational mode of the automatic
     * cache size increase code. At present, only two values listed in
     * #H5C_cache_incr_mode are legal. */

    double lower_hr_threshold;
    /**< Hit rate threshold used by the hit rate threshold cache size
     * increment algorithm.\n
     * When the hit rate over an epoch is below this threshold and the cache
     * is full, the maximum size of the cache is multiplied by increment
     * (below), and then clipped as necessary to stay within \p max_size, and
     * possibly \p max_increment.\n
     * This field must lie in the interval [0.0, 1.0]. 0.8 or 0.9 is a good
     * place to start. */

    double increment;
    /**< Factor by which the hit rate threshold cache size increment
     * algorithm multiplies the current cache max size to obtain a tentative
     * new cache size.\n
     * The actual cache size increase will be clipped to satisfy the \p max_size
     * specified in the general configuration, and possibly max_increment
     * below.\n
     * The parameter must be greater than or equal to 1.0 -- 2.0 is a reasonable
     * value.\n
     * If you set it to 1.0, you will effectively disable cache size increases.
     */

    hbool_t apply_max_increment;
    /**< Boolean flag indicating whether an upper limit should be applied to
     * the size of cache size increases. */

    size_t max_increment;
    /**< Maximum number of bytes by which cache size can be increased in a
     * single step -- if applicable. */

    enum H5C_cache_flash_incr_mode flash_incr_mode;
    /**< Enumerated value indicating the operational mode of the flash cache
     * size increase code. At present, only two listed  values in
     * #H5C_cache_flash_incr_mode are legal.*/

    double flash_multiple;
    /**< The factor by which the size of the triggering entry / entry size
     * increase is multiplied to obtain the initial cache size increment. This
     * increment may be reduced to reflect existing free space in the cache and
     * the \p max_size field above.\n
     * The parameter must lie in the interval [0.0, 1.0]. 0.1 or 0.05 is a good
     * place to start.\n
     * At present, this field must lie in the range [0.1, 10.0]. */

    double flash_threshold;
    /**< The factor by which the current maximum cache size is multiplied to
     * obtain the minimum size entry / entry size increase which may trigger a
     * flash cache size increase. \n
     * At present, this value must lie in the range [0.1, 1.0]. */
    //! <!-- [H5AC_cache_config_t_incr_snip] -->

    /* size decrease control fields: */
    //! <!-- [H5AC_cache_config_t_decr_snip] -->
    enum H5C_cache_decr_mode decr_mode;
    /**< Enumerated value indicating the operational mode of the tomatic
     * cache size decrease code. At present, the values listed in
     * #H5C_cache_decr_mode are legal.*/

    double upper_hr_threshold;
    /**< Hit rate threshold for the hit rate threshold and ageout with hit
     * rate threshold cache size decrement algorithms.\n
     * When \p decr_mode is #H5C_decr__threshold, and the hit rate over a given
     * epoch exceeds the supplied threshold, the current maximum cache
     * size is multiplied by decrement to obtain a tentative new (and smaller)
     * maximum cache size.\n
     * When \p decr_mode is #H5C_decr__age_out_with_threshold, there is
     * no attempt to find and evict aged out entries unless the hit rate in
     * the previous epoch exceeded the supplied threshold.\n
     * This field must lie in the interval [0.0, 1.0].\n
     * For #H5C_incr__threshold, .9995 or .99995 is a good place to start.\n
     * For #H5C_decr__age_out_with_threshold, .999 might be more useful.*/

    double decrement;
    /**< In the hit rate threshold cache size decrease algorithm, this
     * parameter contains the factor by which the current max cache size is
     * multiplied to produce a tentative new cache size.\n
     * The actual cache size decrease will be clipped to satisfy the
     * \ref H5AC_cache_config_t.min_size "min_size" specified in the general
     * configuration, and possibly \ref H5AC_cache_config_t.max_decrement
     * "max_decrement".\n
     * The parameter must be be in the interval [0.0, 1.0].\n
     * If you set it to 1.0, you will effectively
     * disable cache size decreases. 0.9 is a reasonable starting point. */

    hbool_t apply_max_decrement;
    /**< Boolean flag indicating ether an upper limit should be applied to
     * the size of cache size decreases. */

    size_t max_decrement;
    /**< Maximum number of bytes by which the maximum cache size can be
     * decreased in any single step -- if applicable.*/

    int epochs_before_eviction;
    /**< In the ageout based cache size reduction algorithms, this field
     * contains the minimum number of epochs an entry must remain unaccessed in
     * cache before the cache size reduction algorithm tries to evict it. 3 is a
     * reasonable value. */

    hbool_t apply_empty_reserve;
    /**< Boolean flag indicating whether the ageout based decrement
     * algorithms will maintain a empty reserve when decreasing cache size. */

    double empty_reserve;
    /**< Empty reserve as a fraction maximum cache size if applicable.\n When
     * so directed, the ageout based algorithms will not decrease the maximum
     * cache size unless the empty reserve can be met.\n The parameter must lie
     * in the interval [0.0, 1.0]. 0.1 or 0.05 is a good place to start. */
    //! <!-- [H5AC_cache_config_t_decr_snip] -->

    /* parallel configuration fields: */
    //! <!-- [H5AC_cache_config_t_parallel_snip] -->
    size_t dirty_bytes_threshold;
    /**< Threshold number of bytes of dirty metadata generation for
     * triggering synchronizations of the metadata caches serving the target
     * file in the parallel case.\n Synchronization occurs whenever the number
     * of bytes of dirty metadata created since the last synchronization exceeds
     * this limit.\n This field only applies to the parallel case. While it is
     * ignored elsewhere, it can still draw a value out of bounds error.\n It
     * must be consistent across all caches on any given file.\n By default,
     * this field is set to 256 KB. It shouldn't be more than half the current
     * max cache size times the min clean fraction. */

    int metadata_write_strategy;
    /**< Desired metadata write strategy. The valid values for this field
     * are:\n #H5AC_METADATA_WRITE_STRATEGY__PROCESS_0_ONLY: Specifies the only
     * process zero is allowed to write dirty metadata to disk.\n
     * #H5AC_METADATA_WRITE_STRATEGY__DISTRIBUTED: Specifies that process zero
     * still makes the decisions as to what entries should be flushed, but the
     * actual flushes are distributed across the processes in the computation to
     * the extent possible.\n The src/H5ACpublic.h include file in the HDF5
     * library has detailed information on each strategy. */
    //! <!-- [H5AC_cache_config_t_parallel_snip] -->
} H5AC_cache_config_t;
//! <!-- [H5AC_cache_config_t_snip] -->

#define H5AC__CURR_CACHE_IMAGE_CONFIG_VERSION 1

/**
 * No limit on number of times a prefetched entry can appear in subsequent
 * cache images
 * \since 1.10.1
 */
#define H5AC__CACHE_IMAGE__ENTRY_AGEOUT__NONE -1
/**
 * Limit on number of times a prefetched entry can appear in subsequent
 * cache images
 * \since 1.10.1
 */
#define H5AC__CACHE_IMAGE__ENTRY_AGEOUT__MAX 100

//! <!-- [H5AC_cache_image_config_t_snip] -->
/**
 * H5AC_cache_image_config_t is a public structure intended for use in public
 * APIs.  At least in its initial incarnation, it is a copy of \c struct \c
 * H5C_cache_image_ctl_t.
 */

typedef struct H5AC_cache_image_config_t {
    int version;
    /**< Integer field containing the version number of this version of the \c
     *  H5C_image_ctl_t structure.  Any instance of \c H5C_image_ctl_t passed
     *  to the cache must have a known version number, or an error will be
     *  flagged.
     */
    hbool_t generate_image;
    /**< Boolean flag indicating whether a cache image should be created on file
     *   close.
     */
    hbool_t save_resize_status;
    /**< Boolean flag indicating whether the cache image should include the
     *  adaptive cache resize configuration and status.  Note that this field
     *  is ignored at present.
     */
    int entry_ageout;
    /**< Integer field indicating the maximum number of times a
     *   prefetched entry can appear in subsequent cache images.  This field
     *   exists to allow the user to avoid the buildup of infrequently used
     *   entries in long sequences of cache images.
     *
     *   The value of this field must lie in the range \ref
     *   H5AC__CACHE_IMAGE__ENTRY_AGEOUT__NONE (-1) to \ref
     *   H5AC__CACHE_IMAGE__ENTRY_AGEOUT__MAX (100).
     *
     *   \ref H5AC__CACHE_IMAGE__ENTRY_AGEOUT__NONE means that no limit is
     *   imposed on number of times a prefetched entry can appear in subsequent
     *   cache images.
     *
     *   A value of 0 prevents prefetched entries from being included in cache
     *   images.
     *
     *   Positive integers restrict prefetched entries to the specified number
     *   of appearances.
     *
     *   Note that the number of subsequent cache images that a prefetched entry
     *   has appeared in is tracked in an 8 bit field.  Thus, while \ref
     *   H5AC__CACHE_IMAGE__ENTRY_AGEOUT__MAX can be increased from its current
     *   value, any value in excess of 255 will be the functional equivalent of
     *   \ref H5AC__CACHE_IMAGE__ENTRY_AGEOUT__NONE.
     */
} H5AC_cache_image_config_t;

//! <!-- [H5AC_cache_image_config_t_snip] -->

#endif
