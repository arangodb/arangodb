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
 * Created:             H5Lpublic.h
 *
 * Purpose:             Public declarations for the H5L package (links)
 *
 *-------------------------------------------------------------------------
 */
#ifndef H5Lpublic_H
#define H5Lpublic_H

#include "H5public.h"  /* Generic Functions                        */
#include "H5Ipublic.h" /* Identifiers                              */
#include "H5Opublic.h" /* Object Headers                           */
#include "H5Tpublic.h" /* Datatypes                                */

/*****************/
/* Public Macros */
/*****************/

/** Maximum length of a link's name \since 1.8.0 */
#define H5L_MAX_LINK_NAME_LEN UINT32_MAX

/** Macro to indicate operation occurs on same location \since 1.8.0 */
#define H5L_SAME_LOC 0 /* (hid_t) */

#ifdef __cplusplus
extern "C" {
#endif

/*******************/
/* Public Typedefs */
/*******************/

/**
 * \brief  Link class types.
 *
 * Values less than 64 are reserved for the HDF5 library's internal use. Values
 * 64 to 255 are for "user-defined" link class types; these types are defined
 * by HDF5 but their behavior can be overridden by users. Users who want to
 * create new classes of links should contact the HDF5 development team at
 * mailto:help@hdfgroup.org. These values can never change because they appear
 * in HDF5 files.
 */
typedef enum {
    H5L_TYPE_ERROR    = (-1), /**< Invalid link type id         */
    H5L_TYPE_HARD     = 0,    /**< Hard link id                 */
    H5L_TYPE_SOFT     = 1,    /**< Soft link id                 */
    H5L_TYPE_EXTERNAL = 64,   /**< External link id             */
    H5L_TYPE_MAX      = 255   /**< Maximum link type id         */
} H5L_type_t;

/** Maximum value link value for "built-in" link types \since 1.8.0 */
#define H5L_TYPE_BUILTIN_MAX H5L_TYPE_SOFT
/** Link ids at or above this value are "user-defined" link types. \since 1.8.0 */
#define H5L_TYPE_UD_MIN H5L_TYPE_EXTERNAL
/** Maximum link id value for "user-defined" link types. \since 1.12.1 */
#define H5L_TYPE_UD_MAX H5L_TYPE_MAX

/**
 * \brief Information struct for links
 */
//! <!-- [H5L_info2_t_snip] -->
typedef struct {
    H5L_type_t type;         /**< Type of link                   */
    hbool_t    corder_valid; /**< Indicate if creation order is valid */
    int64_t    corder;       /**< Creation order                 */
    H5T_cset_t cset;         /**< Character set of link name     */
    union {
        H5O_token_t token;    /**< Token of location that hard link points to */
        size_t      val_size; /**< Size of a soft link or user-defined link value */
    } u;
} H5L_info2_t;
//! <!-- [H5L_info2_t_snip] -->

/**
 * \brief Prototype for H5Literate2(), H5Literate_by_name2() operator
 *
 * The H5O_token_t version is used in the VOL layer and future public API calls.
 */
//! <!-- [H5L_iterate2_t_snip] -->
typedef herr_t (*H5L_iterate2_t)(hid_t group, const char *name, const H5L_info2_t *info, void *op_data);
//! <!-- [H5L_iterate2_t_snip] -->

/**
 * \brief Callback for external link traversal
 */
typedef herr_t (*H5L_elink_traverse_t)(const char *parent_file_name, const char *parent_group_name,
                                       const char *child_file_name, const char *child_object_name,
                                       unsigned *acc_flags, hid_t fapl_id, void *op_data);

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/
/**
 * \ingroup H5L
 *
 * \brief Moves a link within an HDF5 file
 *
 * \fgdta_loc_id{src_loc}
 * \param[in] src_name Original link name
 * \fgdta_loc_id{dst_loc}
 * \param[in] dst_name New link name
 * \lcpl_id
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lmove() moves a link within an HDF5 file. The original link,
 *          \p src_name, is removed from \p src_loc and the new link,
 *          \p dst_name, is inserted at dst_loc. This change is
 *          accomplished as an atomic operation.
 *
 *          \p src_loc and \p src_name identify the original link.
 *          \p src_loc is the original location identifier; \p src_name is
 *          the path to the link and is interpreted relative to \p src_loc.
 *
 *          \p dst_loc and \p dst_name identify the new link. \p dst_loc is
 *          either a file or group identifier; \p dst_name is the path to
 *          the link and is interpreted relative to \p dst_loc.
 *
 *          \p lcpl_id and \p lapl_id are the link creation and link access
 *          property lists, respectively, associated with the new link,
 *          \p dst_name.
 *
 *          Through these property lists, several properties are available to
 *          govern the behavior of H5Lmove(). The property controlling creation
 *          of missing intermediate groups is set in the link creation property
 *          list with H5Pset_create_intermediate_group(); H5Lmove() ignores any
 *          other properties in the link creation property list. Properties
 *          controlling character encoding, link traversals, and external link
 *          prefixes are set in the link access property list with
 *          H5Pset_char_encoding(), H5Pset_nlinks(), and H5Pset_elink_prefix(),
 *          respectively.
 *
 * \note Note that H5Lmove() does not modify the value of the link; the new
 *       link points to the same object as the original link pointed to.
 *       Furthermore, if the object pointed to by the original link was already
 *       open with a valid object identifier, that identifier will remain valid
 *       after the call to H5Lmove().
 *
 * \attention Exercise care in moving links as it is possible to render data in
 *            a file inaccessible with H5Lmove(). If the link being moved is on
 *            the only path leading to an HDF5 object, that object may become
 *            permanently inaccessible in the file.
 *
 * \since 1.8.0
 *
 *-------------------------------------------------------------------------
 */
H5_DLL herr_t H5Lmove(hid_t src_loc, const char *src_name, hid_t dst_loc, const char *dst_name, hid_t lcpl_id,
                      hid_t lapl_id);
/**
 * \ingroup H5L
 *
 * \brief Creates an identical copy of a link with the same creation time and
 *        target.  The new link can have a different name and be in a different
 *        location than the original.
 *
 * \fgdt_loc_id{src_loc}
 * \param[in] src_name   Name of the link to be copied
 * \fgdt_loc_id{dst_loc}
 * \param[in] dst_name   Name to be assigned to the new copy
 * \lcpl_id
 * \lapl_id
 * \return \herr_t
 *
 * \details H5Lcopy() copies the link specified by \p src_name from the location
 *          specified by \p src_loc_id to the location specified by
 *          \p dst_loc_id. The new copy of the link is created with the name
 *          \p dst_name.
 *
 *          If \p dst_loc_id is a file identifier, \p dst_name will be
 *          interpreted relative to that file's root group.
 *
 *          The new link is created with the creation and access property lists
 *          specified by \p lcpl_id and \p lapl_id. The interpretation of
 *          \p lcpl_id is limited in the manner described in the next paragraph.
 *
 *          H5Lcopy() retains the creation time and the target of the original
 *          link. However, since the link may be renamed, the character
 *          encoding is specified in \p lcpl_id rather than in that of the
 *          original link. Other link creation properties are ignored.
 *
 *          If the link is a soft link, also known as a symbolic link, its
 *          target is interpreted relative to the location of the copy.
 *
 *          Several properties are available to govern the behavior of
 *          H5Lcopy(). These properties are set in the link creation and access
 *          property lists, \p lcpl_id and \p lapl_id, respectively. The
 *          property controlling creation of missing intermediate groups is set
 *          in the link creation property list with
 *          H5Pset_create_intermediate_group(); this function ignores any
 *          other properties in the link creation property list. Properties
 *          controlling character encoding, link traversals, and external link
 *          prefixes are set in the link access property list with
 *          H5Pset_char_encoding(), H5Pset_nlinks(), and
 *          H5Pset_elink_prefix().
 *
 * \note H5Lcopy() does not affect the object that the link points to.
 *
 * \attention H5Lcopy() cannot copy hard links across files as a hard link is
 *            not valid without a target object; to copy objects from one file
 *            to another, see H5Ocopy().
 *
 * \see H5Ocopy()
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lcopy(hid_t src_loc, const char *src_name, hid_t dst_loc, const char *dst_name, hid_t lcpl_id,
                      hid_t lapl_id);
/**
 * \ingroup H5L
 *
 * \brief Creates a hard link to an object
 *
 * \fgdta_loc_id{cur_loc}
 * \param[in] cur_name Name of the target object, which must already exist
 * \fgdta_loc_id{dst_loc}
 * \param[in] dst_name The name of the new link
 * \lcpl_id
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lcreate_hard() creates a new hard link to a pre-existing object
 *          in an HDF5 file.
 *
 *          \p cur_loc and \p cur_name specify the location
 *          and name, respectively, of the target object, i.e., the object that
 *          the new hard link points to. \p dst_loc and \p dst_name specify the
 *          location and name, respectively, of the new hard link.
 *
 *          \p cur_name and \p dst_name are interpreted relative to \p cur_loc
 *          and \p dst_loc, respectively. If a given name begins with \c /,
 *          then it will be interpreted as absolute path in the file.
 *          The names of the created links will be the last element of
 *          each provided path. Prior elements in each path are used to
 *          locate the parent groups of each new link. If \p cur_loc and
 *          \p dst_loc are the same location, the HDF5 macro
 *          #H5L_SAME_LOC can be used for either parameter (but not both).
 *
 *          \p lcpl_id and \p lapl_id are the link creation and access property
 *          lists associated with the new link.
 *
 * \note Hard and soft links are for use only if the target object is in the
 *       current file. If the desired target object is in a different file from
 *       the new link, an external link may be created with
 *       H5Lcreate_external().
 *
 * \note The HDF5 library keeps a count of all hard links pointing to an
 *       object; if the hard link count reaches zero (0), the object will be
 *       deleted from the file. Creating new hard links to an object will
 *       prevent it from being deleted if other links are removed. The
 *       library maintains no similar count for soft links and they can dangle.
 *
 * \note The new link may be one of many that point to that object.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lcreate_hard(hid_t cur_loc, const char *cur_name, hid_t dst_loc, const char *dst_name,
                             hid_t lcpl_id, hid_t lapl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Lcreate_hard}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Lcreate_hard_async(const char *app_file, const char *app_func, unsigned app_line,
                                   hid_t cur_loc_id, const char *cur_name, hid_t new_loc_id,
                                   const char *new_name, hid_t lcpl_id, hid_t lapl_id, hid_t es_id);
#else
H5_DLL herr_t H5Lcreate_hard_async(hid_t cur_loc_id, const char *cur_name, hid_t new_loc_id,
                                   const char *new_name, hid_t lcpl_id, hid_t lapl_id, hid_t es_id);
#endif
/**
 * \ingroup H5L
 *
 * \brief Creates a soft link
 *
 * \param[in] link_target An HDF5 path name
 * \fgdta_loc_id{link_loc_id}
 * \param[in] link_name The name of the new link
 * \lcpl_id
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lcreate_soft() creates a new soft link to an object in an HDF5
 *          file.
 *
 *          \p link_target specifies the HDF5 path name the soft link contains.
 *          \p link_target can be an arbitrary HDF5 path name and is
 *          interpreted only at lookup time. This path may be absolute in the
 *          file or relative to \p link_loc_id.
 *
 *          \p link_loc_id and \p link_name specify the location and name,
 *          respectively, of the new soft link. \p link_name is interpreted
 *          as a path relative to \p link_loc_id, or an absolute path if it
 *          begins with \c /. The name of the created link will be the last
 *          element of the provided path. Prior elements in the path are
 *          used to locate the parent group of the new link.
 *
 *          If \p link_loc_id is a group identifier, the object pointed to by
 *          \p link_name will be accessed as a member of that group. If
 *          \p link_loc_id is a file identifier, the object will be accessed as a
 *          member of the file's root group.
 *
 *          \p lcpl_id and \p lapl_id are the link creation and access property
 *          lists associated with the new link.
 *
 *          For instance, if target_path is \c ./foo, \p link_loc_id specifies
 *          \c ./x/y/bar, and the name of the new link is \c new_link, then a
 *          subsequent request for \c ./x/y/bar/new_link will return the same
 *          object as would be found at \c ./foo.
 *
 * \note H5Lcreate_soft() is for use only if the target object is in the
 *       current file. If the desired target object is in a different file from
 *       the new link, use H5Lcreate_external() to create an external link.
 *
 * \note Soft links and external links are also known as symbolic links as they
 *       use a name to point to an object; hard links employ an object's
 *       address in the file.
 *
 * \note Unlike hard links, a soft link in an HDF5 file is allowed to dangle,
 *       meaning that the target object need not exist at the time that the
 *       link is created.
 *
 * \note The HDF5 library does not keep a count of soft links as it does of
 *       hard links.
 *
 * \note The new link may be one of many that point to that object.
 *
 * \see H5Lcreate_hard(), H5Lcreate_external()
 *
 * \since 1.8.0
 *

 */
H5_DLL herr_t H5Lcreate_soft(const char *link_target, hid_t link_loc_id, const char *link_name, hid_t lcpl_id,
                             hid_t lapl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Lcreate_soft}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Lcreate_soft_async(const char *app_file, const char *app_func, unsigned app_line,
                                   const char *link_target, hid_t link_loc_id, const char *link_name,
                                   hid_t lcpl_id, hid_t lapl_id, hid_t es_id);
#else
H5_DLL herr_t H5Lcreate_soft_async(const char *link_target, hid_t link_loc_id, const char *link_name,
                                   hid_t lcpl_id, hid_t lapl_id, hid_t es_id);
#endif
/**
 * \ingroup H5L
 *
 * \brief Removes a link from a group
 *
 * \fgdta_loc_id
 * \param[in] name Name of the link to delete
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Ldelete() removes the link specified by \p name from the location
 *          \p loc_id.
 *
 *          If the link being removed is a hard link, H5Ldelete() also
 *          decrements the link count for the object to which name points.
 *          Unless there is a duplicate hard link in that group, this action
 *          removes the object to which name points from the group that
 *          previously contained it.
 *
 *          Object headers keep track of how many hard links refer to an
 *          object; when the hard link count, also referred to as the reference
 *          count, reaches zero, the object can be removed from the file. The
 *          file space associated will then be released, i.e., identified in
 *          memory as freespace. Objects which are open are not removed until
 *          all identifiers to the object are closed.
 *
 * \attention Exercise caution in the use of H5Ldelete(); if the link being
 *            removed is on the only path leading to an HDF5 object, that
 *            object may become permanently inaccessible in the file.
 *
 * \see H5Lcreate_hard(), H5Lcreate_soft(), H5Lcreate_external()
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Ldelete(hid_t loc_id, const char *name, hid_t lapl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Ldelete}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Ldelete_async(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                              const char *name, hid_t lapl_id, hid_t es_id);
#else
H5_DLL herr_t H5Ldelete_async(hid_t loc_id, const char *name, hid_t lapl_id, hid_t es_id);
#endif
/**
 * \ingroup H5L
 *
 * \brief Removes the \Emph{n}-th link in a group
 *
 * \fgdta_loc_id
 * \param[in] group_name Name of subject group
 * \param[in] idx_type Index or field which determines the order
 * \param[in] order Order within field or index
 * \param[in] n Link for which to retrieve information
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Ldelete_by_idx() removes the \Emph{n}-th link in a group
 *          according to the specified order, \p order, in the specified index,
 *          \p index.
 *
 *          If \p loc_id specifies the group in which the link resides,
 *          \p group_name can be a dot (\c .).
 *
 * \see H5Ldelete()
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Ldelete_by_idx(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                               H5_iter_order_t order, hsize_t n, hid_t lapl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Ldelete_by_idx}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Ldelete_by_idx_async(const char *app_file, const char *app_func, unsigned app_line,
                                     hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                     H5_iter_order_t order, hsize_t n, hid_t lapl_id, hid_t es_id);
#else
H5_DLL herr_t H5Ldelete_by_idx_async(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                     H5_iter_order_t order, hsize_t n, hid_t lapl_id, hid_t es_id);
#endif
/**
 * \ingroup H5L
 *
 * \brief Returns the value of a link
 *
 * \fgdta_loc_id
 * \param[in] name Link name
 * \param[out] buf The buffer to hold the link value
 * \param[in] size Maximum number of bytes of link value to be returned
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lget_val() returns the value of link \p name. For symbolic links,
 *          this is the path to which the link points, including the null
 *          terminator. For external and user-defined links, it is the link
 *          buffer.
 *
 *          \p size is the size of \p buf and should be the size of the link
 *          value being returned. This size value can be determined through a
 *          call to H5Lget_info(); it is returned in the \c val_size field of
 *          the \ref H5L_info_t \c struct.
 *
 *          If \p size is smaller than the size of the returned value, then the
 *          string stored in \p buf will be truncated to \p size bytes. For
 *          soft links, this means that the value will not be null terminated.
 *
 *          In the case of external links, the target file and object names are
 *          extracted from \p buf by calling H5Lunpack_elink_val().
 *
 *          The link class of link \p name can be determined with a call to
 *          H5Lget_info().
 *
 *          \p lapl_id specifies the link access property list associated with
 *          the link \p name. In the general case, when default link access
 *          properties are acceptable, this can be passed in as #H5P_DEFAULT. An
 *          example of a situation that requires a non-default link access
 *          property list is when the link is an external link; an external
 *          link may require that a link prefix be set in a link access
 *          property list (see H5Pset_elink_prefix()).
 *
 *          This function should be used only after H5Lget_info() has been
 *          called to verify that \p name is a symbolic link. This can be
 *          determined from the \c link_type field of the \ref H5L_info_t
 *          \c struct.
 *
 * \note This function will fail if called on a hard link.
 *
 * \see H5Lget_val_by_idx()
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lget_val(hid_t loc_id, const char *name, void *buf /*out*/, size_t size, hid_t lapl_id);
/**
 * \ingroup H5L
 *
 * \brief Retrieves value of the \Emph{n}-th link in a group, according to the order within an index
 *
 * \fgdta_loc_id
 * \param[in] group_name Group name
 * \param[in] idx_type Type of index
 * \param[in] order Order within field or index
 * \param[in] n Link position for which to retrieve information
 * \param[out] buf The buffer to hold the link value
 * \param[in] size Maximum number of bytes of link value to be returned
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lget_val_by_idx() retrieves the value of the \Emph{n}-th link in
 *          a group, according to the specified order, \p order, within an
 *          index, \p index.
 *
 *          For soft links, the value is an HDF5 path name.
 *
 *          For external links, this is a compound value containing file and
 *          path name information; to use this external link information, it
 *          must first be decoded with H5Lunpack_elink_val()
 *
 *          For user-defined links, this value will be described in the
 *          definition of the user-defined link type.
 *
 *          \p loc_id specifies the location identifier of the group specified
 *          by \p group_name.
 *
 *          \p group_name specifies the group in which the link exists. If
 *          \p loc_id already specifies the group in which the link exists,
 *          \p group_name must be a dot (\c .).
 *
 *          The size in bytes of link_val is specified in \p size. The size
 *          value can be determined through a call to H5Lget_info_by_idx(); it
 *          is returned in the \c val_size field of the \ref H5L_info_t
 *          \c struct. If
 *          size is smaller than the size of the returned value, then the
 *          string stored in link_val will be truncated to size bytes. For soft
 *          links, this means that the value will not be null terminated.
 *
 *          If the type of the link is unknown or uncertain, H5Lget_val_by_idx()
 *          should be called only after the type has been determined via a call
 *          to H5Lget_info_by_idx().
 *
 * \note This function will fail if called on a hard link.
 *
 * \see H5Lget_val()
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lget_val_by_idx(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                H5_iter_order_t order, hsize_t n, void *buf /*out*/, size_t size,
                                hid_t lapl_id);
