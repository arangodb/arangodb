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

/*
 * This file contains public declarations for the H5ES (event set) module.
 */

#ifndef H5ESpublic_H
#define H5ESpublic_H

#include "H5public.h"  /* Generic Functions                        */
#include "H5Ipublic.h" /* Identifiers                              */

/*****************/
/* Public Macros */
/*****************/

/**
 * Default value for "no event set" / synchronous execution. Used in
 * place of a @ref hid_t identifier.
 */
#define H5ES_NONE 0

/* Special "wait" timeout values */
#define H5ES_WAIT_FOREVER (UINT64_MAX) /**< Wait until all operations complete */

/**
 * Don't wait for operations to complete, just check their status.
 * (This allows @ref H5ESwait to behave like a 'test' operation)
 */
#define H5ES_WAIT_NONE (0)

/*******************/
/* Public Typedefs */
/*******************/

/**
 * Asynchronous operation status
 */
typedef enum H5ES_status_t {
    H5ES_STATUS_IN_PROGRESS, /**< Operation(s) have not yet completed         */
    H5ES_STATUS_SUCCEED,     /**< Operation(s) have completed, successfully   */
    H5ES_STATUS_CANCELED,    /**< Operation(s) has been canceled              */
    H5ES_STATUS_FAIL         /**< An operation has completed, but failed      */
} H5ES_status_t;

/**
 * Information about operations in an event set
 */
typedef struct H5ES_op_info_t {
    /* API call info */
    const char *api_name; /**< Name of HDF5 API routine called */
    char       *api_args; /**< "Argument string" for arguments to HDF5 API routine called */

    /* Application info */
    const char *app_file_name; /**< Name of source file where the HDF5 API routine was called */
    const char *app_func_name; /**< Name of function where the HDF5 API routine was called */
    unsigned    app_line_num;  /**< Line # of source file where the HDF5 API routine was called */

    /* Operation info */
    uint64_t op_ins_count; /**< Counter of operation's insertion into event set */
    uint64_t op_ins_ts;    /**< Timestamp for when the operation was inserted into the event set */
    uint64_t op_exec_ts;   /**< Timestamp for when the operation began execution */
    uint64_t op_exec_time; /**< Execution time for operation (in ns) */
} H5ES_op_info_t;

//! <!-- [H5ES_err_info_t_snip] -->
/**
 * Information about failed operations in event set
 */
typedef struct H5ES_err_info_t {
    /* API call info */
    char *api_name; /**< Name of HDF5 API routine called */
    char *api_args; /**< "Argument string" for arguments to HDF5 API routine called */

    /* Application info */
    char    *app_file_name; /**< Name of source file where the HDF5 API routine was called */
    char    *app_func_name; /**< Name of function where the HDF5 API routine was called */
    unsigned app_line_num;  /**< Line # of source file where the HDF5 API routine was called */

    /* Operation info */
    uint64_t op_ins_count; /**< Counter of operation's insertion into event set */
    uint64_t op_ins_ts;    /**< Timestamp for when the operation was inserted into the event set */
    uint64_t op_exec_ts;   /**< Timestamp for when the operation began execution */
    uint64_t op_exec_time; /**< Execution time for operation (in ns) */

    /* Error info */
    hid_t err_stack_id; /**< ID for error stack from failed operation */
} H5ES_err_info_t;
//! <!-- [H5ES_err_info_t_snip] -->

/*
More Possible Info for H5ES_op_info_t:
    Parent Operation's request token (*) -> "parent event count"? -- Could be
        used to "prune" child operations from reported errors, with flag
        to H5ESget_err_info?

Possible debugging routines:  (Should also be configured from Env Var)
    H5ESdebug_signal(hid_t es_id, signal_t sig, uint64_t <event count>);
    H5ESdebug_err_trace_log(hid_t es_id, const char *filename);
    H5ESdebug_err_trace_fh(hid_t es_id, FILE *fh);
    H5ESdebug_err_signal(hid_t es_id, signal_t sig);
[Possibly option to allow operations to be inserted into event set with error?]

    Example usage:
        es_id = H5EScreate();
        H5ESdebug...(es_id, ...);
        ...
        H5Dwrite_async(..., es_id);

How to Trace Async Operations?
    <Example of stacking Logging VOL Connector w/Async VOL Connector>

*/

/**
 * Callback for H5ESregister_insert_func()
 */
typedef int (*H5ES_event_insert_func_t)(const H5ES_op_info_t *op_info, void *ctx);

/**
 * Callback for H5ESregister_complete_func()
 */
typedef int (*H5ES_event_complete_func_t)(const H5ES_op_info_t *op_info, H5ES_status_t status,
                                          hid_t err_stack, void *ctx);

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \ingroup H5ES
 *
 * \brief Creates an event set
 *
 * \return \hid_t{event set}
 *
 * \details H5EScreate() creates a new event set and returns a corresponding
 *          event set identifier.
 *
 * \since 1.14.0
 *
 */
H5_DLL hid_t H5EScreate(void);

