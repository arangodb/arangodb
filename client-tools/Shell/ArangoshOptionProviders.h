////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/ConfigOptionsProvider.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "ApplicationFeatures/TempOptionsProvider.h"
#include "V8/V8PlatformOptionsProvider.h"
#include "V8/V8SecurityOptionsProvider.h"
#include "Shell/ClientOptionsProvider.h"
#include "Shell/ShellConsoleOptionsProvider.h"
#include "Shell/ShellOptionsProvider.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionOptionsProvider.h"
#endif

namespace arangodb {
using ArangoshOptionProviders =
    CoreOptionProviders<ClientOptionsProvider, ConfigOptionsProvider,
                        LanguageOptionsProvider, ShellConsoleOptionsProvider,
                        ShellOptionsProvider, TempOptionsProvider,
                        V8SecurityOptionsProvider, V8PlatformOptionsProvider
#ifdef USE_ENTERPRISE
                        ,
                        EncryptionOptionsProvider
#endif
                        >;
}  // namespace arangodb
