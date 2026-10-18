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
 * This file contains public declarations for authoring VOL connectors.
 */

#ifndef H5VLconnector_H
#define H5VLconnector_H

/* Public headers needed by this file */
#include "H5public.h"   /* Generic Functions                    */
#include "H5Apublic.h"  /* Attributes                           */
#include "H5Dpublic.h"  /* Datasets                             */
#include "H5ESpublic.h" /* Event Stack                          */
#include "H5Fpublic.h"  /* Files                                */
#include "H5Ipublic.h"  /* IDs                                  */
#include "H5Lpublic.h"  /* Links                                */
#include "H5Opublic.h"  /* Objects                              */
#include "H5Rpublic.h"  /* References                           */
#include "H5VLpublic.h" /* Virtual Object Layer                 */

/*****************/
/* Public Macros */
/*****************/

/* Container info version */
#define H5VL_CONTAINER_INFO_VERSION 0x01 /* Container info struct version */

/* The maximum size allowed for blobs */
#define H5VL_MAX_BLOB_ID_SIZE (16) /* Allow for 128-bits blob IDs */

/* # of optional operations reserved for the native VOL connector */
#define H5VL_RESERVED_NATIVE_OPTIONAL 1024

/*******************/
/* Public Typedefs */
/*******************/

/* Types for different ways that objects are located in an HDF5 container */
typedef enum H5VL_loc_type_t {
    H5VL_OBJECT_BY_SELF,
    H5VL_OBJECT_BY_NAME,
    H5VL_OBJECT_BY_IDX,
    H5VL_OBJECT_BY_TOKEN
} H5VL_loc_type_t;

typedef struct H5VL_loc_by_name {
    const char *name;
    hid_t       lapl_id;
} H5VL_loc_by_name_t;

typedef struct H5VL_loc_by_idx {
    const char     *name;
    H5_index_t      idx_type;
    H5_iter_order_t order;
    hsize_t         n;
    hid_t           lapl_id;
} H5VL_loc_by_idx_t;

typedef struct H5VL_loc_by_token {
    H5O_token_t *token;
} H5VL_loc_by_token_t;

/* Structure to hold parameters for object locations.
 * Either: BY_SELF, BY_NAME, BY_IDX, BY_TOKEN
 *
 * Note: Leave loc_by_token as the first union member so we
 *       can perform the simplest initialization of the struct
 *       without raising warnings.
 *
 * Note: BY_SELF requires no union members.
 */
typedef struct H5VL_loc_params_t {
    H5I_type_t      obj_type;
    H5VL_loc_type_t type;
    union {
        H5VL_loc_by_token_t loc_by_token;
        H5VL_loc_by_name_t  loc_by_name;
        H5VL_loc_by_idx_t   loc_by_idx;
    } loc_data;
} H5VL_loc_params_t;

/* Struct for all 'optional' callbacks */
typedef struct H5VL_optional_args_t {
    int   op_type; /* Operation to perform */
    void *args;    /* Pointer to operation's argument struct */
} H5VL_optional_args_t;

/* Values for attribute 'get' operations */
typedef enum H5VL_attr_get_t {
    H5VL_ATTR_GET_ACPL,         /* creation property list              */
    H5VL_ATTR_GET_INFO,         /* info                                */
    H5VL_ATTR_GET_NAME,         /* access property list                */
    H5VL_ATTR_GET_SPACE,        /* dataspace                           */
    H5VL_ATTR_GET_STORAGE_SIZE, /* storage size                        */
    H5VL_ATTR_GET_TYPE          /* datatype                            */
} H5VL_attr_get_t;

/* Parameters for attribute 'get_name' operation */
typedef struct H5VL_attr_get_name_args_t {
    H5VL_loc_params_t loc_params;    /* Location parameters for object access */
    size_t            buf_size;      /* Size of attribute name buffer */
    char             *buf;           /* Buffer for attribute name (OUT) */
    size_t           *attr_name_len; /* Actual length of attribute name (OUT) */
} H5VL_attr_get_name_args_t;

/* Parameters for attribute 'get_info' operation */
typedef struct H5VL_attr_get_info_args_t {
    H5VL_loc_params_t loc_params; /* Location parameters for object access */
    const char       *attr_name;  /* Attribute name (for get_info_by_name) */
    H5A_info_t       *ainfo;      /* Attribute info (OUT) */
} H5VL_attr_get_info_args_t;

/* Parameters for attribute 'get' operations */
typedef struct H5VL_attr_get_args_t {
    H5VL_attr_get_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_ATTR_GET_ACPL */
        struct {
            hid_t acpl_id; /* Attribute creation property list ID (OUT) */
        } get_acpl;

        /* H5VL_ATTR_GET_INFO */
        H5VL_attr_get_info_args_t get_info; /* Attribute info */

        /* H5VL_ATTR_GET_NAME */
        H5VL_attr_get_name_args_t get_name; /* Attribute name */

        /* H5VL_ATTR_GET_SPACE */
        struct {
            hid_t space_id; /* Dataspace ID (OUT) */
        } get_space;

        /* H5VL_ATTR_GET_STORAGE_SIZE */
        struct {
            hsize_t *data_size; /* Size of attribute in file (OUT) */
        } get_storage_size;

        /* H5VL_ATTR_GET_TYPE */
        struct {
            hid_t type_id; /* Datatype ID (OUT) */
        } get_type;
    } args;
} H5VL_attr_get_args_t;

/* Values for attribute 'specific' operation */
typedef enum H5VL_attr_specific_t {
    H5VL_ATTR_DELETE,        /* H5Adelete(_by_name)  */
    H5VL_ATTR_DELETE_BY_IDX, /* H5Adelete_by_idx     */
    H5VL_ATTR_EXISTS,        /* H5Aexists(_by_name)  */
    H5VL_ATTR_ITER,          /* H5Aiterate(_by_name) */
    H5VL_ATTR_RENAME         /* H5Arename(_by_name)  */
} H5VL_attr_specific_t;