/**
 * \ingroup H5L
 *
 * \brief Determines whether a link with the specified name exists in a group
 *
 * \fgdta_loc_id
 * \param[in] name Link name
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lexists() allows an application to determine whether the link \p
 *          name exists in the location specified by \p loc_id. The link may be
 *          of any type; only the presence of a link with that name is checked.
 *
 *          Note that H5Lexists() verifies only that the target link exists. If
 *          name includes either a relative path or an absolute path to the
 *          target link, intermediate steps along the path must be verified
 *          before the existence of the target link can be safely checked. If
 *          the path is not verified, and an intermediate element of the path
 *          does not exist, H5Lexists() will fail. The example in the next
 *          paragraph illustrates one step-by-step method for verifying the
 *          existence of a link with a relative or absolute path.
 *
 *          \Bold{Example:} Use the following steps to verify the existence of
 *          the link \c datasetD in the \c group group1/group2/softlink_to_group3/,
 *          where \c group1 is a member of the group specified by \c loc_id:
 *
 *          1. First, use H5Lexists() to verify that the \c group1 exists.
 *          2. If \c group1 exists, use H5Lexists() again, this time with name
 *             set to \c group1/group2, to verify that \c group2 exists.
 *          3. If \c group2 exists, use H5Lexists() with name set to
 *             \c group1/group2/softlink_to_group3 to verify that
 *             \c softlink_to_group3 exists.
 *          4. If \c softlink_to_group3 exists, you can now safely use
 *             H5Lexists() with \c name set to
 *             \c group1/group2/softlink_to_group3/datasetD to verify that the
 *             target link, \c datasetD, exists.
 *
 *          If the link to be verified is specified with an absolute path, the
 *          same approach should be used, but starting with the first link in
 *          the file's root group. For instance, if \c datasetD were in
 *          \c /group1/group2/softlink_to_group3, the first call to H5Lexists()
 *          would have name set to \c /group1.
 *
 *          Note that this is an outline and does not include all the necessary
 *          details. Depending on circumstances, for example, you may need to
 *          verify that an intermediate link points to a group and that a soft
 *          link points to an existing target.
 *
 * \note The behavior of H5Lexists() was changed in the 1.10 release in the
 *       case where the root group, \c "/", is the name of the link. This
 *       change is described below:
 *       <ol>
 *       <li>Let \c file denote a valid HDF5 file identifier, and let \c lapl
 *          denote a valid link access property list identifier. A call to
 *          H5Lexists() with arguments \c file, \c "/", and \c lapl
 *          returns a positive value; in other words,
 *          \TText{H5Lexists(file, "/", lapl)} returns a positive value.
 *          In the HDF5 1.8 release, this function returns 0.</li>
 *       <li>Let \c root denote a valid HDF5 group identifier that refers to the
 *          root group of an HDF5 file, and let \c lapl denote a valid link
 *          access property list identifier. A call to H5Lexists() with
 *          arguments c root, \c "/", and \c lapl returns a positive value;
 *          in other words, \TText{H5Lexists(root, "/", lapl)} returns a positive
 *          value. In the HDF5 1.8 release, this function returns 0.</li>
 *       </ol>
 *       Note that the function accepts link names and path names. This is
 *       potentially misleading to callers, and we plan to separate the
 *       functionality for link names and path names in a future release.
 *
 * \attention H5Lexists() checks the existence of only the final element in a
 *            relative or absolute path; it does not check any other path
 *            elements. The function will therefore fail when both of the
 *            following conditions exist:
 *            - \c name is not local to the group specified by \c loc_id or,
 *              if \c loc_id is something other than a group identifier, \c name
 *              is not local to the root group.
 *            - Any element of the relative path or absolute path in name,
 *              except the target link, does not exist.
 *
 * \version 1.10.0 Function behavior changed in this release. (See the note.)
 * \since 1.8.0
 *
 */
