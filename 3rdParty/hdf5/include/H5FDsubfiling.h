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

/* Purpose: The public header file for the subfiling driver. */
#ifndef H5FDsubfiling_H
#define H5FDsubfiling_H

#ifdef H5_HAVE_SUBFILING_VFD
/**
 * \def H5FD_SUBFILING
 * Macro that returns the identifier for the #H5FD_SUBFILING driver. \hid_t{file driver}
 */
#define H5FD_SUBFILING (H5FDperform_init(H5FD_subfiling_init))
#else
#define H5FD_SUBFILING (H5I_INVALID_HID)
#endif

/**
 * \def H5FD_SUBFILING_NAME
 * The canonical name for the #H5FD_SUBFILING driver
 */
#define H5FD_SUBFILING_NAME "subfiling"

#ifdef H5_HAVE_SUBFILING_VFD

#ifndef H5FD_SUBFILING_FAPL_MAGIC
/**
 * \def H5FD_SUBFILING_CURR_FAPL_VERSION
 * The version number of the H5FD_subfiling_config_t configuration
 * structure for the #H5FD_SUBFILING driver
 */
#define H5FD_SUBFILING_CURR_FAPL_VERSION 1
/**
 * \def H5FD_SUBFILING_FAPL_MAGIC
 * Unique number used to distinguish the #H5FD_SUBFILING driver from other HDF5 file drivers
 */
#define H5FD_SUBFILING_FAPL_MAGIC 0xFED01331
#endif

/**
 * \def H5FD_SUBFILING_DEFAULT_STRIPE_SIZE
 * The default stripe size (in bytes) for data stripes in subfiles
 */
#define H5FD_SUBFILING_DEFAULT_STRIPE_SIZE (32 * 1024 * 1024)

/**
 * \def H5FD_SUBFILING_DEFAULT_STRIPE_COUNT
 * Macro for the default Subfiling stripe count value. The default
 * is currently to use one subfile per node.
 */
#define H5FD_SUBFILING_DEFAULT_STRIPE_COUNT -1

/**
 * \def H5FD_SUBFILING_FILENAME_TEMPLATE
 * The basic printf-style template for a #H5FD_SUBFILING driver
 * subfile filename. The format specifiers correspond to:
 *
 * \par \%s
 *   base filename, e.g. "file.h5"
 *
 * \par \%PRIu64
 *   file inode, e.g. 11273556
 *
 * \par \%0*d
 *   number (starting at 1) signifying the Nth (out of
 *   total number of subfiles) subfile. Zero-padded
 *   according to the number of digits in the number of
 *   subfiles (calculated by <tt>log10(num_subfiles) + 1)</tt>
 *
 * \par \%d
 *   number of subfiles
 *
 * yielding filenames such as:
 *
 * file.h5.subfile_11273556_01_of_10 \n
 * file.h5.subfile_11273556_02_of_10 \n
 * file.h5.subfile_11273556_10_of_10 \n
 */
#define H5FD_SUBFILING_FILENAME_TEMPLATE "%s.subfile_%" PRIu64 "_%0*d_of_%d"

/**
 * \def H5FD_SUBFILING_CONFIG_FILENAME_TEMPLATE
 * The basic printf-style template for a #H5FD_SUBFILING driver
 * configuration filename. The format specifiers correspond to:
 *
 * \par \%s
 *   base filename, e.g. "file.h5"
 *
 * \par \%PRIu64
 *   file inode, e.g. 11273556
 *
 * yielding a filename such as:
 *
 * file.h5.subfile_11273556.config
 */
#define H5FD_SUBFILING_CONFIG_FILENAME_TEMPLATE "%s.subfile_%" PRIu64 ".config"

/*
 * Environment variables interpreted by the HDF5 Subfiling feature
 */

/**
 * \def H5FD_SUBFILING_STRIPE_SIZE
 * Macro for name of the environment variable that specifies the size
 * (in bytes) for data stripes in subfiles
 *
 * The value set for this environment variable is interpreted as a
 * long long value and must be > 0.
 */
#define H5FD_SUBFILING_STRIPE_SIZE "H5FD_SUBFILING_STRIPE_SIZE"
/**
 * \def H5FD_SUBFILING_IOC_PER_NODE
 * Macro for name of the environment variable that specifies the number
 * of MPI ranks per node to use as I/O concentrators
 *
 * The value set for this environment variable is interpreted as a
 * long value and must be > 0.
 */