/**
 * \ingroup H5ES
 *
 * \brief Waits for operations in event set to complete
 *
 * \es_id
 * \param[in] timeout Total time in nanoseconds to wait for all operations in
 *            the event set to complete
 * \param[out] num_in_progress The number of operations still in progress
 * \param[out] err_occurred Flag if an operation in the event set failed
 * \return \herr_t
 *
 * \details H5ESwait() waits for operations in an event set \p es_id to wait
 *          with \p timeout.
 *
 *          Timeout value is in nanoseconds, and is for the H5ESwait() call and
 *          not for each individual operation in the event set. For example, if
 *          "10" is passed as a timeout value and the event set waited 4
 *          nanoseconds for the first operation to complete, the remaining
 *          operations would be allowed to wait for at most 6 nanoseconds more,
 *          i.e., the timeout value used across all operations in the event set
 *          until it reaches 0, then any remaining operations are only checked
 *          for completion, not waited on.
 *
 *          This call will stop waiting on operations and will return
 *          immediately if an operation fails. If a failure occurs, the value
 *          returned for the number of operations in progress may be inaccurate.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESwait(hid_t es_id, uint64_t timeout, size_t *num_in_progress, hbool_t *err_occurred);

/**
 * \ingroup H5ES
 *
 * \brief Attempt to cancel operations in an event set
 *
 * \es_id
 * \param[out] num_not_canceled The number of events not canceled
 * \param[out] err_occurred Status indicating if error is present in the event set
 * \return \herr_t
 *
 * \details H5EScancel() attempts to cancel operations in an event set specified
 *          by \p es_id. H5ES_NONE is a valid value for \p es_id, but functions as a no-op.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5EScancel(hid_t es_id, size_t *num_not_canceled, hbool_t *err_occurred);

/**
 * \ingroup H5ES
 *
 * \brief Retrieves number of events in an event set
 *
 * \es_id
 * \param[out] count The number of events in the event set
 * \return \herr_t
 *
 * \details H5ESget_count() retrieves number of events in an event set specified
 *          by \p es_id.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESget_count(hid_t es_id, size_t *count);

/**
 * \ingroup H5ES
 *
 * \brief Retrieves the accumulative operation counter for an event set
 *
 * \es_id
 * \param[out] counter The accumulative counter value for an event set
 * \return \herr_t
 *
 * \details H5ESget_op_counter() retrieves the current accumulative count of
 *          event set operations since the event set creation of \p es_id.
 *
 * \note This is designed for wrapper libraries mainly, to use as a mechanism
 *       for matching operations inserted into the event set with possible
 *       errors that occur.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESget_op_counter(hid_t es_id, uint64_t *counter);

/**
 * \ingroup H5ES
 *
 * \brief Checks for failed operations
 *
 * \es_id
 * \param[out] err_occurred Status indicating if error is present in the event
 *             set
 * \return \herr_t
 *
 * \details H5ESget_err_status() checks if event set specified by es_id has
 *          failed operations.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESget_err_status(hid_t es_id, hbool_t *err_occurred);

/**
 * \ingroup H5ES
 *
 * \brief Retrieves the number of failed operations
 *
 * \es_id
 * \param[out] num_errs Number of errors
 * \return \herr_t
 *
 * \details H5ESget_err_count() retrieves the number of failed operations in an
 *          event set specified by \p es_id.
 *
 *          The function does not wait for active operations to complete, so
 *          count may not include all failures.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESget_err_count(hid_t es_id, size_t *num_errs);

/**
 * \ingroup H5ES
 *
 * \brief Retrieves information about failed operations
 *
 * \es_id
 * \param[in] num_err_info The number of elements in \p err_info array
 * \param[out] err_info Array of structures
 * \param[out] err_cleared Number of cleared errors
 * \return \herr_t
 *
 * \details H5ESget_err_info() retrieves information about failed operations in
 *          an event set specified by \p es_id.  The strings retrieved for each
 *          error info must be released by calling H5free_memory().
 *
 *          Below is the description of the \ref H5ES_err_info_t structure:
 *          \snippet this H5ES_err_info_t_snip
 *          \click4more
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESget_err_info(hid_t es_id, size_t num_err_info, H5ES_err_info_t err_info[],
                               size_t *err_cleared);
/**
 * \ingroup H5ES
 *
 * \brief Convenience routine to free an array of H5ES_err_info_t structs
 *
 * \param[in] num_err_info The number of elements in \p err_info array
 * \param[in] err_info Array of structures
 * \return \herr_t
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESfree_err_info(size_t num_err_info, H5ES_err_info_t err_info[]);

/**
 * \ingroup H5ES
 *
 * \brief Registers a callback to invoke when a new operation is inserted into
 *        an event set
 *
 * \es_id
 * \param[in] func The insert function to register
 * \param[in] ctx User-specified information (context) to pass to \p func
 * \return \herr_t
 *
 * \details Only one insert callback can be registered for each event set.
 *          Registering a new callback will replace the existing one.
 *          H5ES_NONE is a valid value for 'es_id', but functions as a no-op
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESregister_insert_func(hid_t es_id, H5ES_event_insert_func_t func, void *ctx);

/**
 * \ingroup H5ES
 *
 * \brief Registers a callback to invoke when an operation completes within an
 *        event set
 *
 * \es_id
 * \param[in] func The completion function to register
 * \param[in] ctx User-specified information (context) to pass to \p func
 * \return \herr_t
 *
 * \details Only one complete callback can be registered for each event set.
 *          Registering a new callback will replace the existing one.
 *          H5ES_NONE is a valid value for 'es_id', but functions as a no-op
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESregister_complete_func(hid_t es_id, H5ES_event_complete_func_t func, void *ctx);

/**
 * \ingroup H5ES
 *
 * \brief Terminates access to an event set
 *
 * \es_id
 * \return \herr_t
 *
 * \details H5ESclose() terminates access to an event set specified by \p es_id.
 *
 * \since 1.14.0
 *
 */
H5_DLL herr_t H5ESclose(hid_t es_id);

#ifdef __cplusplus
}
#endif

#endif /* H5ESpublic_H */