H5_DLL htri_t H5Lexists(hid_t loc_id, const char *name, hid_t lapl_id);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Lexists}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Lexists_async(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                              const char *name, hbool_t *exists, hid_t lapl_id, hid_t es_id);
#else
H5_DLL herr_t H5Lexists_async(hid_t loc_id, const char *name, hbool_t *exists, hid_t lapl_id, hid_t es_id);
#endif
/**
 * \ingroup H5L
 *
 * \brief Returns information about a link
 *
 * \fgdta_loc_id
 * \param[in] name Link name
 * \param[out] linfo Buffer in which link information is returned
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lget_info2() returns information about the specified link through
 *          the \p linfo argument.
 *
 *          The location identifier, \p loc_id, specifies the location of the
 *          link. A link name, \p name, interpreted relative to \p loc_id,
 *          specifies the link being queried.
 *
 *          \p lapl_id is the link access property list associated with the
 *          link name. In the general case, when default link access properties
 *          are acceptable, this can be passed in as #H5P_DEFAULT. An example
 *          of a situation that requires a non-default link access property
 *          list is when the link is an external link; an external link may
 *          require that a link prefix be set in a link access property list
 *          (see H5Pset_elink_prefix()).
 *
 *          H5Lget_info2() returns information about name in the data structure
 *          H5L_info2_t, which is described below and defined in H5Lpublic.h.
 *          This structure is returned in the buffer \p linfo.
 *          \snippet this H5L_info2_t_snip
 *          In the above struct, \c type specifies the link class. Valid values
 *          include the following:
 *          \link_types
 *          There will be additional valid values if user-defined links have
 *          been registered.
 *
 *          \p corder specifies the link's creation order position, while
 *          \p corder_valid indicates whether the value in corder is valid.
 *
 *          If \p corder_valid is \c true, the value in \p corder is known to
 *          be valid; if \p corder_valid is \c false, the value in \p corder is
 *          presumed to be invalid; \p corder starts at zero (0) and is
 *          incremented by one (1) as new links are created. But
 *          higher-numbered entries are not adjusted when a lower-numbered link
 *          is deleted; the deleted link's creation order position is simply
 *          left vacant. In such situations, the value of \p corder for the
 *          last link created will be larger than the number of links remaining
 *          in the group.
 *
 *          \p cset specifies the character set in which the link name is
 *          encoded. Valid values include the following:
 *          \csets
 *          This value is set with #H5Pset_char_encoding.
 *
 *          \c token is the location that a hard link points to, and
 *          \c val_size is the size of a soft link or user-defined link value.
 *          H5O_token_t is used in the VOL layer. It is defined in H5public.h
 *          as:
 *          \snippet H5public.h H5O_token_t_snip
 *
 *          If the link is a symbolic link, \c val_size will be the length of
 *          the link value, e.g., the length of the HDF5 path name with a null
 *          terminator.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Lget_info2(hid_t loc_id, const char *name, H5L_info2_t *linfo, hid_t lapl_id);
/**
 * \ingroup H5L
 *
 * \brief Retrieves metadata for a link in a group, according to the order
 *        within a field or index
 *
 * \loc_id
 * \param[in] group_name Group name
 * \idx_type
 * \order
 * \param[in] n Link position for which to retrieve information
 * \param[out] linfo Buffer in which link information is returned
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lget_info_by_idx2() returns the metadata for a link in a group
 *          according to a specified field or index and a specified order. The
 *          link for which information is to be returned is specified by \p
 *          idx_type, \p order, and \p n as follows:
 *
 *          - \p idx_type specifies the field by which the links in \p
 *            group_name are ordered. The links may be indexed on this field,
 *            in which case operations seeking specific links are likely to
 *            complete more quickly.
 *          - \p order specifies the order in which
 *            the links are to be referenced for the purposes of this function.
 *          - \p n specifies the position of the subject link. Note that this
 *            count is zero-based; 0 (zero) indicates that the function will
 *            return the value of the first link; if \p n is 5, the function
 *            will return the value of the sixth link; etc.
 *
 *          For example, assume that \p idx_type, \p order, and \p n are
 *          #H5_INDEX_NAME, #H5_ITER_DEC, and 5, respectively. #H5_INDEX_NAME
 *          indicates that the links are accessed in lexicographic order by
 *          their names. #H5_ITER_DEC specifies that the list be traversed in
 *          reverse order, or in decremented order. And 5 specifies that this
 *          call to the function will return the metadata for the 6th link
 *          (\c n + 1) from the end.
 *
 *          See H5Literate2() for a list of valid values and further discussion
 *          regarding \p idx_type and \p order.
 *
 *          If \p loc_id specifies the group in which the link resides,
 *          \p group_name can be a dot (\c .).
 *
 * \since 1.12.0
 *
 * \see H5Lget_info2()
 *
 */