#define H5FD_SUBFILING_IOC_PER_NODE "H5FD_SUBFILING_IOC_PER_NODE"
/**
 * \def H5FD_SUBFILING_IOC_SELECTION_CRITERIA
 * Macro for name of the environment variable that provides information
 * for selection MPI ranks as I/O concentrators
 *
 * The value set for this environment variable is interpreted differently,
 * depending on the IOC selection method chosen.
 *
 * For #SELECT_IOC_ONE_PER_NODE, this value is ignored.
 *
 * For #SELECT_IOC_EVERY_NTH_RANK, this value is interpreted as a
 *     long value and must be > 0. The value will correspond to the
 *     `N` value when selecting every `N`-th MPI rank as an I/O
 *     concentrator.
 *
 * For #SELECT_IOC_WITH_CONFIG, this value is ignored as that particular
 *     IOC selection method is not currently supported.
 *
 * For #SELECT_IOC_TOTAL, this value is interpreted as a long value
 *     and must be > 0. The value will correspond to the total number
 *     of I/O concentrators to be used.
 */
#define H5FD_SUBFILING_IOC_SELECTION_CRITERIA "H5FD_SUBFILING_IOC_SELECTION_CRITERIA"
/**
 * \def H5FD_SUBFILING_SUBFILE_PREFIX
 * Macro for name of the environment variable that specifies a prefix
 * to apply to the filenames generated for subfiles
 *
 * The value set for this environment variable is interpreted as a
 * pathname.
 */
#define H5FD_SUBFILING_SUBFILE_PREFIX "H5FD_SUBFILING_SUBFILE_PREFIX"
/**
 * \def H5FD_SUBFILING_CONFIG_FILE_PREFIX
 * Macro for name of the environment variable that specifies a prefix
 * to apply to the subfiling configuration filename. Useful for cases
 * where the application wants to place the configuration file in a
 * different directory than the default of putting it alongside the
 * generated subfiles. For example, when writing to node-local storage
 * one may wish to place the configuration file on a scratch file
 * system readable by all nodes, while the subfiles are initially
 * written to the node-local storage.
 *
 * The value set for this environment variable is interpreted as a
 * pathname that must already exist.
 *
 * NOTE: As this prefix string will be encoded in the driver info
 *       message that gets written to the file, there is an upper
 *       limit of about ~900 single-byte characters for this string,
 *       though possibly less due to other information the driver
 *       may encode. Avoid long prefix names where possible.
 */
#define H5FD_SUBFILING_CONFIG_FILE_PREFIX "H5FD_SUBFILING_CONFIG_FILE_PREFIX"

/**
 * \enum H5FD_subfiling_ioc_select_t
 * This enum defines the various constants to allow different
 * allocations of MPI ranks as I/O concentrators.
 *
 * \var SELECT_IOC_ONE_PER_NODE
 *      Default selection method. One MPI rank per node is used as an
 *      I/O concentrator. If this selection method is used, the number
 *      of I/O concentrators per node can be adjusted with the
 *      #H5FD_SUBFILING_IOC_PER_NODE environment variable.
 *
 * \var SELECT_IOC_EVERY_NTH_RANK
 *      Starting with MPI rank 0, a stride of 'N' is applied to the MPI
 *      rank values to determine the next I/O concentrator. The
 *      #H5FD_SUBFILING_IOC_SELECTION_CRITERIA environment variable must
 *      be set to the value desired for 'N'.
 *
 * \var SELECT_IOC_WITH_CONFIG
 *      Currently unsupported. Use a configuration file to determine
 *      the mapping from MPI ranks to I/O concentrators. The
 *      #H5FD_SUBFILING_IOC_SELECTION_CRITERIA environment variable must
 *      be set to the path to the configuration file.
 *
 * \var SELECT_IOC_TOTAL
 *      Specifies that a total of 'N' I/O concentrators should be used.
 *      Starting with MPI rank 0, a stride of 'MPI comm size' / 'N' is
 *      applied to the MPI rank values to determine the next I/O
 *      concentrator. The #H5FD_SUBFILING_IOC_SELECTION_CRITERIA
 *      environment variable must be set to the value desired for 'N'.
 *
 * \var ioc_selection_options
 *      Unused. Sentinel value
 */
typedef enum {
    SELECT_IOC_ONE_PER_NODE = 0, /* Default                              */
    SELECT_IOC_EVERY_NTH_RANK,   /* Starting at rank 0, select-next += N */
    SELECT_IOC_WITH_CONFIG,      /* NOT IMPLEMENTED: Read-from-file      */
    SELECT_IOC_TOTAL,            /* Starting at rank 0, mpi_size / total */
    ioc_selection_options        /* Sentinel value                       */
    /* NOTE: Add to the Fortran constants (H5f90global.F90)  when adding new entries */
} H5FD_subfiling_ioc_select_t;