/* Parameters for attribute 'iterate' operation */
typedef struct H5VL_attr_iterate_args_t {
    H5_index_t      idx_type; /* Type of index to iterate over */
    H5_iter_order_t order;    /* Order of index iteration */
    hsize_t        *idx;      /* Start/stop iteration index (IN/OUT) */
    H5A_operator2_t op;       /* Iteration callback function */
    void           *op_data;  /* Iteration callback context */
} H5VL_attr_iterate_args_t;

/* Parameters for attribute 'delete_by_idx' operation */
typedef struct H5VL_attr_delete_by_idx_args_t {
    H5_index_t      idx_type; /* Type of index to iterate over */
    H5_iter_order_t order;    /* Order of index iteration */
    hsize_t         n;        /* Iteration index */
} H5VL_attr_delete_by_idx_args_t;

/* Parameters for attribute 'specific' operations */
typedef struct H5VL_attr_specific_args_t {
    H5VL_attr_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_ATTR_DELETE */
        struct {
            const char *name; /* Name of attribute to delete */
        } del;

        /* H5VL_ATTR_DELETE_BY_IDX */
        H5VL_attr_delete_by_idx_args_t delete_by_idx;

        /* H5VL_ATTR_EXISTS */
        struct {
            const char *name;   /* Name of attribute to check */
            hbool_t    *exists; /* Whether attribute exists (OUT) */
        } exists;

        /* H5VL_ATTR_ITER */
        H5VL_attr_iterate_args_t iterate;

        /* H5VL_ATTR_RENAME */
        struct {
            const char *old_name; /* Name of attribute to rename */
            const char *new_name; /* New attribute name */
        } rename;
    } args;
} H5VL_attr_specific_args_t;

/* Typedef for VOL connector attribute optional VOL operations */
typedef int H5VL_attr_optional_t;

/* Values for dataset 'get' operation */
typedef enum H5VL_dataset_get_t {
    H5VL_DATASET_GET_DAPL,         /* access property list                */
    H5VL_DATASET_GET_DCPL,         /* creation property list              */
    H5VL_DATASET_GET_SPACE,        /* dataspace                           */
    H5VL_DATASET_GET_SPACE_STATUS, /* space status                        */
    H5VL_DATASET_GET_STORAGE_SIZE, /* storage size                        */
    H5VL_DATASET_GET_TYPE          /* datatype                            */
} H5VL_dataset_get_t;

/* Parameters for dataset 'get' operations */
typedef struct H5VL_dataset_get_args_t {
    H5VL_dataset_get_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_DATASET_GET_DAPL */
        struct {
            hid_t dapl_id; /* Dataset access property list ID (OUT) */
        } get_dapl;

        /* H5VL_DATASET_GET_DCPL */
        struct {
            hid_t dcpl_id; /* Dataset creation property list ID (OUT) */
        } get_dcpl;

        /* H5VL_DATASET_GET_SPACE */
        struct {
            hid_t space_id; /* Dataspace ID (OUT) */
        } get_space;

        /* H5VL_DATASET_GET_SPACE_STATUS */
        struct {
            H5D_space_status_t *status; /* Storage space allocation status (OUT) */
        } get_space_status;

        /* H5VL_DATASET_GET_STORAGE_SIZE */
        struct {
            hsize_t *storage_size; /* Size of dataset's storage (OUT) */
        } get_storage_size;

        /* H5VL_DATASET_GET_TYPE */
        struct {
            hid_t type_id; /* Datatype ID (OUT) */
        } get_type;
    } args;
} H5VL_dataset_get_args_t;

/* Values for dataset 'specific' operation */
typedef enum H5VL_dataset_specific_t {
    H5VL_DATASET_SET_EXTENT, /* H5Dset_extent                       */
    H5VL_DATASET_FLUSH,      /* H5Dflush                            */
    H5VL_DATASET_REFRESH     /* H5Drefresh                          */
} H5VL_dataset_specific_t;

/* Parameters for dataset 'specific' operations */
typedef struct H5VL_dataset_specific_args_t {
    H5VL_dataset_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_DATASET_SET_EXTENT */
        struct {
            const hsize_t *size; /* New dataspace extent */
        } set_extent;

        /* H5VL_DATASET_FLUSH */
        struct {
            hid_t dset_id; /* Dataset ID (IN) */
        } flush;

        /* H5VL_DATASET_REFRESH */
        struct {
            hid_t dset_id; /* Dataset ID (IN) */
        } refresh;
    } args;
} H5VL_dataset_specific_args_t;

/* Typedef for VOL connector dataset optional VOL operations */
typedef int H5VL_dataset_optional_t;

/* Values for datatype 'get' operation */
typedef enum H5VL_datatype_get_t {
    H5VL_DATATYPE_GET_BINARY_SIZE, /* Get size of serialized form of transient type */
    H5VL_DATATYPE_GET_BINARY,      /* Get serialized form of transient type         */
    H5VL_DATATYPE_GET_TCPL         /* Datatype creation property list               */
} H5VL_datatype_get_t;

/* Parameters for datatype 'get' operations */
typedef struct H5VL_datatype_get_args_t {
    H5VL_datatype_get_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_DATATYPE_GET_BINARY_SIZE */
        struct {
            size_t *size; /* Size of serialized form of datatype (OUT) */
        } get_binary_size;

        /* H5VL_DATATYPE_GET_BINARY */
        struct {
            void  *buf;      /* Buffer to store serialized form of datatype (OUT) */
            size_t buf_size; /* Size of serialized datatype buffer */
        } get_binary;

        /* H5VL_DATATYPE_GET_TCPL */
        struct {
            hid_t tcpl_id; /* Named datatype creation property list ID (OUT) */
        } get_tcpl;
    } args;
} H5VL_datatype_get_args_t;

