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
 * This file contains public declarations for the H5R module.
 */
#ifndef H5Rpublic_H
#define H5Rpublic_H

#include "H5public.h"  /* Generic Functions                        */
#include "H5Gpublic.h" /* Groups                                   */
#include "H5Ipublic.h" /* Identifiers                              */
#include "H5Opublic.h" /* Object Headers                           */

/*****************/
/* Public Macros */
/*****************/

/* Deprecated reference buffer sizes that are kept for backward compatibility */
#define H5R_OBJ_REF_BUF_SIZE      sizeof(haddr_t)
#define H5R_DSET_REG_REF_BUF_SIZE (sizeof(haddr_t) + 4)

/**
 * Default reference buffer size.
 *
 * \internal Note! Be careful with the sizes of the references because they
 *           should really depend on the run-time values in the file.
 */
#define H5R_REF_BUF_SIZE (64)

/*******************/
/* Public Typedefs */
/*******************/

//! <!-- [H5R_type_t_snip] -->
/**
 * Reference types allowed.
 *
 * \internal DO NOT CHANGE THE ORDER or VALUES as reference type values are
 *           encoded into the datatype message header.
 */
typedef enum {
    H5R_BADTYPE         = (-1), /**< Invalid reference type \since 1.0.0           */
    H5R_OBJECT1         = 0,    /**< Backward compatibility (object) \since 1.12.0 */
    H5R_DATASET_REGION1 = 1,    /**< Backward compatibility (region) \since 1.12.0 */
    H5R_OBJECT2         = 2,    /**< Object reference \since 1.12.0                */
    H5R_DATASET_REGION2 = 3,    /**< Region reference \since 1.12.0                */
    H5R_ATTR            = 4,    /**< Attribute Reference \since 1.12.0             */
    H5R_MAXTYPE         = 5     /**< Highest type (invalid) \since 1.0.0           */
} H5R_type_t;
//! <!-- [H5R_type_t_snip] -->

/* Deprecated types are kept for backward compatibility with previous versions */

//! <!-- [hobj_ref_t_snip] -->
/**
 * \deprecated Deprecated object reference type that is used with deprecated
 *             reference APIs.
 *
 * \note This type can only be used with the "native" HDF5 VOL connector.
 */
typedef haddr_t hobj_ref_t;
//! <!-- [hobj_ref_t_snip] -->

//! <!-- [hdset_reg_ref_t_snip] -->
/**
 * Buffer to store heap ID and index
 *
 * This needs to be large enough to store largest #haddr_t in a worst case
 * machine (8 bytes currently) plus an int.
 *
 * \deprecated Dataset region reference type that is used with deprecated
 *             reference APIs.
 *
 * \note This type can only be used with the "native" HDF5 VOL connector.
 */
typedef struct {
    uint8_t __data[H5R_DSET_REG_REF_BUF_SIZE];
} hdset_reg_ref_t;
//! <!-- [hdset_reg_ref_t_snip] -->

//! <!-- [H5R_ref_t_snip] -->
/**
 * Opaque reference type. The same reference type is used for object,
 * dataset region and attribute references. This is the type that
 * should always be used with the current reference API.
 */
typedef struct {
    union {
        uint8_t __data[H5R_REF_BUF_SIZE]; /**< opaque data */
        int64_t align;                    /**< ensures alignment */
    } u;
} H5R_ref_t;
//! <!-- [H5R_ref_t_snip] -->

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

