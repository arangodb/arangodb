////////////////////////////////////////////////////////////////////////////////
/// @brief markers
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_MARKER_H
#define TRIAGENS_VOC_BASE_MARKER_H 1

#include "BasicsC/common.h"
#include "VocBase/datafile.h"
#include "VocBase/voc-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name for a marker
////////////////////////////////////////////////////////////////////////////////

char* TRI_NameMarker (TRI_df_marker_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a marker
////////////////////////////////////////////////////////////////////////////////

void TRI_CloneMarker (TRI_df_marker_t*,
                      TRI_df_marker_t const*,
                      TRI_voc_size_t,
                      TRI_voc_size_t,
                      TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a marker with the most basic information
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMarker (TRI_df_marker_t*,
                     TRI_df_marker_type_e,
                     TRI_voc_size_t,
                     TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