/* Values for datatype 'specific' operation */
typedef enum H5VL_datatype_specific_t {
    H5VL_DATATYPE_FLUSH,  /* H5Tflush */
    H5VL_DATATYPE_REFRESH /* H5Trefresh */
} H5VL_datatype_specific_t;

/* Parameters for datatype 'specific' operations */
typedef struct H5VL_datatype_specific_args_t {
    H5VL_datatype_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_DATATYPE_FLUSH */
        struct {
            hid_t type_id; /* Named datatype ID (IN) */
        } flush;

        /* H5VL_DATATYPE_REFRESH */
        struct {
            hid_t type_id; /* Named datatype ID (IN) */
        } refresh;
    } args;
} H5VL_datatype_specific_args_t;

/* Typedef and values for native VOL connector named datatype optional VOL operations */
typedef int H5VL_datatype_optional_t;
/* (No optional named datatype VOL operations currently) */

/* Info for H5VL_FILE_GET_CONT_INFO */
typedef struct H5VL_file_cont_info_t {
    unsigned version;       /* version information (keep first) */
    uint64_t feature_flags; /* Container feature flags          */
                            /* (none currently defined)         */
    size_t token_size;      /* Size of tokens                   */
    size_t blob_id_size;    /* Size of blob IDs                 */
} H5VL_file_cont_info_t;

/* Values for file 'get' operation */
typedef enum H5VL_file_get_t {
    H5VL_FILE_GET_CONT_INFO, /* file get container info              */
    H5VL_FILE_GET_FAPL,      /* file access property list            */
    H5VL_FILE_GET_FCPL,      /* file creation property list          */
    H5VL_FILE_GET_FILENO,    /* file number                          */
    H5VL_FILE_GET_INTENT,    /* file intent                          */
    H5VL_FILE_GET_NAME,      /* file name                            */
    H5VL_FILE_GET_OBJ_COUNT, /* object count in file                 */
    H5VL_FILE_GET_OBJ_IDS    /* object ids in file                   */
} H5VL_file_get_t;

/* Parameters for file 'get_name' operation */
typedef struct H5VL_file_get_name_args_t {
    H5I_type_t type;          /* ID type of object pointer */
    size_t     buf_size;      /* Size of file name buffer (IN) */
    char      *buf;           /* Buffer for file name (OUT) */
    size_t    *file_name_len; /* Actual length of file name (OUT) */
} H5VL_file_get_name_args_t;

/* Parameters for file 'get_obj_ids' operation */
typedef struct H5VL_file_get_obj_ids_args_t {
    unsigned types;    /* Type of objects to count */
    size_t   max_objs; /* Size of array of object IDs */
    hid_t   *oid_list; /* Array of object IDs (OUT) */
    size_t  *count;    /* # of objects (OUT) */
} H5VL_file_get_obj_ids_args_t;

/* Parameters for file 'get' operations */
typedef struct H5VL_file_get_args_t {
    H5VL_file_get_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_FILE_GET_CONT_INFO */
        struct {
            H5VL_file_cont_info_t *info; /* Container info (OUT) */
        } get_cont_info;

        /* H5VL_FILE_GET_FAPL */
        struct {
            hid_t fapl_id; /* File access property list (OUT) */
        } get_fapl;

        /* H5VL_FILE_GET_FCPL */
        struct {
            hid_t fcpl_id; /* File creation property list (OUT) */
        } get_fcpl;

        /* H5VL_FILE_GET_FILENO */
        struct {
            unsigned long *fileno; /* File "number" (OUT) */
        } get_fileno;

        /* H5VL_FILE_GET_INTENT */
        struct {
            unsigned *flags; /* File open/create intent flags (OUT) */
        } get_intent;

        /* H5VL_FILE_GET_NAME */
        H5VL_file_get_name_args_t get_name;

        /* H5VL_FILE_GET_OBJ_COUNT */
        struct {
            unsigned types; /* Type of objects to count */
            size_t  *count; /* # of objects (OUT) */
        } get_obj_count;

        /* H5VL_FILE_GET_OBJ_IDS */
        H5VL_file_get_obj_ids_args_t get_obj_ids;
    } args;
} H5VL_file_get_args_t;

/* Values for file 'specific' operation */
typedef enum H5VL_file_specific_t {
    H5VL_FILE_FLUSH,         /* Flush file                       */
    H5VL_FILE_REOPEN,        /* Reopen the file                  */
    H5VL_FILE_IS_ACCESSIBLE, /* Check if a file is accessible    */
                             /* Note for VOL connector authors:  */
                             /*  Only return an error from this  */
                             /*  callback if it is not possible  */
                             /*  to determine if a file (or      */
                             /*  container) is accessible.       */
                             /*  It is _not_ an error to return  */
                             /*  a 'false' value for the         */
                             /*  'accessible' argument.          */
    H5VL_FILE_DELETE,        /* Delete a file                    */
    H5VL_FILE_IS_EQUAL       /* Check if two files are the same  */
} H5VL_file_specific_t;

/* Parameters for file 'specific' operations */
typedef struct H5VL_file_specific_args_t {
    H5VL_file_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_FILE_FLUSH */
        struct {
            H5I_type_t  obj_type; /* Type of object to use */
            H5F_scope_t scope;    /* Scope of flush operation */
        } flush;

        /* H5VL_FILE_REOPEN */
        struct {
            void **file; /* File object for new file (OUT) */
        } reopen;

        /* H5VL_FILE_IS_ACCESSIBLE */
        struct {
            const char *filename;   /* Name of file to check */
            hid_t       fapl_id;    /* File access property list to use */
            hbool_t    *accessible; /* Whether file is accessible with FAPL settings (OUT) */
        } is_accessible;

        /* H5VL_FILE_DELETE */
        struct {
            const char *filename; /* Name of file to delete */
            hid_t       fapl_id;  /* File access property list to use */
        } del;

        /* H5VL_FILE_IS_EQUAL */
        struct {
            void    *obj2;      /* Second file object to compare against */
            hbool_t *same_file; /* Whether files are the same (OUT) */
        } is_equal;
    } args;
} H5VL_file_specific_args_t;