H5_DLL herr_t H5Lget_info_by_idx2(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                  H5_iter_order_t order, hsize_t n, H5L_info2_t *linfo, hid_t lapl_id);
/**
 * \ingroup H5L
 *
 * \brief Retrieves name of the \Emph{n}-th link in a group, according to the
 *        order within a specified field or index
 *
 * \loc_id
 * \param[in] group_name Group name
 * \idx_type
 * \order
 * \param[in] n Link position for which to retrieve information
 * \param[out] name Buffer in which link name is returned
 * \param[in] size Size in bytes of \p name
 * \lapl_id
 *
 * \return Returns the size of the link name if successful; otherwise returns a
 *         negative value.
 *
 * \details H5Lget_name_by_idx() retrieves the name of the \Emph{n}-th link in a
 *          group, according to the specified order, \p order, within a specified
 *          field or index, \p idx_type.
 *
 *          \p idx_type specifies the index that is used. Valid values include
 *          the following:
 *          \indexes
 *
 *          \p order specifies the order in which objects are inspected along
 *          the index specified in \p idx_type. Valid values include the
 *          following:
 *          \orders
 *
 *          If \p loc_id specifies the group in which the link resides,
 *          \p group_name can be a dot (\c .).
 *
 *          \details_namelen{link,H5Lget_name_by_idx}
 *
 * \note Please note that in order for the specified index to correspond to the
 *       creation order index, \p order must be set to #H5_ITER_INC or
 *       #H5_ITER_DEC when calling H5Lget_name_by_idx(). \note The index \p n
 *       passed to H5Lget_name_by_idx() is the index of the link within the
 *       link table, sorted according to \p order and \p idx_type. If order is
 *       #H5_ITER_NATIVE, then the link table is not sorted, and it does not
 *       matter what \p idx_type is. Specifying #H5_ITER_NATIVE does not
 *       guarantee any particular order, only that it remains consistent.
 *
 * \since 1.8.0
 *
 */
H5_DLL ssize_t H5Lget_name_by_idx(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                  H5_iter_order_t order, hsize_t n, char *name /*out*/, size_t size,
                                  hid_t lapl_id);
/**
 * \ingroup TRAV
 *
 * \brief Iterates over links in a group, with user callback routine,
 *        according to the order within an index.
 *
 * \group_id{grp_id}
 * \idx_type
 * \order
 * \param[in,out] idx Pointer to an iteration index to allow
 *                    continuing a previous iteration
 * \op
 * \op_data
 * \return \success{The return value of the first operator that returns
 *                  non-zero, or zero if all members were processed with no
 *                  operator returning non-zero.}
 * \return \failure{Negative if an error occurs in the library, or the negative
 *                  value returned by one of the operators.}
 *
 * \details H5Literate2() iterates through the links in a file or
 *          group, \p group_id, in the order of the specified
 *          index, \p idx_type, using a user-defined callback routine
 *          \p op. H5Literate2() does not recursively follow links into
 *          subgroups of the specified group.
 *
 *          Three parameters are used to manage progress of the iteration:
 *          \p idx_type, \p order, and \p idx_p.
 *
 *          \p idx_type specifies the index to be used. If the links have
 *          not been indexed by the index type, they will first be sorted by
 *          that index then the iteration will begin; if the links have been
 *          so indexed, the sorting step will be unnecessary, so the iteration
 *          may begin more quickly. Valid values include the following:
 *          \indexes
 *
 *          \p order specifies the order in which objects are to be inspected
 *          along the index \p idx_type. Valid values include the following:
 *          \orders
 *
 *          \p idx_p tracks the iteration and allows an iteration to be
 *          resumed if it was stopped before all members were processed. It is
 *          passed in by the application with a starting point and returned by
 *          the library with the point at which the iteration stopped.
 *
 *          \p op_data is a user-defined pointer to the data required to
 *          process links in the course of the iteration. This pointer is
 *          passed back to each step of the iteration in the \p op callback
 *          function's \p op_data parameter. \p op is invoked for each link
 *          encounter.
 *
 *          \p op_data is passed to and from each iteration and can be used to
 *          supply or aggregate information across iterations.
 *
 * \remark Same pattern of behavior as H5Giterate().
 *
 * \note This function is also available through the H5Literate() macro.
 *
 * \warning The behavior of H5Literate2() is undefined if the link
 *          membership of \p group_id changes during the iteration.
 *          This does not limit the ability to change link destinations
 *          while iterating, but caution is advised.
 *
 * \callback_note
 *
 * \since 1.12.0
 *
 * \see H5Literate_by_name2(), H5Lvisit2(), H5Lvisit_by_name2()
 *
 */
H5_DLL herr_t H5Literate2(hid_t grp_id, H5_index_t idx_type, H5_iter_order_t order, hsize_t *idx,
                          H5L_iterate2_t op, void *op_data);
/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 *
 * \warning The returned value of the callback routine op will not be set
 *          in the return value for H5Literate_async(), so the \p herr_t value
 *          should not be used for determining the return state of the callback routine.
 *
 * \async_variant_of{H5Literate}
 */
#ifndef H5_DOXYGEN
H5_DLL herr_t H5Literate_async(const char *app_file, const char *app_func, unsigned app_line, hid_t group_id,
                               H5_index_t idx_type, H5_iter_order_t order, hsize_t *idx_p, H5L_iterate2_t op,
                               void *op_data, hid_t es_id);
#else
H5_DLL herr_t H5Literate_async(hid_t group_id, H5_index_t idx_type, H5_iter_order_t order, hsize_t *idx_p,
                               H5L_iterate2_t op, void *op_data, hid_t es_id);
#endif
/**
 * \ingroup TRAV
 *
 * \brief Iterates through links in a group
 *
 * \loc_id
 * \param[in] group_name Group name
 * \idx_type
 * \order
 * \param[in,out] idx iteration position at which to start (\Emph{IN}) or
 *                    position at which an interrupted iteration may be restarted
 *                    (\Emph{OUT})
 * \op
 * \op_data
 * \lapl_id
 * \return \success{The return value of the first operator that returns
 *                  non-zero, or zero if all members were processed with no
 *                  operator returning non-zero.}
 * \return \failure{Negative if an error occurs in the library, or the negative
 *                  value returned by one of the operators.}
 *
 * \details H5Literate_by_name2() iterates through the links in a group
 *          specified by \p loc_id and \p group_name, in the order of the
 *          specified index, \p idx_type, using a user-defined callback routine
 *          \p op. H5Literate_by_name2() does not recursively follow links into
 *          subgroups of the specified group.
 *
 *          \p idx_type specifies the index to be used. If the links have not
 *          been indexed by the index type, they will first be sorted by that
 *          index then the iteration will begin; if the links have been so
 *          indexed, the sorting step will be unnecessary, so the iteration may
 *          begin more quickly. Valid values include the following:
 *          \indexes
 *
 *          \p order specifies the order in which objects are to be inspected
 *          along the index specified in \p idx_type. Valid values include the
 *          following:
 *          \orders
 *
 *          \p idx_p allows an interrupted iteration to be resumed; it is
 *          passed in by the application with a starting point and returned by
 *          the library with the point at which the iteration stopped.
 *
 * \warning H5Literate_by_name2() assumes that the membership of the group being
 *          iterated over remains unchanged through the iteration; if any of the
 *          links in the group change during the iteration, the function's
 *          behavior is undefined. Note, however, that objects pointed to by the
 *          links can be modified.
 *
 * \note H5Literate_by_name2() is not recursive. In particular, if a member of
 *       \p group_name is found to be a group, call it \c subgroup_a,
 *       H5Literate_by_name2() does not examine the members of \c subgroup_a.
 *       When recursive iteration is required, the application must handle the
 *       recursion, explicitly calling H5Literate_by_name2() on discovered
 *       subgroups.
 *
 * \note H5Literate_by_name2() is the same as H5Literate2(), except that
 *       H5Literate2() always proceeds in alphanumeric order.
 *
 * \callback_note
 *
 * \since 1.12.0
 *
 * \see H5Literate(), H5Lvisit()
 *
 */
H5_DLL herr_t H5Literate_by_name2(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                  H5_iter_order_t order, hsize_t *idx, H5L_iterate2_t op, void *op_data,
                                  hid_t lapl_id);