/* Constructors */
/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Creates an object reference
 *
 * \fgdta_loc_id
 * \param[in] name      Name of object
 * \oapl_id
 * \param[out] ref_ptr  Pointer to reference
 *
 * \return \herr_t
 *
 * \details H5Rcreate_object() creates a reference pointing to the
 *          object named \p name located at \p loc_id. The parameters \p
 *          loc_id and \p name are used to locate the object.
 *
 *          The parameter \p oapl_id is an object access property list
 *          identifier for the referenced object. The access property list
 *          must be of the same type as the object being referenced, that is
 *          a group, dataset or committed datatype property list.
 *
 *          \ref H5R_ref_t is defined in H5Rpublic.h as:
 *          \snippet this H5R_ref_t_snip
 *
 *          The function returns a \p ref_ptr pointer, which must be released
 *          using H5Rdestroy() to avoid resource leaks and possible HDF5
 *          library shutdown issues.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Rcreate_object(hid_t loc_id, const char *name, hid_t oapl_id, H5R_ref_t *ref_ptr);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Creates a region reference
 *
 * \fgdta_loc_id
 * \param[in] name      Name of object
 * \space_id
 * \oapl_id
 * \param[out] ref_ptr  Pointer to reference
 *
 * \return \herr_t
 *
 * \details H5Rcreate_region() creates the reference, \p ref_ptr,
 *          pointing to the region represented by \p space_id within the
 *          object named name located at \p loc_id.
 *
 *          The parameters \p loc_id and \p name are used to locate the
 *          object. The parameter \p space_id identifies the dataset region
 *          that a dataset region reference points to.
 *
 *          The parameter \p oapl_id is an object access property list
 *          identifier for the referenced object. The access property list
 *          must be of the same type as the object being referenced, that is
 *          a dataset property list in this case.
 *
 *          \ref H5R_ref_t is defined in H5Rpublic.h as:
 *          \snippet this H5R_ref_t_snip
 *
 *          The function returns a \p ref_ptr pointer, which must be released
 *          using H5Rdestroy() to avoid resource leaks and possible HDF5
 *          library shutdown issues.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Rcreate_region(hid_t loc_id, const char *name, hid_t space_id, hid_t oapl_id,
                               H5R_ref_t *ref_ptr);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Creates an attribute reference
 *
 * \fgdta_loc_id
 * \param[in] name      Name of object
 * \param[in] attr_name Name of attribute
 * \oapl_id
 * \param[out] ref_ptr  Pointer to reference
 *
 * \return \herr_t
 *
 * \details H5Rcreate_attr() creates the reference, \p ref_ptr, pointing
 *          to the attribute named \p attr_name and attached to the object
 *          named \p name located at \p loc_id.
 *
 *          The parameters \p loc_id and \p name locate the object. The
 *          parameter \p attr_name specifies the attribute within the object.
 *
 *          The parameter \p oapl_id is an object access property list
 *          identifier for the object that the referenced attribute is
 *          attached to. The access property list must be of the same type
 *          as that object, that is a group, dataset or committed datatype
 *          property list.
 *
 *          \ref H5R_ref_t is defined in H5Rpublic.h as:
 *          \snippet this H5R_ref_t_snip
 *
 *          The function returns a \p ref_ptr pointer, which must be released
 *          using H5Rdestroy() to avoid resource leaks and possible HDF5
 *          library shutdown issues.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Rcreate_attr(hid_t loc_id, const char *name, const char *attr_name, hid_t oapl_id,
                             H5R_ref_t *ref_ptr);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Closes a reference
 *
 * \param[in] ref_ptr  Pointer to reference
 *
 * \return \herr_t
 *
 * \details Given a reference, \p ref_ptr, to an object, region or attribute
 *          attached to an object, H5Rdestroy() releases allocated resources
 *          from a previous create call.
 *
 *          \ref H5R_ref_t is defined in H5Rpublic.h as:
 *          \snippet this H5R_ref_t_snip
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Rdestroy(H5R_ref_t *ref_ptr);