/* Typedef for VOL connector file optional VOL operations */
typedef int H5VL_file_optional_t;

/* Values for group 'get' operation */
typedef enum H5VL_group_get_t {
    H5VL_GROUP_GET_GCPL, /* group creation property list     */
    H5VL_GROUP_GET_INFO  /* group info                       */
} H5VL_group_get_t;

/* Parameters for group 'get_info' operation */
typedef struct H5VL_group_get_info_args_t {
    H5VL_loc_params_t loc_params; /* Location parameters for object access */
    H5G_info_t       *ginfo;      /* Group info (OUT) */
} H5VL_group_get_info_args_t;

/* Parameters for group 'get' operations */
typedef struct H5VL_group_get_args_t {
    H5VL_group_get_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_GROUP_GET_GCPL */
        struct {
            hid_t gcpl_id; /* Group creation property list (OUT) */
        } get_gcpl;

        /* H5VL_GROUP_GET_INFO */
        H5VL_group_get_info_args_t get_info; /* Group info */
    } args;
} H5VL_group_get_args_t;

/* Values for group 'specific' operation */
typedef enum H5VL_group_specific_t {
    H5VL_GROUP_MOUNT,   /* Mount a file on a group          */
    H5VL_GROUP_UNMOUNT, /* Unmount a file on a group        */
    H5VL_GROUP_FLUSH,   /* H5Gflush                         */
    H5VL_GROUP_REFRESH  /* H5Grefresh                       */
} H5VL_group_specific_t;

/* Parameters for group 'mount' operation */
typedef struct H5VL_group_spec_mount_args_t {
    const char *name;       /* Name of location to mount child file */
    void       *child_file; /* Pointer to child file object */
    hid_t       fmpl_id;    /* File mount property list to use */
} H5VL_group_spec_mount_args_t;

/* Parameters for group 'specific' operations */
typedef struct H5VL_group_specific_args_t {
    H5VL_group_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_GROUP_MOUNT */
        H5VL_group_spec_mount_args_t mount;

        /* H5VL_GROUP_UNMOUNT */
        struct {
            const char *name; /* Name of location to unmount child file */
        } unmount;

        /* H5VL_GROUP_FLUSH */
        struct {
            hid_t grp_id; /* Group ID (IN) */
        } flush;

        /* H5VL_GROUP_REFRESH */
        struct {
            hid_t grp_id; /* Group ID (IN) */
        } refresh;
    } args;
} H5VL_group_specific_args_t;

/* Typedef for VOL connector group optional VOL operations */
typedef int H5VL_group_optional_t;

/* Link create types for VOL */
typedef enum H5VL_link_create_t {
    H5VL_LINK_CREATE_HARD,
    H5VL_LINK_CREATE_SOFT,
    H5VL_LINK_CREATE_UD
} H5VL_link_create_t;

/* Parameters for link 'create' operations */
typedef struct H5VL_link_create_args_t {
    H5VL_link_create_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_LINK_CREATE_HARD */
        struct {
            void             *curr_obj;        /* Current object */
            H5VL_loc_params_t curr_loc_params; /* Location parameters for current object */
        } hard;

        /* H5VL_LINK_CREATE_SOFT */
        struct {
            const char *target; /* Target of soft link */
        } soft;

        /* H5VL_LINK_CREATE_UD */
        struct {
            H5L_type_t  type;     /* Type of link to create */
            const void *buf;      /* Buffer that contains link info */
            size_t      buf_size; /* Size of link info buffer */
        } ud;
    } args;
} H5VL_link_create_args_t;

/* Values for link 'get' operation */
typedef enum H5VL_link_get_t {
    H5VL_LINK_GET_INFO, /* link info                         */
    H5VL_LINK_GET_NAME, /* link name                         */
    H5VL_LINK_GET_VAL   /* link value                        */
} H5VL_link_get_t;

/* Parameters for link 'get' operations */
typedef struct H5VL_link_get_args_t {
    H5VL_link_get_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_LINK_GET_INFO */
        struct {
            H5L_info2_t *linfo; /* Pointer to link's info (OUT) */
        } get_info;

        /* H5VL_LINK_GET_NAME */
        struct {
            size_t  name_size; /* Size of link name buffer (IN) */
            char   *name;      /* Buffer for link name (OUT) */
            size_t *name_len;  /* Actual length of link name (OUT) */
        } get_name;

        /* H5VL_LINK_GET_VAL */
        struct {
            size_t buf_size; /* Size of link value buffer (IN) */
            void  *buf;      /* Buffer for link value (OUT) */
        } get_val;
    } args;
} H5VL_link_get_args_t;

/* Values for link 'specific' operation */
typedef enum H5VL_link_specific_t {
    H5VL_LINK_DELETE, /* H5Ldelete(_by_idx)                */
    H5VL_LINK_EXISTS, /* link existence                    */
    H5VL_LINK_ITER    /* H5Literate/visit(_by_name)              */
} H5VL_link_specific_t;

/* Parameters for link 'iterate' operation */
typedef struct H5VL_link_iterate_args_t {
    hbool_t         recursive; /* Whether iteration is recursive */
    H5_index_t      idx_type;  /* Type of index to iterate over */
    H5_iter_order_t order;     /* Order of index iteration */
    hsize_t        *idx_p;     /* Start/stop iteration index (OUT) */
    H5L_iterate2_t  op;        /* Iteration callback function */
    void           *op_data;   /* Iteration callback context */
} H5VL_link_iterate_args_t;

/* Parameters for link 'specific' operations */
typedef struct H5VL_link_specific_args_t {
    H5VL_link_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_LINK_DELETE */
        /* No args */

        /* H5VL_LINK_EXISTS */
        struct {
            hbool_t *exists; /* Whether link exists (OUT) */
        } exists;

        /* H5VL_LINK_ITER */
        H5VL_link_iterate_args_t iterate;
    } args;
} H5VL_link_specific_args_t;