/**
 * \ingroup TRAV
 *
 * \brief Recursively visits all links starting from a specified group
 *
 * \group_id{grp_id}
 * \idx_type
 * \order
 * \op
 * \op_data
 * \return \success{The return value of the first operator that returns
 *                  non-zero, or zero if all members were processed with no
 *                  operator returning non-zero.}
 * \return \failure{Negative if an error occurs in the library, or the negative
 *                  value returned by one of the operators.}
 *
 * \details H5Lvisit2() is a recursive iteration function to visit all links in
 *          and below a group in an HDF5 file, thus providing a mechanism for
 *          an application to perform a common set of operations across all of
 *          those links or a dynamically selected subset. For non-recursive
 *          iteration across the members of a group, see H5Literate2().
 *
 *          The group serving as the root of the iteration is specified by its
 *          group or file identifier, \p group_id.
 *
 *          Two parameters are used to establish the iteration: \p idx_type and
 *          \p order.
 *
 *          \p idx_type specifies the index to be used. If the links have not
 *          been indexed by the index type, they will first be sorted by that
 *          index then the iteration will begin; if the links have been so
 *          indexed, the sorting step will be unnecessary, so the iteration may
 *          begin more quickly. Valid values include the following:
 *          \indexes
 *
 *          Note that the index type passed in \p idx_type is a best effort
 *          setting. If the application passes in a value indicating iteration
 *          in creation order and a group is encountered that was not tracked
 *          in creation order, that group will be iterated over in
 *          lexicographic order by name, or name order. (Name order is the
 *          native order used by the HDF5 library and is always available.)
 *
 *          \p order specifies the order in which objects are to be inspected
 *          along the index specified in \p idx_type. Valid values include the
 *          following:
 *          \orders
 *
 *          \p op is a callback function of type \ref H5L_iterate2_t that is invoked
 *          for each link encountered.
 *          \snippet this H5L_iterate2_t_snip
 *
 *          The \ref H5L_info2_t struct is defined (in H5Lpublic.h) as follows:
 *          \snippet this H5L_info2_t_snip
 *
 *          The possible return values from the callback function, and the
 *          effect of each, are as follows:
 *          \li Zero causes the visit iterator to continue, returning zero when
 *              all group members have been processed.
 *          \li  A positive value causes the visit iterator to immediately
 *               return that positive value, indicating short-circuit success.
 *          \li A negative value causes the visit iterator to immediately
 *              return that value, indicating failure.
 *
 *          The H5Lvisit2() \p op_data parameter is a user-defined pointer to
 *          the data required to process links in the course of the iteration.
 *          This pointer is passed back to each step of the iteration in the
 *          \p op callback function's \p op_data parameter.
 *
 *          H5Lvisit2() and H5Ovisit2() are companion functions: one for
 *          examining and operating on links; the other for examining and
 *          operating on the objects that those links point to. Both functions
 *          ensure that by the time the function completes successfully, every
 *          link or object below the specified point in the file has been
 *          presented to the application for whatever processing the
 *          application requires.
 *
 * \callback_note
 *
 * \since 1.12.0
 *
 * \see H5Literate()
 *
 */
H5_DLL herr_t H5Lvisit2(hid_t grp_id, H5_index_t idx_type, H5_iter_order_t order, H5L_iterate2_t op,
                        void *op_data);
/**
 * \ingroup TRAV
 *
 * \brief Recursively visits all links starting from a specified group
 *
 * \loc_id
 * \param[in] group_name Group name
 * \idx_type
 * \order
 * \op
 * \op_data
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lvisit_by_name2() is a recursive iteration function to visit all
 *          links in and below a group in an HDF5 file, thus providing a
 *          mechanism for an application to perform a common set of operations
 *          across all of those links or a dynamically selected subset. For
 *          non-recursive iteration across the members of a group, see
 *          H5Literate2().
 *
 *          The group serving as the root of the iteration is specified by the
 *          \p loc_id / \p group_name parameter pair. \p loc_id specifies a
 *          file or group; group_name specifies either a group in the file
 *          (with an absolute name based in the file's root group) or a group
 *          relative to \p loc_id. If \p loc_id fully specifies the group that
 *          is to serve as the root of the iteration, group_name should be '.'
 *          (a dot). (Note that when \p loc_id fully specifies the group
 *          that is to serve as the root of the iteration, the user may wish to
 *          consider using H5Lvisit2() instead of H5Lvisit_by_name2().)
 *
 *          Two parameters are used to establish the iteration: \p idx_type and
 *          \p order.
 *
 *          \p idx_type specifies the index to be used. If the links have not
 *          been indexed by the index type, they will first be sorted by that
 *          index then the iteration will begin; if the links have been so
 *          indexed, the sorting step will be unnecessary, so the iteration may
 *          begin more quickly. Valid values include the following:
 *          \indexes
 *
 *          Note that the index type passed in \p idx_type is a best effort
 *          setting. If the application passes in a value indicating iteration
 *          in creation order and a group is encountered that was not tracked
 *          in creation order, that group will be iterated over in
 *          lexicographic order by name, or name order. (Name order is the
 *          native order used by the HDF5 library and is always available.)
 *
 *          \p order specifies the order in which objects are to be inspected
 *          along the index specified in \p idx_type. Valid values include the
 *          following:
 *          \orders
 *
 *          The \p op callback function, the related \ref H5L_info2_t
 *          \c struct, and the effect that the callback function's return value
 *          has on the application are described in H5Lvisit2().
 *
 *          The H5Lvisit_by_name2() \p op_data parameter is a user-defined
 *          pointer to the data required to process links in the course of the
 *          iteration. This pointer is passed back to each step of the
 *          iteration in the callback function's \p op_data parameter.
 *
 *          \p lapl_id is a link access property list. In the general case,
 *          when default link access properties are acceptable, this can be
 *          passed in as #H5P_DEFAULT. An example of a situation that requires
 *          a non-default link access property list is when the link is an
 *          external link; an external link may require that a link prefix be
 *          set in a link access property list (see H5Pset_elink_prefix()).
 *
 *          H5Lvisit_by_name2() and H5Ovisit_by_name2() are companion
 *          functions: one for examining and operating on links; the other for
 *          examining and operating on the objects that those links point to.
 *          Both functions ensure that by the time the function completes
 *          successfully, every link or object below the specified point in the
 *          file has been presented to the application for whatever processing
 *          the application requires.
 *
 * \callback_note
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Lvisit_by_name2(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                H5_iter_order_t order, H5L_iterate2_t op, void *op_data, hid_t lapl_id);
/* UD link functions */
/**
 * \ingroup H5L
 *
 * \brief Creates a link of a user-defined type
 *
 * \loc_id{link_loc_id}
 * \param[in] link_name Link name
 * \param[in] link_type User-defined link class
 * \param[in] udata User-supplied link information
 * \param[in] udata_size Size of udata buffer
 * \lcpl_id
 * \lapl_id
 *
 * \return \herr_t
 *
 * \details H5Lcreate_ud() creates a link of user-defined type \p link_type
 *          named \p link_name at the location specified in \p link_loc_id with
 *          user-specified data \p udata.
 *
 *          \p link_name is interpreted relative to \p link_loc_id. If
 *          \p link_name begins with \c /, then it will be interpreted as
 *          an absolute path in the file. The name of the created link
 *          will be the last element of the provided path. Prior elements
 *          in the path are used to locate the parent group of the new link.
 *
 *          Valid values for the link class of the new link, \p link_type,
 *          include #H5L_TYPE_EXTERNAL and any user-defined link classes that
 *          have been registered with the library. See H5Lregister() for
 *          further information.
 *
 *          The format of the information pointed to by \p udata is defined by
 *          the user. \p udata_size specifies the size of the \p udata buffer.
 *          \p udata may be NULL if \p udata_size is zero (0).
 *
 *          The property lists specified by \p lcpl_id and \p lapl_id specify
 *          properties used to create and access the link.
 *
 * \note The external link type, #H5L_TYPE_EXTERNAL, included in the HDF5
 *       library distribution, is implemented as a user-defined link type. This
 *       was done, in part, to provide a model for the implementation of other
 *       user-defined links.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lcreate_ud(hid_t link_loc_id, const char *link_name, H5L_type_t link_type, const void *udata,
                           size_t udata_size, hid_t lcpl_id, hid_t lapl_id);
/**
 * \ingroup H5LA
 *
 * \brief Determines whether a class of user-defined links is registered
 *
 * \param[in] id User-defined link class identifier
 *
 * \return \htri_t
 *
 * \details H5Lis_registered() tests whether a user-defined link class is
 *          currently registered, either by the HDF5 library or by the user
 *          through the use of H5Lregister().
 *
 * \note A link class must be registered to create new links of that type or to
 *       traverse existing links of that type.
 *
 * \since 1.8.0
 *
 */
H5_DLL htri_t H5Lis_registered(H5L_type_t id);

/* External link functions */
/**
 * \ingroup H5L
 *
 * \brief Decodes external link information
 *
 * \param[in] ext_linkval Buffer containing external link information
 * \param[in] link_size Size, in bytes, of the \p ext_linkval buffer
 * \param[out] flags External link flags, packed as a bitmap (\Emph{Reserved as
 *                   a bitmap for flags; no flags are currently defined, so the
 *                   only valid value * is 0.})
 * \param[out] filename Returned filename \param[out] obj_path Returned
 * object path, relative to \p filename
 *
 * \return \herr_t
 *
 * \details H5Lunpack_elink_val() decodes the external link information
 *          returned by H5Lget_val() in the \p ext_linkval buffer.
 *
 *          \p ext_linkval should be the buffer set by H5Lget_val() and will
 *          consist of two NULL-terminated strings, the filename and object
 *          path, one after the other.
 *
 *          Given this buffer, H5Lunpack_elink_val() creates pointers to the
 *          filename and object path within the buffer and returns them in
 *          \p filename and \p obj_path, unless they are passed in as NULL.
 *
 *          H5Lunpack_elink_val() requires that \p ext_linkval contain a
 *          concatenated pair of null-terminated strings, so use of this
 *          function on a string that is not an external link \p udata buffer
 *          may result in a segmentation fault. This failure can be avoided by
 *          adhering to the following procedure:
 *          <ol>
 *            <li>Call H5Lget_info() to get the link type and the size of the
 *                link value.<li>
 *            <li>Verify that the link is an external link, i.e., that its link
 *                type is #H5L_TYPE_EXTERNAL.</li>
 *            <li>Call H5Lget_val() to get the link value.</li>
 *            <li>Call H5Lunpack_elink_val() to unpack that value.</li>
 *          </ol>
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lunpack_elink_val(const void *ext_linkval /*in*/, size_t link_size, unsigned *flags,
                                  const char **filename /*out*/, const char **obj_path /*out*/);
