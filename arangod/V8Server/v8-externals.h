////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_V8_SERVER_V8_EXTERNALS_H
#define ARANGOD_V8_SERVER_V8_EXTERNALS_H 1

#include "Basics/Common.h"

/// @brief wrapped class for TRI_vocbase_t
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
static int32_t const WRP_VOCBASE_TYPE = 1;

/// @brief wrapped class for LogicalCollection
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
static int32_t const WRP_VOCBASE_COL_TYPE = 2;

/// @brief wrapped class for LogicalView
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
static int32_t const WRP_VOCBASE_VIEW_TYPE = 3;

/// @brief wrapped class for IResearch Analyzer
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
static int32_t const WRP_IRESEARCH_ANALYZER_TYPE = 4;

#endif