/* Typedef and values for native VOL connector link optional VOL operations */
typedef int H5VL_link_optional_t;
/* (No optional link VOL operations currently) */

/* Values for object 'get' operation */
typedef enum H5VL_object_get_t {
    H5VL_OBJECT_GET_FILE, /* object file                       */
    H5VL_OBJECT_GET_NAME, /* object name                       */
    H5VL_OBJECT_GET_TYPE, /* object type                       */
    H5VL_OBJECT_GET_INFO  /* H5Oget_info(_by_idx|name)         */
} H5VL_object_get_t;

/* Parameters for object 'get' operations */
typedef struct H5VL_object_get_args_t {
    H5VL_object_get_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_OBJECT_GET_FILE */
        struct {
            void **file; /* File object (OUT) */
        } get_file;

        /* H5VL_OBJECT_GET_NAME */
        struct {
            size_t  buf_size; /* Size of name buffer (IN) */
            char   *buf;      /* Buffer for name (OUT) */
            size_t *name_len; /* Actual length of name (OUT) */
        } get_name;

        /* H5VL_OBJECT_GET_TYPE */
        struct {
            H5O_type_t *obj_type; /* Type of object (OUT) */
        } get_type;

        /* H5VL_OBJECT_GET_INFO */
        struct {
            unsigned     fields; /* Flags for fields to retrieve */
            H5O_info2_t *oinfo;  /* Pointer to object info (OUT) */
        } get_info;
    } args;
} H5VL_object_get_args_t;

/* Values for object 'specific' operation */
typedef enum H5VL_object_specific_t {
    H5VL_OBJECT_CHANGE_REF_COUNT, /* H5Oincr/decr_refcount             */
    H5VL_OBJECT_EXISTS,           /* H5Oexists_by_name                 */
    H5VL_OBJECT_LOOKUP,           /* Lookup object                     */
    H5VL_OBJECT_VISIT,            /* H5Ovisit(_by_name)                */
    H5VL_OBJECT_FLUSH,            /* H5{D|G|O|T}flush                  */
    H5VL_OBJECT_REFRESH           /* H5{D|G|O|T}refresh                */
} H5VL_object_specific_t;

/* Parameters for object 'visit' operation */
typedef struct H5VL_object_visit_args_t {
    H5_index_t      idx_type; /* Type of index to iterate over */
    H5_iter_order_t order;    /* Order of index iteration */
    unsigned        fields;   /* Flags for fields to provide in 'info' object for 'op' callback */
    H5O_iterate2_t  op;       /* Iteration callback function */
    void           *op_data;  /* Iteration callback context */
} H5VL_object_visit_args_t;

/* Parameters for object 'specific' operations */
typedef struct H5VL_object_specific_args_t {
    H5VL_object_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_OBJECT_CHANGE_REF_COUNT */
        struct {
            int delta; /* Amount to modify object's refcount */
        } change_rc;

        /* H5VL_OBJECT_EXISTS */
        struct {
            hbool_t *exists; /* Whether object exists (OUT) */
        } exists;

        /* H5VL_OBJECT_LOOKUP */
        struct {
            H5O_token_t *token_ptr; /* Pointer to token for lookup (OUT) */
        } lookup;

        /* H5VL_OBJECT_VISIT */
        H5VL_object_visit_args_t visit;

        /* H5VL_OBJECT_FLUSH */
        struct {
            hid_t obj_id; /* Object ID (IN) */
        } flush;

        /* H5VL_OBJECT_REFRESH */
        struct {
            hid_t obj_id; /* Object ID (IN) */
        } refresh;
    } args;
} H5VL_object_specific_args_t;

/* Typedef for VOL connector object optional VOL operations */
typedef int H5VL_object_optional_t;

/* Status values for async request operations */
typedef enum H5VL_request_status_t {
    H5VL_REQUEST_STATUS_IN_PROGRESS, /* Operation has not yet completed                       */
    H5VL_REQUEST_STATUS_SUCCEED,     /* Operation has completed, successfully                 */
    H5VL_REQUEST_STATUS_FAIL,        /* Operation has completed, but failed                   */
    H5VL_REQUEST_STATUS_CANT_CANCEL, /* An attempt to cancel this operation was made, but it  */
                                     /*  can't be canceled immediately.  The operation has    */
                                     /*  not completed successfully or failed, and is not yet */
                                     /*  in progress.  Another attempt to cancel it may be    */
                                     /*  attempted and may (or may not) succeed.              */
    H5VL_REQUEST_STATUS_CANCELED     /* Operation has not completed and was canceled          */
} H5VL_request_status_t;

/* Values for async request 'specific' operation */
typedef enum H5VL_request_specific_t {
    H5VL_REQUEST_GET_ERR_STACK, /* Retrieve error stack for failed operation */
    H5VL_REQUEST_GET_EXEC_TIME  /* Retrieve execution time for operation */
} H5VL_request_specific_t;

/* Parameters for request 'specific' operations */
typedef struct H5VL_request_specific_args_t {
    H5VL_request_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_REQUEST_GET_ERR_STACK */
        struct {
            hid_t err_stack_id; /* Error stack ID for operation (OUT) */
        } get_err_stack;

        /* H5VL_REQUEST_GET_EXEC_TIME */
        struct {
            uint64_t *exec_ts;   /* Timestamp for start of task execution (OUT) */
            uint64_t *exec_time; /* Duration of task execution (in ns) (OUT) */
        } get_exec_time;
    } args;
} H5VL_request_specific_args_t;

/* Typedef and values for native VOL connector request optional VOL operations */
typedef int H5VL_request_optional_t;
/* (No optional request VOL operations currently) */