/* Info */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Retrieves the type of a reference
 *
 * \param[in] ref_ptr  Pointer to reference
 *
 * \return Returns a valid reference type if successful; otherwise returns #H5R_BADTYPE .
 *
 * \details Given a reference, \p ref_ptr, H5Rget_type() returns the
 *          type of the reference.
 *
 *          Valid returned reference types are:
 *          \snippet this H5R_type_t_snip
 *
 *          Note that #H5R_OBJECT1 and #H5R_DATASET_REGION1 can never be
 *          associated with an \ref H5R_ref_t reference and can, therefore, never be
 *          returned through that function.
 *
 *          \ref H5R_ref_t is defined in H5Rpublic.h as:
 *          \snippet this H5R_ref_t_snip
 *
 * \since 1.12.0
 *
 */
H5_DLL H5R_type_t H5Rget_type(const H5R_ref_t *ref_ptr);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Determines whether two references are equal
 *
 * \param[in]  ref1_ptr  Pointer to reference to compare
 * \param[in]  ref2_ptr  Pointer to reference to compare
 *
 * \return Returns a positive value if the references are equal. Returns
 *          0 if the references are not equal. Returns a negative value when the
 *          function fails.
 *
 * \details H5Requal() determines whether two references point to the
 *          same object, region or attribute.
 *
 *          \ref H5R_ref_t is defined in H5Rpublic.h as:
 *          \snippet this H5R_ref_t_snip
 *
 * \since 1.12.0
 *
 */
H5_DLL htri_t H5Requal(const H5R_ref_t *ref1_ptr, const H5R_ref_t *ref2_ptr);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Copies an existing reference
 *
 * \param[in]  src_ref_ptr  Pointer to reference to copy
 * \param[out] dst_ref_ptr  Pointer to output reference
 *
 * \return \herr_t
 *
 * \details H5Rcopy() creates a copy of an existing reference.
 *          \p src_ref_ptr points to the reference to copy, and \p dst_ref_ptr is the
 *          pointer to the destination reference.
 *
 *          The function returns a \p dst_ref_ptr pointer, which must be released
 *          using H5Rdestroy() to avoid resource leaks and possible HDF5
 *          library shutdown issues.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Rcopy(const H5R_ref_t *src_ref_ptr, H5R_ref_t *dst_ref_ptr);

/* Dereference */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Opens the HDF5 object referenced
 *
 * \param[in] ref_ptr  Pointer to reference to open
 * \rapl_id
 * \oapl_id
 *
 * \return \hid_t{object}
 *
 * \details Given a reference, \p ref_ptr, to an object, a region in
 *          an object, or an attribute attached to an object, H5Ropen_object()
 *          opens that object and returns an identifier.
 *
 *          The parameter \p oapl_id is an object access property list
 *          identifier for the referenced object. The access property list
 *          must be of the same type as the object being referenced, that is
 *          a group or dataset property list.
 *
 *          \ref H5R_ref_t is defined in H5Rpublic.h as:
 *          \snippet this H5R_ref_t_snip
 *
 *          The object opened with this function should be closed when it
 *          is no longer needed so that resource leaks will not develop. Use
 *          the appropriate close function, such as H5Oclose() or H5Dclose()
 *          for datasets.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Ropen_object(H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t oapl_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Ropen_object}
 *
 * \since 1.14.0
 *
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Ropen_object_async(const char *app_file, const char *app_func, unsigned app_line,
                                  H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t oapl_id, hid_t es_id);
