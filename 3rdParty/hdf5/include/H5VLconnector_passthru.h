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
 * This file contains public declarations for authoring VOL connectors
 * which act as "passthrough" connectors that forward their API calls to
 * an underlying connector.
 *
 * An example of this might be a logging connector, which creates log messages
 * and then passes the call on to an underlying VOL connector.
 *
 * The functionality required to implement such a connector is specialized
 * and non-trivial so it has been split into this header in an effort to keep
 * the H5VLconnector.h header easier to understand.
 */

#ifndef H5VLconnector_passthru_H
#define H5VLconnector_passthru_H

/* Public headers needed by this file */
#include "H5public.h"   /* Generic Functions                    */
#include "H5Ipublic.h"  /* IDs                                  */
#include "H5VLpublic.h" /* Virtual Object Layer                 */

/* Semi-public headers mainly for VOL connector authors */
#include "H5VLconnector.h"

/*****************/
/* Public Macros */
/*****************/

/*******************/
/* Public Typedefs */
/*******************/

/********************/
/* Public Variables */
/********************/

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

/* Helper routines for VOL connector authors */
H5_DLL herr_t H5VLcmp_connector_cls(int *cmp, hid_t connector_id1, hid_t connector_id2);
/**
 * \ingroup H5VL
 *
 * \brief Wrap an internal object with a "wrap context" and register an
 *        hid_t for the resulting object.
 *
 * \param[in] obj  VOL object.
 * \param[in] type VOL-managed object class. Allowable values are:
 *                 - #H5I_FILE
 *                 - #H5I_GROUP
 *                 - #H5I_DATATYPE
 *                 - #H5I_DATASET
 *                 - #H5I_MAP
 *                 - #H5I_ATTR
 *
 * \return \hid_t{VOL connector}
 *
 * \note This routine is mainly targeted toward wrapping objects for
 *       iteration routine callbacks (i.e. the callbacks from H5Aiterate*,
 *       H5Literate* / H5Lvisit*, and H5Ovisit* ). Using it in an application
 *       will return an error indicating the API context isn't available or
 *       can't be retrieved.
 *
 */
H5_DLL hid_t  H5VLwrap_register(void *obj, H5I_type_t type);
H5_DLL herr_t H5VLretrieve_lib_state(void **state);
H5_DLL herr_t H5VLstart_lib_state(void);
H5_DLL herr_t H5VLrestore_lib_state(const void *state);
H5_DLL herr_t H5VLfinish_lib_state(void);
H5_DLL herr_t H5VLfree_lib_state(void *state);

/* Pass-through callbacks */
H5_DLL void  *H5VLget_object(void *obj, hid_t connector_id);
H5_DLL herr_t H5VLget_wrap_ctx(void *obj, hid_t connector_id, void **wrap_ctx);
H5_DLL void  *H5VLwrap_object(void *obj, H5I_type_t obj_type, hid_t connector_id, void *wrap_ctx);
H5_DLL void  *H5VLunwrap_object(void *obj, hid_t connector_id);
H5_DLL herr_t H5VLfree_wrap_ctx(void *wrap_ctx, hid_t connector_id);

/* Public wrappers for generic callbacks */
H5_DLL herr_t H5VLinitialize(hid_t connector_id, hid_t vipl_id);
H5_DLL herr_t H5VLterminate(hid_t connector_id);
H5_DLL herr_t H5VLget_cap_flags(hid_t connector_id, uint64_t *cap_flags);
H5_DLL herr_t H5VLget_value(hid_t connector_id, H5VL_class_value_t *conn_value);

/* Public wrappers for info fields and callbacks */
H5_DLL herr_t H5VLcopy_connector_info(hid_t connector_id, void **dst_vol_info, void *src_vol_info);
H5_DLL herr_t H5VLcmp_connector_info(int *cmp, hid_t connector_id, const void *info1, const void *info2);
H5_DLL herr_t H5VLfree_connector_info(hid_t connector_id, void *vol_info);
H5_DLL herr_t H5VLconnector_info_to_str(const void *info, hid_t connector_id, char **str);
H5_DLL herr_t H5VLconnector_str_to_info(const char *str, hid_t connector_id, void **info);