/* Values for 'blob' 'specific' operation */
typedef enum H5VL_blob_specific_t {
    H5VL_BLOB_DELETE, /* Delete a blob (by ID) */
    H5VL_BLOB_ISNULL, /* Check if a blob ID is "null" */
    H5VL_BLOB_SETNULL /* Set a blob ID to the connector's "null" blob ID value */
} H5VL_blob_specific_t;

/* Parameters for blob 'specific' operations */
typedef struct H5VL_blob_specific_args_t {
    H5VL_blob_specific_t op_type; /* Operation to perform */

    /* Parameters for each operation */
    union {
        /* H5VL_BLOB_DELETE */
        /* No args */

        /* H5VL_BLOB_ISNULL */
        struct {
            hbool_t *isnull; /* Whether blob ID is "null" (OUT) */
        } is_null;

        /* H5VL_BLOB_SETNULL */
        /* No args */
    } args;
} H5VL_blob_specific_args_t;

/* Typedef and values for native VOL connector blob optional VOL operations */
typedef int H5VL_blob_optional_t;
/* (No optional blob VOL operations currently) */

/* VOL connector info fields & callbacks */
typedef struct H5VL_info_class_t {
    size_t size;                     /* Size of the VOL info                         */
    void *(*copy)(const void *info); /* Callback to create a copy of the VOL info    */
    herr_t (*cmp)(int *cmp_value, const void *info1, const void *info2); /* Callback to compare VOL info */
    herr_t (*free)(void *info);                     /* Callback to release a VOL info               */
    herr_t (*to_str)(const void *info, char **str); /* Callback to serialize connector's info into a string */
    herr_t (*from_str)(const char *str,
                       void      **info); /* Callback to deserialize a string into connector's info */
} H5VL_info_class_t;

/* VOL object wrap / retrieval callbacks */
/* (These must be implemented by "pass through" VOL connectors, and should not be implemented by terminal VOL
 * connectors) */
typedef struct H5VL_wrap_class_t {
    void *(*get_object)(const void *obj); /* Callback to retrieve underlying object       */
    herr_t (*get_wrap_ctx)(
        const void *obj,
        void      **wrap_ctx); /* Callback to retrieve the object wrapping context for the connector */
    void *(*wrap_object)(void *obj, H5I_type_t obj_type,
                         void *wrap_ctx); /* Callback to wrap a library object */
    void *(*unwrap_object)(void *obj);    /* Callback to unwrap a library object */
    herr_t (*free_wrap_ctx)(
        void *wrap_ctx); /* Callback to release the object wrapping context for the connector */
} H5VL_wrap_class_t;

/* H5A routines */
typedef struct H5VL_attr_class_t {
    void *(*create)(void *obj, const H5VL_loc_params_t *loc_params, const char *attr_name, hid_t type_id,
                    hid_t space_id, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req);
    void *(*open)(void *obj, const H5VL_loc_params_t *loc_params, const char *attr_name, hid_t aapl_id,
                  hid_t dxpl_id, void **req);
    herr_t (*read)(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req);
    herr_t (*write)(void *attr, hid_t mem_type_id, const void *buf, hid_t dxpl_id, void **req);
    herr_t (*get)(void *obj, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*specific)(void *obj, const H5VL_loc_params_t *loc_params, H5VL_attr_specific_args_t *args,
                       hid_t dxpl_id, void **req);
    herr_t (*optional)(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*close)(void *attr, hid_t dxpl_id, void **req);
} H5VL_attr_class_t;

/* H5D routines */
typedef struct H5VL_dataset_class_t {
    void *(*create)(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t lcpl_id,
                    hid_t type_id, hid_t space_id, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req);
    void *(*open)(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t dapl_id,
                  hid_t dxpl_id, void **req);
    herr_t (*read)(size_t count, void *dset[], hid_t mem_type_id[], hid_t mem_space_id[],
                   hid_t file_space_id[], hid_t dxpl_id, void *buf[], void **req);
    herr_t (*write)(size_t count, void *dset[], hid_t mem_type_id[], hid_t mem_space_id[],
                    hid_t file_space_id[], hid_t dxpl_id, const void *buf[], void **req);
    herr_t (*get)(void *obj, H5VL_dataset_get_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*specific)(void *obj, H5VL_dataset_specific_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*optional)(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*close)(void *dset, hid_t dxpl_id, void **req);
} H5VL_dataset_class_t;

/* H5T routines*/
typedef struct H5VL_datatype_class_t {
    void *(*commit)(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t type_id,
                    hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id, void **req);
    void *(*open)(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t tapl_id,
                  hid_t dxpl_id, void **req);
    herr_t (*get)(void *obj, H5VL_datatype_get_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*specific)(void *obj, H5VL_datatype_specific_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*optional)(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*close)(void *dt, hid_t dxpl_id, void **req);
} H5VL_datatype_class_t;