/**
 * \ingroup H5L
 *
 * \brief Creates an external link, a soft link to an object in a different file.
 *
 * \param[in] file_name   Name of the target file containing the target object.
 * \param[in] obj_name    Path within the target file to the target object
 * \fgdt_loc_id{link_loc_id}
 * \param[in] link_name   Name of the new link, relative to \p link_loc_id
 * \lcpl_id
 * \lapl_id
 * \return \herr_t
 *
 * \details H5Lcreate_external() creates a new external link. An external link
 *          is a soft link to an object in a different HDF5 file from the
 *          location of the link, i.e., to an external object.
 *
 *          \p file_name identifies the target file containing the target
 *          object; \p obj_name specifies the path of the target object within
 *          that file. \p obj_name must be an absolute pathname in
 *          \p file_name, i.e., it must start at the target file's root group,
 *          but it is not interpreted until an application attempts to traverse
 *          it.
 *
 *          \p link_loc_id and \p link_name specify the location and name,
 *          respectively, of the new link. \p link_name is interpreted relative
 *          to \p link_loc_id. If \p link_name begins with \c /, then it is
 *          interpreted as an absolute path in the file. The name of the created
 *          link will be the last element of the provided path. Prior elements in
 *          the path are used to locate the parent group of the new link.
 *
 *          \p lcpl_id is the link creation property list used in creating the
 *          new link.
 *
 *          \p lapl_id is the link access property list used in traversing the
 *          new link. Note that an external file opened by the traversal of an
 *          external link is always opened with the weak file close degree
 *          property setting, #H5F_CLOSE_WEAK (see H5Pset_fclose_degree());
 *          any file close degree property setting in \p lapl_id is ignored.
 *
 *          An external link behaves similarly to a soft link, and like a soft
 *          link in an HDF5 file, it may dangle: the target file and object
 *          need not exist at the time that the external link is created.
 *
 *          When the external link \p link_name is accessed, the library will
 *          search for the target file \p file_name as described below:
 *
 *          - If \p file_name is a relative pathname, the following steps are
 *            performed:
 *            - The library will get the prefix(es) set in the environment
 *              variable \c HDF5_EXT_PREFIX and will try to prepend each prefix
 *              to \p file_name to form a new \p file_name.
 *            - If the new \p file_name does not exist or if \c HDF5_EXT_PREFIX
 *              is not set, the library will get the prefix set via
 *              H5Pset_elink_prefix() and prepend it to \p file_name to form a
 *              new \p file_name.
 *            - If the new \p file_name does not exist or no prefix is being
 *              set by H5Pset_elink_prefix(), then the path of the file
 *              associated with \p link_loc_id is obtained. This path can be
 *              the absolute path or the current working directory plus the
 *              relative path of that file when it is created/opened. The
 *              library will prepend this path to \p file_name to form a new
 *              \p file_name.
 *            - If the new \p file_name does not exist, then the library will
 *              look for \p file_name and will return failure/success
 *              accordingly.
 *          - If \p file_name is an absolute pathname, the library will first
 *            try to find \p file_name. If \p file_name does not exist,
 *            \p file_name is stripped of directory paths to form a new
 *            \p file_name. The search for the new \p file_name then follows
 *            the same steps as described above for a relative pathname. See
 *            examples below illustrating how target_file_name is stripped to
 *            form a new \p file_name.
 *
 *          Note that \p file_name is considered to be an absolute pathname
 *          when the following condition is true:
 *
 *          - For Unix, the first character of \p file_name is a slash (\c /).
 *            For example, consider a \p file_name of \c /tmp/A.h5.
 *            If that target file does not exist, the new \p file_name after
 *            stripping will be \c A.h5.
 *          - For Windows, there are 6 cases:
 *            -# \p file_name is an absolute drive with an absolute pathname.
 *               For example, consider a \p file_name of \c /tmp/A.h5. If that
 *               target file does not exist, the new \p file_name after
 *               stripping will be \c A.h5.
 *            -# \p file_name is an absolute pathname without specifying drive
 *               name. For example, consider a \p file_name of \c /tmp/A.h5.
 *               If that target file does not exist, the new \p file_name after
 *               stripping will be \c A.h5.
 *            -# \p file_name is an absolute drive with a relative pathname.
 *               For example, consider a \p file_name of \c /tmp/A.h5. If that
 *               target file does not exist, the new \p file_name after
 *               stripping will be \c tmp\A.h5.
 *            -# \p file_name is in UNC (Uniform Naming Convention) format with
 *               a server name, share name, and pathname. For example, consider
 *               a \p file_name of \c /tmp/A.h5. If that target file does not
 *               exist, the new \p file_name after stripping will be \c A.h5.
 *            -# \p file_name is in Long UNC (Uniform Naming Convention) format
 *               with a server name, share name, and pathname. For example,
 *               consider a \p file_name of \c /tmp/A.h5. If that target file
 *               does not exist, the new \p file_name after stripping will be
 *               \c A.h5.
 *            -# \p file_name is in Long UNC (Uniform Naming Convention) format
 *               with an absolute drive and an absolute pathname. For example,
 *               consider a \p file_name of \c /tmp/A.h5. If that target file
 *               does not exist, the new \p file_name after stripping will be
 *               \c A.h5.
 *
 *          The library opens the target file \p file_name with the file access
 *          property list that is set via H5Pset_elink_fapl() when the external
 *          link link_name is accessed. If no such property list is set, the
 *          library uses the file access property list associated with the file
 *          of \p link_loc_id to open the target file.
 *
 *          If an application requires additional control over file access
 *          flags or the file access property list, see H5Pset_elink_cb(); this
 *          function enables the use of an external link callback function as
 *          described in H5L_elink_traverse_t().
 *
 * \attention A file close degree property setting (H5Pset_fclose_degree()) in
 *            the external link file access property list or in the external
 *            link callback function will be ignored. A file opened by means of
 *            traversing an external link is always opened with the weak file
 *            close degree property setting, #H5F_CLOSE_WEAK .
 *
 * \see H5Lcreate_hard(), H5Lcreate_soft(), H5Lcreate_ud()
 *
 * \since 1.8.0
 */
H5_DLL herr_t H5Lcreate_external(const char *file_name, const char *obj_name, hid_t link_loc_id,
                                 const char *link_name, hid_t lcpl_id, hid_t lapl_id);

/// \cond DEV
/* API Wrappers for async routines */
/* (Must be defined _after_ the function prototype) */
/* (And must only defined when included in application code, not the library) */
#ifndef H5L_MODULE
#define H5Lcreate_hard_async(...)   H5Lcreate_hard_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Lcreate_soft_async(...)   H5Lcreate_soft_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Ldelete_async(...)        H5Ldelete_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Ldelete_by_idx_async(...) H5Ldelete_by_idx_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Lexists_async(...)        H5Lexists_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Literate_async(...)       H5Literate_async(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Define "wrapper" versions of function calls, to allow compile-time values to
 *      be passed in by language wrapper or library layer on top of HDF5.
 */
#define H5Lcreate_hard_async_wrap   H5_NO_EXPAND(H5Lcreate_hard_async)
#define H5Lcreate_soft_async_wrap   H5_NO_EXPAND(H5Lcreate_soft_async)
#define H5Ldelete_async_wrap        H5_NO_EXPAND(H5Ldelete_async)
#define H5Ldelete_by_idx_async_wrap H5_NO_EXPAND(H5Ldelete_by_idx_async)
#define H5Lexists_async_wrap        H5_NO_EXPAND(H5Lexists_async)
#define H5Literate_async_wrap       H5_NO_EXPAND(H5Literate_async)
#endif /* H5L_MODULE */
/// \endcond

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS

/* Macros */

/* Typedefs */

//! <!-- [H5L_info1_t_snip] -->
/**
 * Information struct for link (for H5Lget_info1() and H5Lget_info_by_idx1())
 */
typedef struct {
    H5L_type_t type;         /**< Type of link                   */
    hbool_t    corder_valid; /**< Indicate if creation order is valid */
    int64_t    corder;       /**< Creation order                 */
    H5T_cset_t cset;         /**< Character set of link name     */
    union {
        haddr_t address;  /**< Address hard link points to    */
        size_t  val_size; /**< Size of a soft link or UD link value */
    } u;
} H5L_info1_t;
//! <!-- [H5L_info1_t_snip] -->

/** Prototype for H5Literate1() / H5Literate_by_name1() operator */
//! <!-- [H5L_iterate1_t_snip] -->
typedef herr_t (*H5L_iterate1_t)(hid_t group, const char *name, const H5L_info1_t *info, void *op_data);
//! <!-- [H5L_iterate1_t_snip] -->