/* Public wrappers for attribute callbacks */
H5_DLL void  *H5VLattr_create(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                              const char *attr_name, hid_t type_id, hid_t space_id, hid_t acpl_id,
                              hid_t aapl_id, hid_t dxpl_id, void **req);
H5_DLL void  *H5VLattr_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                            const char *name, hid_t aapl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLattr_read(void *attr, hid_t connector_id, hid_t dtype_id, void *buf, hid_t dxpl_id,
                            void **req);
H5_DLL herr_t H5VLattr_write(void *attr, hid_t connector_id, hid_t dtype_id, const void *buf, hid_t dxpl_id,
                             void **req);
H5_DLL herr_t H5VLattr_get(void *obj, hid_t connector_id, H5VL_attr_get_args_t *args, hid_t dxpl_id,
                           void **req);
H5_DLL herr_t H5VLattr_specific(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                                H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLattr_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                                void **req);
H5_DLL herr_t H5VLattr_close(void *attr, hid_t connector_id, hid_t dxpl_id, void **req);

/* Public wrappers for dataset callbacks */
H5_DLL void  *H5VLdataset_create(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                                 const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id, hid_t dcpl_id,
                                 hid_t dapl_id, hid_t dxpl_id, void **req);
H5_DLL void  *H5VLdataset_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                               const char *name, hid_t dapl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLdataset_read(size_t count, void *dset[], hid_t connector_id, hid_t mem_type_id[],
                               hid_t mem_space_id[], hid_t file_space_id[], hid_t plist_id, void *buf[],
                               void **req);
H5_DLL herr_t H5VLdataset_write(size_t count, void *dset[], hid_t connector_id, hid_t mem_type_id[],
                                hid_t mem_space_id[], hid_t file_space_id[], hid_t plist_id,
                                const void *buf[], void **req);
H5_DLL herr_t H5VLdataset_get(void *dset, hid_t connector_id, H5VL_dataset_get_args_t *args, hid_t dxpl_id,
                              void **req);
H5_DLL herr_t H5VLdataset_specific(void *obj, hid_t connector_id, H5VL_dataset_specific_args_t *args,
                                   hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLdataset_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                                   void **req);
H5_DLL herr_t H5VLdataset_close(void *dset, hid_t connector_id, hid_t dxpl_id, void **req);

/* Public wrappers for named datatype callbacks */
H5_DLL void  *H5VLdatatype_commit(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                                  const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id,
                                  hid_t dxpl_id, void **req);
H5_DLL void  *H5VLdatatype_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                                const char *name, hid_t tapl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLdatatype_get(void *dt, hid_t connector_id, H5VL_datatype_get_args_t *args, hid_t dxpl_id,
                               void **req);
H5_DLL herr_t H5VLdatatype_specific(void *obj, hid_t connector_id, H5VL_datatype_specific_args_t *args,
                                    hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLdatatype_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                                    void **req);
H5_DLL herr_t H5VLdatatype_close(void *dt, hid_t connector_id, hid_t dxpl_id, void **req);

/* Public wrappers for file callbacks */
H5_DLL void  *H5VLfile_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id,
                              void **req);
H5_DLL void  *H5VLfile_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLfile_get(void *file, hid_t connector_id, H5VL_file_get_args_t *args, hid_t dxpl_id,
                           void **req);
H5_DLL herr_t H5VLfile_specific(void *obj, hid_t connector_id, H5VL_file_specific_args_t *args, hid_t dxpl_id,
                                void **req);
H5_DLL herr_t H5VLfile_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                                void **req);
H5_DLL herr_t H5VLfile_close(void *file, hid_t connector_id, hid_t dxpl_id, void **req);

/* Public wrappers for group callbacks */
H5_DLL void  *H5VLgroup_create(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                               const char *name, hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id,
                               void **req);
H5_DLL void  *H5VLgroup_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                             const char *name, hid_t gapl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLgroup_get(void *obj, hid_t connector_id, H5VL_group_get_args_t *args, hid_t dxpl_id,
                            void **req);
H5_DLL herr_t H5VLgroup_specific(void *obj, hid_t connector_id, H5VL_group_specific_args_t *args,
                                 hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLgroup_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                                 void **req);