/* H5F routines */
typedef struct H5VL_file_class_t {
    void *(*create)(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id,
                    void **req);
    void *(*open)(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
    herr_t (*get)(void *obj, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*specific)(void *obj, H5VL_file_specific_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*optional)(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*close)(void *file, hid_t dxpl_id, void **req);
} H5VL_file_class_t;

/* H5G routines */
typedef struct H5VL_group_class_t {
    void *(*create)(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t lcpl_id,
                    hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req);
    void *(*open)(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t gapl_id,
                  hid_t dxpl_id, void **req);
    herr_t (*get)(void *obj, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*specific)(void *obj, H5VL_group_specific_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*optional)(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
    herr_t (*close)(void *grp, hid_t dxpl_id, void **req);
} H5VL_group_class_t;

/* H5L routines */
typedef struct H5VL_link_class_t {
    herr_t (*create)(H5VL_link_create_args_t *args, void *obj, const H5VL_loc_params_t *loc_params,
                     hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
    herr_t (*copy)(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                   const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id,
                   void **req);
    herr_t (*move)(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                   const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id,
                   void **req);
    herr_t (*get)(void *obj, const H5VL_loc_params_t *loc_params, H5VL_link_get_args_t *args, hid_t dxpl_id,
                  void **req);
    herr_t (*specific)(void *obj, const H5VL_loc_params_t *loc_params, H5VL_link_specific_args_t *args,
                       hid_t dxpl_id, void **req);
    herr_t (*optional)(void *obj, const H5VL_loc_params_t *loc_params, H5VL_optional_args_t *args,
                       hid_t dxpl_id, void **req);
} H5VL_link_class_t;

/* H5O routines */
typedef struct H5VL_object_class_t {
    void *(*open)(void *obj, const H5VL_loc_params_t *loc_params, H5I_type_t *opened_type, hid_t dxpl_id,
                  void **req);
    herr_t (*copy)(void *src_obj, const H5VL_loc_params_t *loc_params1, const char *src_name, void *dst_obj,
                   const H5VL_loc_params_t *loc_params2, const char *dst_name, hid_t ocpypl_id, hid_t lcpl_id,
                   hid_t dxpl_id, void **req);
    herr_t (*get)(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_get_args_t *args, hid_t dxpl_id,
                  void **req);
    herr_t (*specific)(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_specific_args_t *args,
                       hid_t dxpl_id, void **req);
    herr_t (*optional)(void *obj, const H5VL_loc_params_t *loc_params, H5VL_optional_args_t *args,
                       hid_t dxpl_id, void **req);
} H5VL_object_class_t;

/* Asynchronous request 'notify' callback */
typedef herr_t (*H5VL_request_notify_t)(void *ctx, H5VL_request_status_t status);

/* "Levels" for 'get connector class' introspection callback */
typedef enum H5VL_get_conn_lvl_t {
    H5VL_GET_CONN_LVL_CURR, /* Get "current" connector (for this object) */
    H5VL_GET_CONN_LVL_TERM  /* Get "terminal" connector (for this object) */
                            /* (Recursively called, for pass-through connectors) */
                            /* (Connectors that "split" must choose which connector to return) */
} H5VL_get_conn_lvl_t;

/* Forward declaration of H5VL_class_t, defined later in this file */
struct H5VL_class_t;

/* Container/connector introspection routines */
typedef struct H5VL_introspect_class_t {
    herr_t (*get_conn_cls)(void *obj, H5VL_get_conn_lvl_t lvl, const struct H5VL_class_t **conn_cls);
    herr_t (*get_cap_flags)(const void *info, uint64_t *cap_flags);
    herr_t (*opt_query)(void *obj, H5VL_subclass_t cls, int opt_type, uint64_t *flags);
} H5VL_introspect_class_t;

/* Async request operation routines */
typedef struct H5VL_request_class_t {
    herr_t (*wait)(void *req, uint64_t timeout, H5VL_request_status_t *status);
    herr_t (*notify)(void *req, H5VL_request_notify_t cb, void *ctx);
    herr_t (*cancel)(void *req, H5VL_request_status_t *status);
    herr_t (*specific)(void *req, H5VL_request_specific_args_t *args);
    herr_t (*optional)(void *req, H5VL_optional_args_t *args);
    herr_t (*free)(void *req);
} H5VL_request_class_t;

/* 'blob' routines */
typedef struct H5VL_blob_class_t {
    herr_t (*put)(void *obj, const void *buf, size_t size, void *blob_id, void *ctx);
    herr_t (*get)(void *obj, const void *blob_id, void *buf, size_t size, void *ctx);
    herr_t (*specific)(void *obj, void *blob_id, H5VL_blob_specific_args_t *args);
    herr_t (*optional)(void *obj, void *blob_id, H5VL_optional_args_t *args);
} H5VL_blob_class_t;

/* Object token routines */
typedef struct H5VL_token_class_t {
    herr_t (*cmp)(void *obj, const H5O_token_t *token1, const H5O_token_t *token2, int *cmp_value);
    herr_t (*to_str)(void *obj, H5I_type_t obj_type, const H5O_token_t *token, char **token_str);
    herr_t (*from_str)(void *obj, H5I_type_t obj_type, const char *token_str, H5O_token_t *token);
} H5VL_token_class_t;

/**
 * \ingroup H5VLDEV
 * Class information for each VOL connector
 */
//! <!-- [H5VL_class_t_snip] -->
typedef struct H5VL_class_t {
    /* Overall connector fields & callbacks */
    unsigned           version;          /**< VOL connector class struct version number */
    H5VL_class_value_t value;            /**< Value to identify connector               */
    const char        *name;             /**< Connector name (MUST be unique!)          */
    unsigned           conn_version;     /**< Version number of connector               */
    uint64_t           cap_flags;        /**< Capability flags for connector            */
    herr_t (*initialize)(hid_t vipl_id); /**< Connector initialization callback         */
    herr_t (*terminate)(void);           /**< Connector termination callback            */

    /* VOL framework */
    H5VL_info_class_t info_cls; /**< VOL info fields & callbacks  */
    H5VL_wrap_class_t wrap_cls; /**< VOL object wrap / retrieval callbacks */

    /* Data Model */
    H5VL_attr_class_t     attr_cls;     /**< Attribute (H5A*) class callbacks */
    H5VL_dataset_class_t  dataset_cls;  /**< Dataset (H5D*) class callbacks   */
    H5VL_datatype_class_t datatype_cls; /**< Datatype (H5T*) class callbacks  */
    H5VL_file_class_t     file_cls;     /**< File (H5F*) class callbacks      */
    H5VL_group_class_t    group_cls;    /**< Group (H5G*) class callbacks     */
    H5VL_link_class_t     link_cls;     /**< Link (H5L*) class callbacks      */
    H5VL_object_class_t   object_cls;   /**< Object (H5O*) class callbacks    */

    /* Infrastructure / Services */
    H5VL_introspect_class_t introspect_cls; /**< Container/connector introspection class callbacks */
    H5VL_request_class_t    request_cls;    /**< Asynchronous request class callbacks */
    H5VL_blob_class_t       blob_cls;       /**< 'Blob' class callbacks */
    H5VL_token_class_t      token_cls;      /**< VOL connector object token class callbacks */

    /* Catch-all */
    herr_t (*optional)(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id,
                       void **req); /**< Optional callback */
} H5VL_class_t;
//! <!-- [H5VL_class_t_snip] -->

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
/**
 * \ingroup H5VLDEV
 * \brief Registers a new VOL connector
 *
 * \param[in] cls A pointer to the plugin structure to register
 * \vipl_id
 * \return \hid_t{VOL connector}
 *
 * \details H5VLregister_connector() registers a new VOL connector as a member
 *          of the virtual object layer class. This VOL connector identifier is
 *          good until the library is closed or the connector is unregistered.
 *
 *          \p vipl_id is either #H5P_DEFAULT or the identifier of a VOL
 *          initialization property list of class #H5P_VOL_INITIALIZE created
 *          with H5Pcreate(). When created, this property list contains no
 *          library properties. If a VOL connector author decides that
 *          initialization-specific data are needed, they can be added to the
 *          empty list and retrieved by the connector in the VOL connector's
 *          initialize callback. Use of the VOL initialization property list is
 *          uncommon, as most VOL-specific properties are added to the file
 *          access property list via the connector's API calls which set the
 *          VOL connector for the file open/create. For more information, see
 *          the \ref_vol_doc.
 *
 *          H5VL_class_t is defined in H5VLconnector.h in the source code. It
 *          contains class information for each VOL connector:
 *          \snippet this H5VL_class_t_snip
 *
 * \since 1.12.0
 *
 */
H5_DLL hid_t H5VLregister_connector(const H5VL_class_t *cls, hid_t vipl_id);
/**
 * \ingroup H5VLDEV
 */
H5_DLL void *H5VLobject(hid_t obj_id);
/**
 * \ingroup H5VLDEV
 */
H5_DLL hid_t H5VLget_file_type(void *file_obj, hid_t connector_id, hid_t dtype_id);
/**
 * \ingroup H5VLDEV
 */
H5_DLL hid_t H5VLpeek_connector_id_by_name(const char *name);
/**
 * \ingroup H5VLDEV
 */
H5_DLL hid_t H5VLpeek_connector_id_by_value(H5VL_class_value_t value);

/* User-defined optional operations */
H5_DLL herr_t H5VLregister_opt_operation(H5VL_subclass_t subcls, const char *op_name, int *op_val);
H5_DLL herr_t H5VLfind_opt_operation(H5VL_subclass_t subcls, const char *op_name, int *op_val);
H5_DLL herr_t H5VLunregister_opt_operation(H5VL_subclass_t subcls, const char *op_name);
H5_DLL herr_t H5VLattr_optional_op(const char *app_file, const char *app_func, unsigned app_line,
                                   hid_t attr_id, H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id);
H5_DLL herr_t H5VLdataset_optional_op(const char *app_file, const char *app_func, unsigned app_line,
                                      hid_t dset_id, H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id);
H5_DLL herr_t H5VLdatatype_optional_op(const char *app_file, const char *app_func, unsigned app_line,
                                       hid_t type_id, H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id);
H5_DLL herr_t H5VLfile_optional_op(const char *app_file, const char *app_func, unsigned app_line,
                                   hid_t file_id, H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id);
H5_DLL herr_t H5VLgroup_optional_op(const char *app_file, const char *app_func, unsigned app_line,
                                    hid_t group_id, H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id);
H5_DLL herr_t H5VLlink_optional_op(const char *app_file, const char *app_func, unsigned app_line,
                                   hid_t loc_id, const char *name, hid_t lapl_id, H5VL_optional_args_t *args,
                                   hid_t dxpl_id, hid_t es_id);
H5_DLL herr_t H5VLobject_optional_op(const char *app_file, const char *app_func, unsigned app_line,
                                     hid_t loc_id, const char *name, hid_t lapl_id,
                                     H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id);
H5_DLL herr_t H5VLrequest_optional_op(void *req, hid_t connector_id, H5VL_optional_args_t *args);

/* API Wrappers for "optional_op" routines */
/* (Must be defined _after_ the function prototype) */
/* (And must only defined when included in application code, not the library) */
#ifndef H5VL_MODULE
/* Inject application compile-time macros into function calls */
#define H5VLattr_optional_op(...)     H5VLattr_optional_op(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5VLdataset_optional_op(...)  H5VLdataset_optional_op(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5VLdatatype_optional_op(...) H5VLdatatype_optional_op(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5VLfile_optional_op(...)     H5VLfile_optional_op(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5VLgroup_optional_op(...)    H5VLgroup_optional_op(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5VLlink_optional_op(...)     H5VLlink_optional_op(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5VLobject_optional_op(...)   H5VLobject_optional_op(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Define "wrapper" versions of function calls, to allow compile-time values to
 *      be passed in by language wrapper or library layer on top of HDF5.
 */
#define H5VLattr_optional_op_wrap     H5_NO_EXPAND(H5VLattr_optional_op)
#define H5VLdataset_optional_op_wrap  H5_NO_EXPAND(H5VLdataset_optional_op)
#define H5VLdatatype_optional_op_wrap H5_NO_EXPAND(H5VLdatatype_optional_op)
#define H5VLfile_optional_op_wrap     H5_NO_EXPAND(H5VLfile_optional_op)
#define H5VLgroup_optional_op_wrap    H5_NO_EXPAND(H5VLgroup_optional_op)
#define H5VLlink_optional_op_wrap     H5_NO_EXPAND(H5VLlink_optional_op)
#define H5VLobject_optional_op_wrap   H5_NO_EXPAND(H5VLobject_optional_op)
#endif /* H5VL_MODULE */

#ifdef __cplusplus
}
#endif

#endif /* H5VLconnector_H */