/* Function prototypes */
/**
 * \ingroup H5L
 *
 * \brief Returns information about a link
 *
 * \fgdta_loc_id
 * \param[in] name Link name
 * \param[out] linfo Buffer in which link information is returned
 * \lapl_id
 *
 * \return \herr_t
 *
 * \deprecated As of HDF5-1.12 this function has been deprecated in favor of
 *             the function H5Lget_info2() or the macro H5Lget_info().
 *
 * \details H5Lget_info1() returns information about the specified link through
 *          the \p linfo argument.
 *
 *          The location identifier, \p loc_id, specifies the location of the
 *          link. A link name, \p name, interpreted relative to \p loc_id,
 *          specifies the link being queried.
 *
 *          \p lapl_id is the link access property list associated with the
 *          link \p name. In the general case, when default link access
 *          properties are acceptable, this can be passed in as #H5P_DEFAULT.
 *          An example of a situation that requires a non-default link access
 *          property list is when the link is an external link; an external
 *          link may require that a link prefix be set in a link access
 *          property list (see H5Pset_elink_prefix()).
 *
 *          H5Lget_info1() returns information about name in the data structure
 *          \ref H5L_info1_t, which is described below and defined in
 *          H5Lpublic.h. This structure is returned in the buffer \p linfo.
 *          \snippet this H5L_info1_t_snip
 *          In the above struct, type specifies the link class. Valid values
 *          include the following:
 *          \link_types
 *          There will be additional valid values if user-defined links have
 *          been registered.
 *
 *          \c corder specifies the link's creation order position while
 *          \c corder_valid indicates whether the value in \c corder is valid.
 *
 *          If \c corder_valid is \c true, the value in \c corder is known to
 *          be valid; if \c corder_valid is \c false, the value in \c corder is
 *          presumed to be invalid;
 *
 *          \c corder starts at zero (0) and is incremented by one (1) as new
 *          links are created. But higher-numbered entries are not adjusted
 *          when a lower-numbered link is deleted; the deleted link's creation
 *          order position is simply left vacant. In such situations, the value
 *          of \c corder for the last link created will be larger than the
 *          number of links remaining in the group.
 *
 *          \c cset specifies the character set in which the link name is
 *          encoded. Valid values include the following:
 *          \csets
 *          This value is set with #H5Pset_char_encoding.
 *
 *          \c address and \c val_size are returned for hard and symbolic
 *          links, respectively. Symbolic links include soft and external links
 *          and some user-defined links.
 *
 *          If the link is a hard link, \c address specifies the file address
 *          that the link points to.
 *
 *          If the link is a symbolic link, \c val_size will be the length of
 *          the link value, e.g., the length of the HDF5 path name with a null
 *          terminator.
 *
 * \version 1.12.0 Function was deprecated.
 * \version 1.8.2 Fortran subroutine added in this release.
 * \version 1.8.4 Fortran subroutine syntax changed in this release.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lget_info1(hid_t loc_id, const char *name, H5L_info1_t *linfo /*out*/, hid_t lapl_id);
/**
 * \ingroup H5L
 *
 * \brief Retrieves metadata for a link in a group, according to the order
 *        within a field or index
 *
 * \loc_id
 * \param[in] group_name Group name
 * \idx_type
 * \order
 * \param[in] n Link position for which to retrieve information
 * \param[out] linfo Buffer in which link information is returned
 * \lapl_id
 *
 * \return \herr_t
 *
 * \deprecated As of HDF5-1.12 this function has been deprecated in favor of
 *             the function H5Lget_info_by_idx2() and the macro
 *             H5Lget_info_by_idx().
 *
 * \details H5Lget_info_by_idx1() returns the metadata for a link in a group
 *          according to a specified field or index and a specified order.
 *
 *          The link for which information is to be returned is specified by \p
 *          idx_type, \p order, and \p n as follows:
 *
 *          - \p idx_type specifies the field by which the links in \p
 *            group_name are ordered. The links may be indexed on this field,
 *            in which case operations seeking specific links are likely to
 *            complete more quickly.
 *          - \p order specifies the order in which
 *            the links are to be referenced for the purposes of this function.
 *          - \p n specifies the position of the subject link. Note that this
 *            count is zero-based; 0 (zero) indicates that the function will
 *            return the value of the first link; if \p n is 5, the function
 *            will return the value of the sixth link; etc.
 *
 *          For example, assume that \p idx_type, \p order, and \p n are
 *          #H5_INDEX_NAME, #H5_ITER_DEC, and 5, respectively. #H5_INDEX_NAME
 *          indicates that the links are accessed in lexicographic order by
 *          their names. #H5_ITER_DEC specifies that the list be traversed in
 *          reverse order, or in decremented order. And 5 specifies that this
 *          call to the function will return the metadata for the 6th link
 *          (\c n + 1) from the end.
 *
 *          See H5Literate1() for a list of valid values and further discussion
 *          regarding \p idx_type and \p order.
 *
 *          If \p loc_id specifies the group in which the link resides,
 *          \p group_name can be a dot (\c .).
 *
 * \version 1.12.0 Function was renamed to H5Lget_index_by_idx1() and deprecated.
 * \version 1.8.4 Fortran subroutine syntax changed in this release.
 * \version 1.8.2 Fortran subroutine added in this release.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lget_info_by_idx1(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                  H5_iter_order_t order, hsize_t n, H5L_info1_t *linfo /*out*/,
                                  hid_t lapl_id);
/**
 * \ingroup TRAV
 *
 * \brief Iterates over links in a group, with user callback routine,
 *        according to the order within an index.
 *
 * \group_id{grp_id}
 * \idx_type
 * \order
 * \param[in,out] idx Pointer to an iteration index to allow
 *                    continuing a previous iteration
 * \op
 * \op_data
 * \return \success{The return value of the first operator that returns
 *                  non-zero, or zero if all members were processed with no
 *                  operator returning non-zero.}
 * \return \failure{Negative if an error occurs in the library, or the negative
 *                  value returned by one of the operators.}
 *
 * \deprecated Deprecated in favor of H5Literate2().
 *
 * \details H5Literate1() iterates through the links in a file or
 *          group, \p group_id, in the order of the specified
 *          index, \p idx_type, using a user-defined callback routine
 *          \p op. H5Literate1() does not recursively follow links into
 *          subgroups of the specified group.
 *
 *          Three parameters are used to manage progress of the iteration:
 *          \p idx_type, \p order, and \p idx_p.
 *
 *          \p idx_type specifies the index to be used. If the links have
 *          not been indexed by the index type, they will first be sorted by
 *          that index then the iteration will begin; if the links have been
 *          so indexed, the sorting step will be unnecessary, so the iteration
 *          may begin more quickly. Valid values include the following:
 *          \indexes
 *
 *          \p order specifies the order in which objects are to be inspected
 *          along the index \p idx_type. Valid values include the following:
 *          \orders
 *
 *          \p idx_p tracks the iteration and allows an iteration to be
 *          resumed if it was stopped before all members were processed. It is
 *          passed in by the application with a starting point and returned by
 *          the library with the point at which the iteration stopped.
 *
 *          \p op_data is a user-defined pointer to the data required to
 *          process links in the course of the iteration. This pointer is
 *          passed back to each step of the iteration in the \p op callback
 *          function's \p op_data parameter. \p op is invoked for each link
 *          encounter.
 *
 *          \p op_data is passed to and from each iteration and can be used to
 *          supply or aggregate information across iterations.
 *
 * \remark Same pattern of behavior as H5Giterate().
 *
 * \note This function is also available through the H5Literate() macro.
 *
 * \warning The behavior of H5Literate1() is undefined if the link
 *          membership of \p group_id changes during the iteration.
 *          This does not limit the ability to change link destinations
 *          while iterating, but caution is advised.
 *
 * \callback_note
 *
 * \version 1.12.0 Function was deprecated in this release.
 * \since 1.8.0
 *
 * \see H5Literate_by_name2(), H5Lvisit2(), H5Lvisit_by_name2()
 *
 */
H5_DLL herr_t H5Literate1(hid_t grp_id, H5_index_t idx_type, H5_iter_order_t order, hsize_t *idx,
                          H5L_iterate1_t op, void *op_data);
