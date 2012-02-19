////////////////////////////////////////////////////////////////////////////////
/// @brief where conditions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_WHERE_H
#define TRIAGENS_DURHAM_VOC_BASE_WHERE_H 1

#include "VocBase/vocbase.h"
#include "VocBase/document-collection.h"
#include "VocBase/index.h"
#include "VocBase/context.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief where clause type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QRY_WHERE_BOOLEAN,
  TRI_QRY_WHERE_GENERAL,
  TRI_QRY_WHERE_HASH_CONSTANT,
  TRI_QRY_WHERE_WITHIN_CONSTANT,
  TRI_QRY_WHERE_PRIMARY_CONSTANT
}
TRI_qry_where_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_s {
  TRI_qry_where_type_e _type;

  struct TRI_qry_where_s* (*clone) (struct TRI_qry_where_s const*);
  void (*free) (struct TRI_qry_where_s*);
}
TRI_qry_where_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract where clause for conditions
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_cond_s {
  TRI_qry_where_t base;
}
TRI_qry_where_cond_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief constant where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_boolean_s {
  TRI_qry_where_cond_t base;

  bool _value;
}
TRI_qry_where_boolean_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_general_s {
  TRI_qry_where_cond_t base;

  char* _code;
}
TRI_qry_where_general_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract primary index where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_primary_s {
  TRI_qry_where_t base;

  TRI_voc_did_t (*did) (struct TRI_qry_where_primary_s*, TRI_rc_context_t*);
}
TRI_qry_where_primary_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief primary index where clause with a constant document identifier
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_primary_const_s {
  TRI_qry_where_primary_t base;

  TRI_voc_did_t _did;
}
TRI_qry_where_primary_const_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_within_s {
  TRI_qry_where_t base;

  TRI_idx_iid_t _iid;
  char* _nameDistance;

  double* (*coordinates) (struct TRI_qry_where_within_s*, TRI_rc_context_t*);
  double (*radius) (struct TRI_qry_where_within_s*, TRI_rc_context_t*);
}
TRI_qry_where_within_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index where clause with a constant document identifier
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_within_const_s {
  TRI_qry_where_within_t base;

  double _coordinates[2];
  double _radius;
}
TRI_qry_where_within_const_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief hash index where
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_hash_const_s {
  TRI_qry_where_t base;
  TRI_idx_iid_t _iid;
  TRI_json_t* _parameters; // a json list object
}
TRI_qry_where_hash_const_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


