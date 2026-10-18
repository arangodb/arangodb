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
 * This file contains public declarations for the H5L (link) developer
 *      support routines.
 */

#ifndef H5Ldevelop_H
#define H5Ldevelop_H

/* Include package's public header */
#include "H5Lpublic.h"

/*****************/
/* Public Macros */
/*****************/

/**
 * \brief Current version of the H5L_class_t struct
 */
#define H5L_LINK_CLASS_T_VERS 1

/**
 * \brief Version of external link format
 */
#define H5L_EXT_VERSION 0

/**
 * \brief Valid flags for external links
 */
#define H5L_EXT_FLAGS_ALL 0

/*******************/
/* Public Typedefs */
/*******************/

/* The H5L_class_t struct can be used to override the behavior of a
 * "user-defined" link class. Users should populate the struct with callback
 * functions defined below.
 */
/* Callback prototypes for user-defined links */
/**
 * \brief Link creation callback
 */
typedef herr_t (*H5L_create_func_t)(const char *link_name, hid_t loc_group, const void *lnkdata,
                                    size_t lnkdata_size, hid_t lcpl_id);
/**
 * \brief Callback for link move
 */
typedef herr_t (*H5L_move_func_t)(const char *new_name, hid_t new_loc, const void *lnkdata,
                                  size_t lnkdata_size);
/**
 * \brief Callback for link copy
 */
typedef herr_t (*H5L_copy_func_t)(const char *new_name, hid_t new_loc, const void *lnkdata,
                                  size_t lnkdata_size);
/**
 * \brief Callback during link traversal
 */
typedef hid_t (*H5L_traverse_func_t)(const char *link_name, hid_t cur_group, const void *lnkdata,
                                     size_t lnkdata_size, hid_t lapl_id, hid_t dxpl_id);
/**
 * \brief Callback for link deletion
 */
typedef herr_t (*H5L_delete_func_t)(const char *link_name, hid_t file, const void *lnkdata,
                                    size_t lnkdata_size);
/**
 * \brief Callback for querying the link.
 *
 * Returns the size of the buffer needed.
 */
typedef ssize_t (*H5L_query_func_t)(const char *link_name, const void *lnkdata, size_t lnkdata_size,
                                    void *buf /*out*/, size_t buf_size);

/**
 * \brief Link prototype
 *
 * The H5L_class_t struct can be used to override the behavior of a
 * "user-defined" link class. Users should populate the struct with callback
 * functions defined elsewhere.
 */