H5_DLL herr_t H5VLgroup_close(void *grp, hid_t connector_id, hid_t dxpl_id, void **req);

/* Public wrappers for link callbacks */
H5_DLL herr_t H5VLlink_create(H5VL_link_create_args_t *args, void *obj, const H5VL_loc_params_t *loc_params,
                              hid_t connector_id, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLlink_copy(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                            const H5VL_loc_params_t *loc_params2, hid_t connector_id, hid_t lcpl_id,
                            hid_t lapl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLlink_move(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                            const H5VL_loc_params_t *loc_params2, hid_t connector_id, hid_t lcpl_id,
                            hid_t lapl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLlink_get(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                           H5VL_link_get_args_t *args, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLlink_specific(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                                H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLlink_optional(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                                H5VL_optional_args_t *args, hid_t dxpl_id, void **req);

/* Public wrappers for object callbacks */
H5_DLL void  *H5VLobject_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                              H5I_type_t *opened_type, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLobject_copy(void *src_obj, const H5VL_loc_params_t *loc_params1, const char *src_name,
                              void *dst_obj, const H5VL_loc_params_t *loc_params2, const char *dst_name,
                              hid_t connector_id, hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLobject_get(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                             H5VL_object_get_args_t *args, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLobject_specific(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                                  H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req);
H5_DLL herr_t H5VLobject_optional(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                                  H5VL_optional_args_t *args, hid_t dxpl_id, void **req);

/* Public wrappers for connector/container introspection callbacks */
H5_DLL herr_t H5VLintrospect_get_conn_cls(void *obj, hid_t connector_id, H5VL_get_conn_lvl_t lvl,
                                          const H5VL_class_t **conn_cls);
H5_DLL herr_t H5VLintrospect_get_cap_flags(const void *info, hid_t connector_id, uint64_t *cap_flags);
H5_DLL herr_t H5VLintrospect_opt_query(void *obj, hid_t connector_id, H5VL_subclass_t subcls, int opt_type,
                                       uint64_t *flags);

/* Public wrappers for asynchronous request callbacks */
H5_DLL herr_t H5VLrequest_wait(void *req, hid_t connector_id, uint64_t timeout,
                               H5VL_request_status_t *status);
H5_DLL herr_t H5VLrequest_notify(void *req, hid_t connector_id, H5VL_request_notify_t cb, void *ctx);
H5_DLL herr_t H5VLrequest_cancel(void *req, hid_t connector_id, H5VL_request_status_t *status);
H5_DLL herr_t H5VLrequest_specific(void *req, hid_t connector_id, H5VL_request_specific_args_t *args);
H5_DLL herr_t H5VLrequest_optional(void *req, hid_t connector_id, H5VL_optional_args_t *args);
H5_DLL herr_t H5VLrequest_free(void *req, hid_t connector_id);

/* Public wrappers for blob callbacks */
H5_DLL herr_t H5VLblob_put(void *obj, hid_t connector_id, const void *buf, size_t size, void *blob_id,
                           void *ctx);
H5_DLL herr_t H5VLblob_get(void *obj, hid_t connector_id, const void *blob_id, void *buf, size_t size,
                           void *ctx);
H5_DLL herr_t H5VLblob_specific(void *obj, hid_t connector_id, void *blob_id,
                                H5VL_blob_specific_args_t *args);
H5_DLL herr_t H5VLblob_optional(void *obj, hid_t connector_id, void *blob_id, H5VL_optional_args_t *args);

/* Public wrappers for token callbacks */
H5_DLL herr_t H5VLtoken_cmp(void *obj, hid_t connector_id, const H5O_token_t *token1,
                            const H5O_token_t *token2, int *cmp_value);
H5_DLL herr_t H5VLtoken_to_str(void *obj, H5I_type_t obj_type, hid_t connector_id, const H5O_token_t *token,
                               char **token_str);
H5_DLL herr_t H5VLtoken_from_str(void *obj, H5I_type_t obj_type, hid_t connector_id, const char *token_str,
                                 H5O_token_t *token);

/* Public wrappers for generic 'optional' callback */
H5_DLL herr_t H5VLoptional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                           void **req);

#ifdef __cplusplus
}
#endif

#endif /* H5VLconnector_passthru_H */