/**
 * \struct H5FD_subfiling_params_t
 * \brief Subfiling parameter structure that is shared between the #H5FD_SUBFILING
 *        and #H5FD_IOC drivers
 *
 * \var H5FD_subfiling_ioc_select_t H5FD_subfiling_params_t::ioc_selection
 *      The method to use for selecting MPI ranks to be I/O concentrators. The
 *      current default is to select one MPI rank per node to be an I/O concentrator.
 *      Refer to #H5FD_subfiling_ioc_select_t for a description of the algorithms
 *      available for use.
 *
 * \var int64_t H5FD_subfiling_params_t::stripe_size
 *      The stripe size defines the size (in bytes) of the data stripes in the
 *      subfiles for the logical HDF5 file. Data is striped across the subfiles
 *      in a round-robin wrap-around fashion in segments equal to the stripe size.
 *
 *      For example, in an HDF5 file consisting of four subfiles with a 1MiB stripe
 *      size, the first and fifth 1MiB of data would reside in the first subfile,
 *      the second and sixth 1MiB of data would reside in the second subfile and so
 *      on.
 *
 *      This value can also be set or adjusted with the #H5FD_SUBFILING_STRIPE_SIZE
 *      environment variable.
 *
 * \var int32_t H5FD_subfiling_params_t::stripe_count
 *      The target number of subfiles to use for the logical HDF5 file. The current
 *      default is to use one subfile per node, but it can be useful to set a
 *      different target number of subfiles, especially if the HDF5 application will
 *      pre-create the HDF5 file on a single MPI rank. In that particular case, the
 *      single rank will need to know how many subfiles the logical HDF5 file will
 *      consist of in order to properly pre-create the file.
 *
 *      This value is used in conjunction with the IOC selection method to determine
 *      which MPI ranks will be assigned as I/O concentrators. Alternatively, the
 *      mapping between MPI ranks and I/O concentrators can be set or adjusted with a
 *      combination of the #ioc_selection field and the #H5FD_SUBFILING_IOC_PER_NODE
 *      and #H5FD_SUBFILING_IOC_SELECTION_CRITERIA environment variables.
 */
typedef struct H5FD_subfiling_params_t {
    H5FD_subfiling_ioc_select_t ioc_selection; /* Method to select I/O concentrators          */
    int64_t                     stripe_size;   /* Size (in bytes) of data stripes in subfiles */
    int32_t                     stripe_count;  /* Target number of subfiles to use            */
} H5FD_subfiling_params_t;

//! <!-- [H5FD_subfiling_config_t_snip] -->
/**
 * \struct H5FD_subfiling_config_t
 * \brief Configuration structure for H5Pset_fapl_subfiling() / H5Pget_fapl_subfiling()
 *
 * \details H5FD_subfiling_config_t is a public structure that is used to pass
 *          subfiling configuration data to the #H5FD_SUBFILING driver via
 *          a File Access Property List. A pointer to an instance of this structure
 *          is a parameter to H5Pset_fapl_subfiling() and H5Pget_fapl_subfiling().
 *
 * \var uint32_t H5FD_subfiling_config_t::magic
 *      A somewhat unique number which distinguishes the #H5FD_SUBFILING driver
 *      from other drivers. Used in combination with a version number, it can
 *      help to validate a user-generated File Access Property List. This field
 *      should be set to #H5FD_SUBFILING_FAPL_MAGIC.
 *
 * \var uint32_t H5FD_subfiling_config_t::version
 *      Version number of the H5FD_subfiling_config_t structure. Any instance
 *      passed to H5Pset_fapl_subfiling() / H5Pget_fapl_subfiling() must have
 *      a recognized version number or an error will be raised. Currently, this
 *      field should be set to #H5FD_SUBFILING_CURR_FAPL_VERSION.
 *
 * \var hid_t H5FD_subfiling_config_t::ioc_fapl_id
 *      The File Access Property List which is setup with the file driver that
 *      the #H5FD_SUBFILING driver will use for servicing I/O requests to the
 *      subfiles. Currently, the File Access Property List must be setup with
 *      the #H5FD_IOC driver by calling H5Pset_fapl_ioc(), but future development
 *      may allow other file drivers to be used.
 *
 * \var bool H5FD_subfiling_config_t::require_ioc
 *      A boolean flag which indicates whether the #H5FD_SUBFILING driver should
 *      use the #H5FD_IOC driver for its I/O operations. This field should currently
 *      always be set to true.
 *
 * \var H5FD_subfiling_params_t H5FD_subfiling_config_t::shared_cfg
 *      A structure which contains the subfiling parameters that are shared between
 *      the #H5FD_SUBFILING and #H5FD_IOC drivers. This includes the subfile stripe
 *      size, stripe count, IOC selection method, etc.
 *
 */
typedef struct H5FD_subfiling_config_t {
    uint32_t magic;                     /* Must be set to H5FD_SUBFILING_FAPL_MAGIC                         */
    uint32_t version;                   /* Must be set to H5FD_SUBFILING_CURR_FAPL_VERSION                  */
    hid_t    ioc_fapl_id;               /* The FAPL setup with the stacked VFD to use for I/O concentrators */
    bool     require_ioc;               /* Whether to use the IOC VFD (currently must always be true)       */
    H5FD_subfiling_params_t shared_cfg; /* Subfiling/IOC parameters (stripe size, stripe count, etc.)       */
} H5FD_subfiling_config_t;
//! <!-- [H5FD_subfiling_config_t_snip] -->

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Internal routine to initialize #H5FD_SUBFILING driver. Not meant to be
 *        called directly by an HDF5 application
 */