/**
 * \ingroup TRAV
 *
 * \brief Iterates through links in a group by its name
 *
 * \loc_id
 * \param[in] group_name Group name
 * \idx_type
 * \order
 * \param[in,out] idx iteration position at which to start (\Emph{IN}) or
 *                position at which an interrupted iteration may be restarted
 *                (\Emph{OUT})
 * \op
 * \op_data
 * \lapl_id
 *
 * \return \success{The return value of the first operator that returns
 *                  non-zero, or zero if all members were processed with no
 *                  operator returning non-zero.}
 * \return \failure{Negative if an error occurs in the library, or the negative
 *                  value returned by one of the operators.}
 *
 * \deprecated As of HDF5-1.12 this function has been deprecated in favor of
 *             the function H5Literate_by_name2() or the macro
 *             H5Literate_by_name().
 *
 * \details H5Literate_by_name1() iterates through the links in a group
 *          specified by \p loc_id and \p group_name, in the order of the
 *          specified index, \p idx_type, using a user-defined callback routine
 *          \p op. H5Literate_by_name1() does not recursively follow links into
 *          subgroups of the specified group.
 *
 *          \p idx_type specifies the index to be used. If the links have not
 *          been indexed by the index type, they will first be sorted by that
 *          index then the iteration will begin; if the links have been so
 *          indexed, the sorting step will be unnecessary, so the iteration may
 *          begin more quickly. Valid values include the following:
 *          \indexes
 *
 *          \p order specifies the order in which objects are to be inspected
 *          along the index specified in \p idx_type. Valid values include the
 *          following:
 *          \orders
 *
 *          \p idx allows an interrupted iteration to be resumed; it is
 *          passed in by the application with a starting point and returned by
 *          the library with the point at which the iteration stopped.
 *
 * \warning H5Literate_by_name1() assumes that the membership of the group being
 *          iterated over remains unchanged through the iteration; if any of the
 *          links in the group change during the iteration, the function's
 *          behavior is undefined. Note, however, that objects pointed to by the
 *          links can be modified.
 *
 * \note H5Literate_by_name1() is not recursive. In particular, if a member of
 *       \p group_name is found to be a group, call it \c subgroup_a,
 *       H5Literate_by_name1() does not examine the members of \c subgroup_a.
 *       When recursive iteration is required, the application must handle the
 *       recursion, explicitly calling H5Literate_by_name1() on discovered
 *       subgroups.
 *
 * \callback_note
 *
 * \note H5Literate_by_name1() is the same as H5Giterate(), except that
 *       H5Giterate() always proceeds in lexicographic order.
 *
 * \version 1.12.0 Function H5Literate_by_name() was renamed to
 *                 H5Literate_by_name1() and deprecated.
 * \version 1.8.8 Fortran subroutine added.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Literate_by_name1(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                  H5_iter_order_t order, hsize_t *idx, H5L_iterate1_t op, void *op_data,
                                  hid_t lapl_id);
/**
 * \ingroup TRAV
 *
 * \brief Recursively visits all links starting from a specified group
 *
 * \group_id{grp_id}
 * \idx_type
 * \order
 * \op
 * \op_data
 *
 * \return \success{The return value of the first operator that returns
 *                  non-zero, or zero if all members were processed with no
 *                  operator returning non-zero.}
 * \return \failure{Negative if an error occurs in the library, or the negative
 *                  value returned by one of the operators.}
 *
 * \deprecated As of HDF5-1.12 this function has been deprecated in favor of
 *             the function H5Lvisit2() or the macro H5Lvisit().
 *
 * \details H5Lvisit1() is a recursive iteration function to visit all links in
 *          and below a group in an HDF5 file, thus providing a mechanism for
 *          an application to perform a common set of operations across all of
 *          those links or a dynamically selected subset. For non-recursive
 *          iteration across the members of a group, see H5Literate1().
 *
 *          The group serving as the root of the iteration is specified by its
 *          group or file identifier, \p group_id.
 *
 *          Two parameters are used to establish the iteration: \p idx_type and
 *          \p order.
 *
 *          \p idx_type specifies the index to be used. If the links have not
 *          been indexed by the index type, they will first be sorted by that
 *          index then the iteration will begin; if the links have been so
 *          indexed, the sorting step will be unnecessary, so the iteration may
 *          begin more quickly. Valid values include the following:
 *          \indexes
 *
 *          Note that the index type passed in \p idx_type is a best effort
 *          setting. If the application passes in a value indicating iteration
 *          in creation order and a group is encountered that was not tracked
 *          in creation order, that group will be iterated over in
 *          lexicographic order by name, or name order. (Name order is the
 *          native order used by the HDF5 library and is always available.)
 *
 *          \p order specifies the order in which objects are to be inspected
 *          along the index specified in \p idx_type. Valid values include the
 *          following:
 *          \orders
 *
 *          \p op is a callback function of type \ref H5L_iterate1_t that is invoked
 *          for each link encountered.
 *          \snippet this H5L_iterate1_t_snip
 *
 *          The \ref H5L_info1_t struct is defined (in H5Lpublic.h) as follows:
 *          \snippet this H5L_info1_t_snip
 *
 *          The possible return values from the callback function, and the
 *          effect of each, are as follows:
 *          \li Zero causes the visit iterator to continue, returning zero when
 *              all group members have been processed.
 *          \li  A positive value causes the visit iterator to immediately
 *               return that positive value, indicating short-circuit success.
 *          \li A negative value causes the visit iterator to immediately
 *              return that value, indicating failure.
 *
 *          The H5Lvisit1() \p op_data parameter is a user-defined pointer to
 *          the data required to process links in the course of the iteration.
 *          This pointer is passed back to each step of the iteration in the
 *          \p op callback function's \p op_data parameter.
 *
 *          H5Lvisit1() and H5Ovisit1() are companion functions: one for
 *          examining and operating on links; the other for examining and
 *          operating on the objects that those links point to. Both functions
 *          ensure that by the time the function completes successfully, every
 *          link or object below the specified point in the file has been
 *          presented to the application for whatever processing the
 *          application requires.
 *
 * \callback_note
 *
 * \version 1.12.0 Function was renamed from H5Lvisit() to H5Lvisit1() and
 *                 deprecated.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lvisit1(hid_t grp_id, H5_index_t idx_type, H5_iter_order_t order, H5L_iterate1_t op,
                        void *op_data);
/**
 * \ingroup TRAV
 *
 * \brief Recursively visits all links starting from a specified group
 *
 * \loc_id
 * \param[in] group_name Group name
 * \idx_type
 * \order
 * \op
 * \op_data
 * \lapl_id
 *
 * \return \success{The return value of the first operator that returns
 *                  non-zero, or zero if all members were processed with no
 *                  operator returning non-zero.}
 * \return \failure{Negative if an error occurs in the library, or the negative
 *                  value returned by one of the operators.}
 *
 * \deprecated As of HDF5-1.12 this function has been deprecated in favor of
 *             the function H5Lvisit_by_name2() or the macro H5Lvisit_by_name().
 *
 * \details H5Lvisit_by_name1() is a recursive iteration function to visit all
 *          links in and below a group in an HDF5 file, thus providing a
 *          mechanism for an application to perform a common set of operations
 *          across all of those links or a dynamically selected subset. For
 *          non-recursive iteration across the members of a group, see
 *          H5Literate1().
 *
 *          The group serving as the root of the iteration is specified by the
 *          \p loc_id / \p group_name parameter pair. \p loc_id specifies a
 *          file or group; group_name specifies either a group in the file
 *          (with an absolute name based in the file's root group) or a group
 *          relative to \p loc_id. If \p loc_id fully specifies the group that
 *          is to serve as the root of the iteration, group_name should be '.'
 *          (a dot). (Note that when \p loc_id fully specifies the group
 *          that is to serve as the root of the iteration, the user may wish to
 *          consider using H5Lvisit1() instead of H5Lvisit_by_name1().)
 *
 *          Two parameters are used to establish the iteration: \p idx_type and
 *          \p order.
 *
 *          \p idx_type specifies the index to be used. If the links have not
 *          been indexed by the index type, they will first be sorted by that
 *          index then the iteration will begin; if the links have been so
 *          indexed, the sorting step will be unnecessary, so the iteration may
 *          begin more quickly. Valid values include the following:
 *          \indexes
 *
 *          Note that the index type passed in \p idx_type is a best effort
 *          setting. If the application passes in a value indicating iteration
 *          in creation order and a group is encountered that was not tracked
 *          in creation order, that group will be iterated over in
 *          lexicographic order by name, or name order. (Name order is the
 *          native order used by the HDF5 library and is always available.)
 *
 *          \p order specifies the order in which objects are to be inspected
 *          along the index specified in \p idx_type. Valid values include the
 *          following:
 *          \orders
 *
 *          The \p op callback function, the related \ref H5L_info1_t
 *          \c struct, and the effect that the callback function's return value
 *          has on the application are described in H5Lvisit1().
 *
 *          The H5Lvisit_by_name1() \p op_data parameter is a user-defined
 *          pointer to the data required to process links in the course of the
 *          iteration. This pointer is passed back to each step of the
 *          iteration in the callback function's \p op_data parameter.
 *
 *          \p lapl_id is a link access property list. In the general case,
 *          when default link access properties are acceptable, this can be
 *          passed in as #H5P_DEFAULT. An example of a situation that requires
 *          a non-default link access property list is when the link is an
 *          external link; an external link may require that a link prefix be
 *          set in a link access property list (see H5Pset_elink_prefix()).
 *
 *          H5Lvisit_by_name1() and H5Ovisit_by_name1() are companion
 *          functions: one for examining and operating on links; the other for
 *          examining and operating on the objects that those links point to.
 *          Both functions ensure that by the time the function completes
 *          successfully, every link or object below the specified point in the
 *          file has been presented to the application for whatever processing
 *          the application requires.
 *
 * \callback_note
 *
 * \version 1.12.0 Function renamed from H5Lvisit_by_name() to
 *                 H5Lvisit_by_name1() and deprecated.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Lvisit_by_name1(hid_t loc_id, const char *group_name, H5_index_t idx_type,
                                H5_iter_order_t order, H5L_iterate1_t op, void *op_data, hid_t lapl_id);

#endif /* H5_NO_DEPRECATED_SYMBOLS */

#ifdef __cplusplus
}
#endif
#endif /* H5Lpublic_H */