#else
H5_DLL hid_t H5Ropen_object_async(unsigned app_line, H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t oapl_id,
                                  hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Sets up a dataspace and selection as specified by a region reference.
 *
 * \param[in] ref_ptr  Pointer to reference to open
 * \rapl_id
 * \oapl_id
 *
 * \return \hid_t{dataspace}
 *
 * \details H5Ropen_region() creates a copy of the dataspace of the
 *          dataset pointed to by a region reference, \p ref_ptr, and defines
 *          a selection matching the selection pointed to by \p ref_ptr within
 *          the dataspace copy.
 *
 *          The parameter \p rapl id is a reference access property list
 *          identifier for the reference. The access property list can
 *          be used to access external files that the reference points to
 *          (through a file access property list).
 *
 *          The parameter \p oapl id is an object access property list
 *          identifier for the referenced object. The access property list
 *          must be of the same type as the object being referenced, that is
 *          a dataset property list in that case.
 *
 *          Use H5Sclose() to release the dataspace identifier returned by
 *          this function when the identifier is no longer needed.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Ropen_region(H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t oapl_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Ropen_region}
 *
 * \since 1.14.0
 *
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Ropen_region_async(const char *app_file, const char *app_func, unsigned app_line,
                                  H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t oapl_id, hid_t es_id);
#else
H5_DLL hid_t H5Ropen_region_async(H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t oapl_id, hid_t es_id);
#endif

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Opens the HDF5 attribute referenced
 *
 * \param[in] ref_ptr  Pointer to reference to open
 * \rapl_id
 * \aapl_id
 *
 * \return \hid_t{attribute}
 *
 * \details Given a reference, \p ref_ptr, to an attribute attached to
 *          an object, H5Ropen_attr() opens the attribute attached to that
 *          object and returns an identifier.
 *
 *          The parameter \p rapl id is a reference access property list
 *          identifier for the reference. The access property list can
 *          be used to access external files that the reference points to
 *          (through a file access property list).
 *
 *          The parameter \p aapl_id is an attribute access property list
 *          identifier for the referenced attribute.
 *
 *          The attribute opened with this function should be closed with
 *          H5Aclose() when it is no longer needed.
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5Ropen_attr(H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t aapl_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup ASYNC
 * \async_variant_of{H5Ropen_attr}
 *
 * \since 1.14.0
 */
#ifndef H5_DOXYGEN
H5_DLL hid_t H5Ropen_attr_async(const char *app_file, const char *app_func, unsigned app_line,
                                H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t aapl_id, hid_t es_id);
#else
H5_DLL hid_t H5Ropen_attr_async(H5R_ref_t *ref_ptr, hid_t rapl_id, hid_t aapl_id, hid_t es_id);
#endif

/* Get type */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Retrieves the type of object that an object reference points to
 *
 * \param[in] ref_ptr  Pointer to reference to query
 * \rapl_id
 * \param[out] obj_type Type of referenced object
 *
 * \return \herr_t
 *
 * \details Given a reference, \p ref_ptr, H5Rget_obj_type3() retrieves
 *          the type of the referenced object in \p obj_type.
 *
 *          The parameter \p rapl id is a reference access property list
 *          identifier for the reference. The access property list can
 *          be used to access external files that the reference points to
 *          (through a file access property list).
 *
 *          Upon success, the function returns in \p obj_type the type of
 *          the object that the reference points to.  Valid values for this
 *          referenced object type are as followed (defined in H5Opublic.h):
 *          \snippet H5Opublic.h H5O_type_t_snip
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Rget_obj_type3(H5R_ref_t *ref_ptr, hid_t rapl_id, H5O_type_t *obj_type);

/* Get name */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Retrieves the file name for a referenced object
 *
 * \param[in] ref_ptr  Pointer to reference to query
 * \param[in,out] name Buffer to place the file name of the reference
 * \param[in] size     Size of the \p name buffer. When the size is passed in,
 *                     the \c NULL terminator needs to be included.
 *
 * \return Returns the length of the name if successful, otherwise, a negative value.
 *
 * \details H5Rget_file_name() retrieves the file name for the object,
 *          region or attribute reference pointed to by \p ref_ptr.
 *
 *          \details_namelen{file,H5Rget_file_name}
 *
 * \since 1.12.0
 *
 */
H5_DLL ssize_t H5Rget_file_name(const H5R_ref_t *ref_ptr, char *name, size_t size);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Retrieves the object name for a referenced object
 *
 * \param[in] ref_ptr  Pointer to reference to query
 * \rapl_id
 * \param[in,out] name Buffer to place the object name of the reference
 * \param[in] size     Size of the \p name buffer. When the size is passed in,
 *                     the \c NULL terminator needs to be included.
 *
 * \return Returns the length of the name if successful, returning
 *          0 (zero) if no name is associated with the identifier. Otherwise
 *          returns a negative value.
 *
 * \details H5Rget_obj_name() retrieves the object name for the object,
 *          region or attribute reference pointed to by \p ref_ptr.
 *
 *          The parameter \p rapl_id is a reference access property list
 *          identifier for the reference. The access property list can
 *          be used to access external files that the reference points to
 *          (through a file access property list).
 *
 *          \details_namelen{object,H5Rget_obj_name}
 *
 *          If \p ref_ptr is an object reference, \p name will be returned with
 *          a name for the referenced object. If \p ref_ptr is a dataset region
 *          reference, \p name will contain a name for the object containing
 *          the referenced region. If \p ref_ptr is an attribute reference, \p
 *          name will contain a name for the object the attribute is attached
 *          to. Note that an object in an HDF5 file may have multiple paths
 *          if there are multiple links pointing to it. This function may
 *          return any one of these paths.
 *
 * \since 1.12.0
 *
 */
H5_DLL ssize_t H5Rget_obj_name(H5R_ref_t *ref_ptr, hid_t rapl_id, char *name, size_t size);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Retrieves the attribute name for a referenced object
 *
 * \param[in] ref_ptr  Pointer to reference to query
 * \param[in,out] name Buffer to place the attribute name of the reference
 * \param[in] size     Size of the \p name buffer
 *
 * \return Returns the length of the name if successful, otherwise, a negative value.
 *
 * \details H5Rget_attr_name() retrieves the attribute name for the
 *          attribute reference pointed to by \p ref_ptr.
 *
 *          \details_namelen{attribute,H5Rget_attr_name}
 *
 * \since 1.12.0
 *
 */
H5_DLL ssize_t H5Rget_attr_name(const H5R_ref_t *ref_ptr, char *name, size_t size);

/// \cond DEV
/* API Wrappers for async routines */
/* (Must be defined _after_ the function prototype) */
/* (And must only defined when included in application code, not the library) */
#ifndef H5R_MODULE
#define H5Ropen_object_async(...) H5Ropen_object_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Ropen_region_async(...) H5Ropen_region_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Ropen_attr_async(...)   H5Ropen_attr_async(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Define "wrapper" versions of function calls, to allow compile-time values to
 * be passed in by language wrapper or library layer on top of HDF5. */
#define H5Ropen_object_async_wrap H5_NO_EXPAND(H5Ropen_object_async)
#define H5Ropen_region_async_wrap H5_NO_EXPAND(H5Ropen_region_async)
#define H5Ropen_attr_async_wrap   H5_NO_EXPAND(H5Ropen_attr_async)
#endif /* H5R_MODULE */
/// \endcond

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is or will be deprecated.
 */

/* Macros */

/* Versions for compatibility */
#define H5R_OBJECT         H5R_OBJECT1
#define H5R_DATASET_REGION H5R_DATASET_REGION1

/* Function prototypes */
#ifndef H5_NO_DEPRECATED_SYMBOLS

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Retrieves the type of object that an object reference points to
 *
 * \param[in] id The dataset containing the reference object or the group
 *            containing that dataset
 * \param[in] ref_type Type of reference to query
 * \param[in] ref Reference to query
 *
 * \return Returns a valid object type if successful; otherwise returns a
 *         negative value (#H5G_UNKNOWN).
 *
 * \deprecated This function has been renamed from H5Rget_obj_type() and is
 *             deprecated in favor of the macro H5Rget_obj_type() or the
 *             function H5Rget_obj_type2().
 *
 * \details Given an object reference, \p ref, H5Rget_obj_type1() returns the
 *          type of the referenced object.
 *
 *          A \Emph{reference type} is the type of reference, either an object
 *          reference or a dataset region reference. An \Emph{object reference}
 *          points to an HDF5 object, while a \Emph{dataset region reference}
 *          points to a defined region within a dataset.
 *
 *          The \Emph{referenced object} is the object the reference points
 *          to. The \Emph{referenced object type}, or the type of the referenced
 *          object, is the type of the object that the reference points to.
 *
 *          The location identifier, \p id, is the identifier for either the
 *          dataset containing the object reference or the group containing that
 *          dataset.
 *
 *          Valid reference types, to pass in as \p ref_type, include the
 *          following:
 *          \snippet this H5R_type_t_snip
 *
 *          If the application does not already know the object reference type,
 *          that can be determined with three preliminary calls:
 *
 *          \li Call H5Dget_type() on the dataset containing the reference to
 *              get a datatype identifier for the dataset's datatype.
 *          \li Using that datatype identifier, H5Tget_class() returns a datatype
 *              class.\n If the datatype class is #H5T_REFERENCE, H5Tequal() can
 *              then be used to determine whether the reference's datatype is
 *              #H5T_STD_REF_OBJ or #H5T_STD_REF_DSETREG:
 *              - If the datatype is #H5T_STD_REF_OBJ, the reference object type
 *                is #H5R_OBJECT.
 *              - If the datatype is #H5T_STD_REF_DSETREG, the reference object
 *                type is #H5R_DATASET_REGION.
 *
 *          When the function completes successfully, it returns one of the
 *          following valid object type values (defined in H5Gpublic.h):
 *          \snippet H5Gpublic.h H5G_obj_t_snip
 *
 * \version 1.8.0 Function H5Rget_obj_type() renamed to H5Rget_obj_type1() and
 *                deprecated in this release.
 *
 * \since 1.2.0
 *
 */
H5_DLL H5G_obj_t H5Rget_obj_type1(hid_t id, H5R_type_t ref_type, const void *ref);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Opens the HDF5 object referenced
 *
 * \obj_id
 * \param[in] ref_type The reference type of \p ref
 * \param[in] ref Reference to open
 *
 * \return Returns identifier of referenced object if successful; otherwise
 *         returns a negative value.
 *
 * \deprecated This function has been renamed from H5Rdereference() and is
 *             deprecated in favor of the macro H5Rdereference() or the function
 *             H5Rdereference2().
 *
 * \details Given a reference, \p ref, to an object or a region in an object,
 *          H5Rdereference1() opens that object and returns an identifier.
 *
 *          The parameter \p obj_id must be a valid identifier for an object in
 *          the HDF5 file containing the referenced object, including the file
 *          identifier.
 *
 *          The parameter \p ref_type specifies the reference type of the
 *          reference \p ref. \p ref_type may contain either of the following
 *          values:
 *          - #H5R_OBJECT
 *          - #H5R_DATASET_REGION
 *
 *          The object opened with this function should be closed when it is no
 *          longer needed so that resource leaks will not develop. Use the
 *          appropriate close function, such as H5Oclose() or H5Dclose() for
 *          datasets.
 *
 * \version 1.10.0 Function H5Rdereference() renamed to H5Rdereference1() and
 *                 deprecated in this release.
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Rdereference1(hid_t obj_id, H5R_type_t ref_type, const void *ref);

#endif /* H5_NO_DEPRECATED_SYMBOLS */

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Creates a reference
 *
 * \param[out] ref Reference created by the function call
 * \param[in] loc_id Location identifier used to locate the object being pointed to
 * \param[in] name Name of object at location \p loc_id
 * \param[in] ref_type Type of reference
 * \param[in] space_id Dataspace identifier with selection. Used only for
 *                     dataset region references; pass as -1 if reference is
 *                     an object reference, i.e., of type #H5R_OBJECT
 *
 * \return \herr_t
 *
 * \deprecated As of HDF5-1.12, this function has been deprecated in favor of
 *             H5Rcreate_object(), H5Rcreate_region() and H5Rcreate_attr().
 *
 * \details H5Rcreate() creates the reference, \p ref, of the type specified in
 *          \p ref_type, pointing to the object \p name located at \p loc_id.
 *
 *          The HDF5 library maps the void type specified above for \p ref to
 *          the type specified in \p ref_type, which will be one of the following:
 *          \snippet this H5R_type_t_snip
 *
 *          The parameters \p loc_id and \p name are used to locate the object.
 *
 *          The parameter \p space_id identifies the dataset region that a
 *          dataset region reference points to. This parameter is used only with
 *          dataset region references and should be set to -1 if the reference
 *          is an object reference, #H5R_OBJECT.
 *
 * \since 1.0.0
 */
H5_DLL herr_t H5Rcreate(void *ref, hid_t loc_id, const char *name, H5R_type_t ref_type, hid_t space_id);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Retrieves the type of object that an object reference points to
 *
 * \param[in] id The dataset containing the reference object or the group
 *            containing that dataset
 * \param[in] ref_type Type of reference to query
 * \param[in] ref Reference to query
 * \param[out] obj_type Type of referenced object
 *
 * \return \herr_t
 *
 * \details Given an object reference, \p ref, H5Rget_obj_type2() returns the
 *          type of the referenced object in \p obj_type.
 *
 *          A \Emph{reference type} is the type of reference, either an object
 *          reference or a dataset region reference. An \Emph{object reference}
 *          points to an HDF5 object while a \Emph{dataset region reference}
 *          points to a defined region within a dataset.
 *
 *          The \Emph{referenced object} is the object the reference points
 *          to. The \Emph{referenced object type}, or the type of the referenced
 *          object, is the type of the object that the reference points to.
 *
 *          The location identifier, \p id, is the identifier for either the
 *          dataset containing the object reference or the group containing that
 *          dataset.
 *
 *          Valid reference types, to pass in as \p ref_type, include the
 *          following:
 *          \snippet this H5R_type_t_snip
 *
 *          If the application does not already know the object reference type,
 *          that can be determined with three preliminary calls:
 *
 *          \li Call H5Dget_type() on the dataset containing the reference to
 *              get a datatype identifier for the dataset's datatype.
 *          \li Using that datatype identifier, H5Tget_class() returns a datatype
 *              class.\n If the datatype class is #H5T_REFERENCE, H5Tequal() can
 *              then be used to determine whether the reference's datatype is
 *              #H5T_STD_REF_OBJ or #H5T_STD_REF_DSETREG:
 *              - If the datatype is #H5T_STD_REF_OBJ, the reference object type
 *                is #H5R_OBJECT.
 *              - If the datatype is #H5T_STD_REF_DSETREG, the reference object
 *                type is #H5R_DATASET_REGION.
 *
 *          When the function completes successfully, it returns one of the
 *          following valid object type values (defined in H5Opublic.h):
 *          \snippet H5Opublic.h H5O_type_t_snip
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Rget_obj_type2(hid_t id, H5R_type_t ref_type, const void *ref, H5O_type_t *obj_type);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Opens the HDF5 object referenced
 *
 * \obj_id
 * \oapl_id
 * \param[in] ref_type The reference type of \p ref
 * \param[in] ref Reference to open
 *
 * \return Returns identifier of referenced object if successful; otherwise
 *         returns a negative value.
 *
 * \deprecated As of HDF5-1.12, this function and H5Rdereference1() have been
 *             deprecated in favor of H5Ropen_attr(), H5Ropen_object()
 *             and H5Ropen_region().
 *
 * \details Given a reference, \p ref, to an object or a region in an object,
 *          H5Rdereference2() opens that object and returns an identifier.
 *
 *          The parameter \p obj_id must be a valid identifier for the HDF5 file
 *          containing the referenced object or for any object in that HDF5
 *          file.
 *
 *          The parameter \p oapl_id is an object access property list
 *          identifier for the referenced object. The access property list must
 *          be of the same type as the object being referenced, that is a group,
 *          dataset, or datatype property list.
 *
 *          The parameter \p ref_type specifies the reference type of the
 *          reference \p ref. \p ref_type may contain either of the following
 *          values:
 *          - #H5R_OBJECT
 *          - #H5R_DATASET_REGION
 *
 *          The object opened with this function should be closed when it is no
 *          longer needed so that resource leaks will not develop. Use the
 *          appropriate close function, such as H5Oclose() or H5Dclose() for
 *          datasets.
 *
 * \since 1.10.0
 *
 */
H5_DLL hid_t H5Rdereference2(hid_t obj_id, hid_t oapl_id, H5R_type_t ref_type, const void *ref);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Sets up a dataspace and selection as specified by a region reference
 *
 * \param[in] dataset File identifier or identifier for any object in the file
 *                    containing the referenced region
 * \param[in] ref_type Reference type of \p ref, which must be #H5R_DATASET_REGION
 * \param[in] ref Region reference to open
 *
 * \return Returns a valid dataspace identifier if successful; otherwise returns
 *         a negative value.
 *
 * \details H5Rget_region() creates a copy of the dataspace of the dataset
 *          pointed to by a region reference, \p ref, and defines a selection
 *          matching the selection pointed to by ref within the dataspace copy.
 *
 *          \p dataset is used to identify the file containing the referenced
 *          region; it can be a file identifier or an identifier for any object
 *          in the file.
 *
 *          The parameter \p ref_type specifies the reference type of \p ref and
 *          must contain the value #H5R_DATASET_REGION.
 *
 *          Use H5Sclose() to release the dataspace identifier returned by this
 *          function when the identifier is no longer needed.
 *
 * \since 1.0.0
 *
 */
H5_DLL hid_t H5Rget_region(hid_t dataset, H5R_type_t ref_type, const void *ref);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5R
 *
 * \brief Retrieves a name for a referenced object
 *
 * \param[in] loc_id Identifier for the file containing the reference or for
 *                   any object in that file
 * \param[in] ref_type Type of reference
 * \param[in] ref An object or dataset region reference
 * \param[out] name A buffer to place the name of the referenced object or
 *                  dataset region. If \c NULL, then this call will return the
 *                  size in bytes of the name.
 * \param[in] size The size of the \p name buffer. When the size is passed in,
 *                 the \c NULL terminator needs to be included.
 *
 * \return Returns the length of the name if successful, returning 0 (zero) if
 *         no name is associated with the identifier. Otherwise returns a
 *         negative value.
 *
 * \deprecated As of HDF5-1.12, this function has been deprecated in favor of
 *             H5Rget_file_name(), H5Rget_obj_name() and H5Rget_attr_name().
 *
 * \details H5Rget_name() retrieves a name for the object identified by \p ref.\n
 *          \p loc_id is used to identify the file containing the reference. It
 *          can be the file identifier for the file containing the reference or
 *          an identifier for any object in that file.
 *
 *          \ref H5R_type_t is the reference type of \p ref. Valid values
 *          include the following:
 *          \snippet this H5R_type_t_snip
 *
 *          \p ref is the reference for which the target object's name is
 *          sought.
 *
 *          If \p ref is an object reference, \p name will be returned with a
 *          name for the referenced object. If \p ref is a dataset region
 *          reference, \p name will contain a name for the object containing the
 *          referenced region.
 *
 *          \details_namelen{,H5Rget_name}
 *
 *          \note An object in an HDF5 file may have multiple paths if multiple
 *          links point to it. This function may return any one of these paths.
 *
 * \since 1.8.0
 */
H5_DLL ssize_t H5Rget_name(hid_t loc_id, H5R_type_t ref_type, const void *ref, char *name, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* H5Rpublic_H */