H5_DLL hid_t H5FD_subfiling_init(void);
/**
 * \ingroup FAPL
 *
 * \brief Modifies the specified File Access Property List to use the #H5FD_SUBFILING driver
 *
 * \fapl_id
 * \param[in] vfd_config Pointer to #H5FD_SUBFILING driver configuration structure. May be NULL.
 * \returns \herr_t
 *
 * \details H5Pset_fapl_subfiling() modifies the File Access Property List to use the
 *          #H5FD_SUBFILING driver.
 *
 *          The #H5FD_SUBFILING driver is an MPI-based file driver that allows an
 *          HDF5 application to distribute a logical HDF5 file across a collection
 *          of "subfiles" in equal-sized data segment "stripes". I/O to the logical
 *          HDF5 file is then directed to the appropriate "subfile" according to the
 *          #H5FD_SUBFILING configuration and a system of I/O concentrators, which
 *          are MPI ranks operating worker threads.
 *
 *          By allowing a configurable stripe size, number of I/O concentrators and
 *          method for selecting MPI ranks as I/O concentrators, the #H5FD_SUBFILING
 *          driver aims to enable an HDF5 application to find a middle ground between
 *          the single shared file and file-per-process approaches to parallel file I/O
 *          for the particular machine the application is running on. In general, the
 *          goal is to avoid some of the complexity of the file-per-process approach
 *          while also minimizing the locking issues of the single shared file approach
 *          on a parallel file system.
 *
 * \note Since the #H5FD_SUBFILING driver is an MPI-based file driver, the HDF5
 *       application should ensure that H5Pset_mpi_params() is called before this
 *       routine so that the appropriate MPI communicator and info objects will be
 *       setup for use by the #H5FD_SUBFILING and #H5FD_IOC drivers.
 *
 * \note The current architecture of the #H5FD_SUBFILING driver requires that the
 *       HDF5 application must have been initialized with MPI_Init_thread() using
 *       a value of MPI_THREAD_MULTIPLE for the thread support level.
 *
 * \note The \p vfd_config parameter may be NULL. In this case, the reference
 *       implementation I/O concentrator VFD will be used with the default settings
 *       of one I/O concentrator per node and a stripe size of 32MiB. Refer to the
 *       H5FD_subfiling_config_t documentation for information about configuration
 *       for the #H5FD_SUBFILING driver.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5Pset_fapl_subfiling(hid_t fapl_id, const H5FD_subfiling_config_t *vfd_config);
/**
 * \ingroup FAPL
 *
 * \brief Queries a File Access Property List for #H5FD_SUBFILING file driver properties
 *
 * \fapl_id
 * \param[out] config_out Pointer to H5FD_subfiling_config_t structure through which the
 *                        #H5FD_SUBFILING file driver properties will be returned.
 *
 * \returns \herr_t
 *
 * \details H5Pget_fapl_subfiling() queries the specified File Access Property List for
 *          #H5FD_SUBFILING driver properties as set by H5Pset_fapl_subfiling(). If the
 *          #H5FD_SUBFILING driver has not been set on the File Access Property List, a
 *          default configuration is returned. An HDF5 application may use this
 *          functionality to manually configure the #H5FD_SUBFILING driver by calling
 *          H5Pget_fapl_subfiling() on a newly-created File Access Property List, adjusting
 *          the default values and then calling H5Pset_fapl_subfiling() with the configured
 *          H5FD_subfiling_config_t structure.
 *
 * \note H5Pget_fapl_subfiling() returns the #H5FD_SUBFILING driver properties as they
 *       were initially set for the File Access Property List using H5Pset_fapl_subfiling().
 *       Alternatively, the driver properties can be modified at runtime according to values
 *       set for the #H5FD_SUBFILING_STRIPE_SIZE, #H5FD_SUBFILING_IOC_PER_NODE and
 *       #H5FD_SUBFILING_IOC_SELECTION_CRITERIA environment variables. However, driver
 *       properties set through environment variables will not be reflected in what is
 *       returned by H5Pget_fapl_subfiling(), so an application may need to check those
 *       environment variables to get accurate values for the #H5FD_SUBFILING driver
 *       properties.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5Pget_fapl_subfiling(hid_t fapl_id, H5FD_subfiling_config_t *config_out);

#ifdef __cplusplus
}
#endif

#endif /* H5_HAVE_SUBFILING_VFD */

#endif /* H5FDsubfiling_H */