//! <!-- [H5L_class_t_snip] -->
typedef struct {
    int                 version;     /**< Version number of this struct       */
    H5L_type_t          id;          /**< Link type ID                        */
    const char         *comment;     /**< Comment for debugging               */
    H5L_create_func_t   create_func; /**< Callback during link creation       */
    H5L_move_func_t     move_func;   /**< Callback after moving link          */
    H5L_copy_func_t     copy_func;   /**< Callback after copying link         */
    H5L_traverse_func_t trav_func;   /**< Callback during link traversal      */
    H5L_delete_func_t   del_func;    /**< Callback for link deletion          */
    H5L_query_func_t    query_func;  /**< Callback for queries                */
} H5L_class_t;
//! <!-- [H5L_class_t_snip] -->

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
 * \ingroup H5LA
 *
 * \brief Registers a user-defined link class or changes behavior of an
 *        existing class
 *
 * \param[in] cls Pointer to a buffer containing the struct describing the
 *            user-defined link class
 *
 * \return \herr_t
 *
 * \details H5Lregister() registers a class of user-defined links, or changes
 *          the behavior of an existing class.
 *
 *          \p cls is a pointer to a buffer containing a copy of the
 *          H5L_class_t struct. This struct is defined in H5Lpublic.h as
 *          follows:
 *          \snippet this H5L_class_t_snip
 *
 *          The class definition passed with \p cls must include at least the
 *          following:
 *          \li An H5L_class_t version (which should be #H5L_LINK_CLASS_T_VERS)
 *          \li A link class identifier, \c class_id
 *          \li A traversal function, \c trav_func
 *
 *          Remaining \c struct members are optional and may be passed as NULL.
 *
 *          The link class passed in \c class_id must be in the user-definable
 *          range between #H5L_TYPE_UD_MIN and #H5L_TYPE_UD_MAX
 *          (see the table below) and will override
 *          any existing link class with that identifier.
 *
 *          As distributed, valid values of \c class_id used in HDF5 include
 *          the following (defined in H5Lpublic.h):
 *          \link_types
 *
 *          The hard and soft link class identifiers cannot be modified or
 *          reassigned, but the external link class is implemented as an
 *          example in the user-definable link class identifier range.
 *          H5Lregister() is used to register additional link classes. It could
 *          also be used to modify the behavior of the external link class,
 *          though that is not recommended.
 *
 *          The following table summarizes existing link types and values and
 *          the reserved and user-definable link class identifier value ranges.
 *          <table>
 *            <tr>
 *              <th>Link class identifier or Value range</th>
 *              <th>Description</th>
 *              <th>Link class or label</th>
 *            </tr>
 *            <tr>
 *              <td>0 to 63</td>
 *              <td>Reserved range</td>
 *              <td></td>
 *            </tr>
 *            <tr>
 *              <td>64 to 255</td>
 *              <td>User-definable range</td>
 *              <td></td>
 *            </tr>
 *            <tr>
 *              <td>64</td>
 *              <td>Minimum user-defined value</td>
 *              <td>#H5L_TYPE_UD_MIN</td>
 *            </tr>
 *            <tr>
 *              <td>64</td>
 *              <td>External link</td>
 *              <td>#H5L_TYPE_EXTERNAL</td>
 *            </tr>
 *            <tr>
 *              <td>255</td>
 *              <td>Maximum user-defined value</td>
 *              <td>#H5L_TYPE_UD_MAX</td>
 *            </tr>
 *            <tr>
 *              <td>255</td>
 *              <td>Maximum value</td>
 *              <td>#H5L_TYPE_MAX</td>
 *            </tr>
 *            <tr>
 *              <td>-1</td>
 *              <td>Error</td>
 *              <td>#H5L_TYPE_ERROR</td>
 *            </tr>
 *          </table>
 *
 *          Note that HDF5 internally registers user-defined link classes only
 *          by the numeric value of the link class identifier. An application,
 *          on the other hand, will generally use a name for a user-defined
 *          class, if for no other purpose than as a variable name. Assume,
 *          for example, that a complex link type is registered with the link
 *          class identifier 73 and that the code includes the following
 *          assignment:
 *          \code
 *          H5L_TYPE_COMPLEX_A = 73
 *          \endcode
 *          The application can refer to the link class with a term,
 *          \c  H5L_TYPE_COMPLEX_A, that conveys meaning to a human reviewing
 *          the code, while HDF5 recognizes it by the more cryptic numeric
 *          identifier, 73.
 *
 * \attention Important details and considerations include the following:
 *            \li If you plan to distribute files or software with a
 *                user-defined link class, please contact the Help Desk at
 *                The HDF Group to help prevent collisions between \c class_id
 *                values. See below.
 *            \li As distributed with HDF5, the external link class is
 *                implemented as an example of a user-defined link class with
 *                #H5L_TYPE_EXTERNAL equal to #H5L_TYPE_UD_MIN. \c class_id in
 *                the H5L_class_t \c struct must not equal #H5L_TYPE_UD_MIN
 *                unless you intend to overwrite or modify the behavior of
 *                external links.
 *            \li H5Lregister() can be used only with link class identifiers
 *                in the user-definable range (see table above).
 *            \li The hard and soft links defined by the HDF5 library,
 *                #H5L_TYPE_HARD and #H5L_TYPE_SOFT, reside in the reserved
 *                range below #H5L_TYPE_UD_MIN and cannot be redefined or
 *                modified.
 *            \li H5Lis_registered() can be used to determine whether a desired
 *                link class identifier is available. \Emph{Note that this
 *                function will tell you only whether the link class identifier
 *                has been registered with the installed copy of HDF5; it
 *                cannot tell you whether the link class has been registered
 *                with The HDF Group.}
 *            \li #H5L_TYPE_MAX is the maximum allowed value for a link type
 *                identifier.
 *            \li #H5L_TYPE_UD_MIN equals #H5L_TYPE_EXTERNAL.
 *            \li #H5L_TYPE_UD_MAX equals #H5L_TYPE_MAX.
 *            \li #H5L_TYPE_ERROR indicates that an error has occurred.
 *
 * \note \Bold{Registration with The HDF Group:}\n
 *       There are sometimes reasons to take a broader approach to registering
 *       a user-defined link class than just invoking H5Lregister(). For
 *       example:
 *       \li A user-defined link class is intended for use across an
 *           organization, among collaborators, or across a community of users.
 *       \li An application or library overlying HDF5 invokes a user-defined
 *           link class that must be shipped with the software.
 *       \li Files are distributed that make use of a user-defined link class.
 *       \li Or simply, a specific user-defined link class is thought to be
 *           widely useful.
 *
 *       In such cases, you are encouraged to register that link class with
 *       The HDF Group's Helpdesk. The HDF Group maintains a registry of known
 *       user-defined link classes and tracks the selected link class
 *       identifiers. This registry is intended to reduce the risk of
 *       collisions between \c class_id values and to help coordinate the use
 *       of specialized link classes.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lregister(const H5L_class_t *cls);
/**
 * \ingroup H5LA
 *
 * \brief Unregisters a class of user-defined links
 *
 * \param[in] id User-defined link class identifier
 *
 * \return \herr_t
 *
 * \details H5Lunregister() unregisters a class of user-defined links,
 *          preventing them from being traversed, queried, moved, etc.
 *
 * \note A link class can be re-registered using H5Lregister().
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lunregister(H5L_type_t id);

#ifdef __cplusplus
}
#endif

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS

/** Previous versions of the H5L_class_t struct \since 1.10.3 */
#define H5L_LINK_CLASS_T_VERS_0 0

/** Callback during link traversal */
typedef hid_t (*H5L_traverse_0_func_t)(const char *link_name, hid_t cur_group, const void *lnkdata,
                                       size_t lnkdata_size, hid_t lapl_id);

/** User-defined link types */
typedef struct {
    int                   version;     /**< Version number of this struct        */
    H5L_type_t            id;          /**< Link type ID                         */
    const char           *comment;     /**< Comment for debugging                */
    H5L_create_func_t     create_func; /**< Callback during link creation        */
    H5L_move_func_t       move_func;   /**< Callback after moving link           */
    H5L_copy_func_t       copy_func;   /**< Callback after copying link          */
    H5L_traverse_0_func_t trav_func;   /**< Callback during link traversal       */
    H5L_delete_func_t     del_func;    /**< Callback for link deletion           */
    H5L_query_func_t      query_func;  /**< Callback for queries                 */
} H5L_class_0_t;

#endif /* H5_NO_DEPRECATED_SYMBOLS */

#endif /* H5Ldevelop_H */
