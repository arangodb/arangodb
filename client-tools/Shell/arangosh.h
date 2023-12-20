////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Utils/ArangoClient.h"

namespace arangodb {
namespace application_features {
class V8ShellFeaturePhase;
}

class ShellFeature;
class V8ShellFeature;
class V8SecurityFeature;
class V8PlatformFeature;
class LanguageFeature;
class FileSystemFeature;
class ShellConsoleFeature;
class TempFeature;
class EncryptionFeature;
class ProcessMonitoringFeature;
using ArangoshFeaturesList = TypeList<
    // Phases
    BasicFeaturePhaseClient, CommunicationFeaturePhase, GreetingsFeaturePhase,
    // Features
    VersionFeature,  // VersionFeature must go first
#ifdef USE_ENTERPRISE
    EncryptionFeature,
#endif
    ShellConsoleFeature, HttpEndpointProvider, ConfigFeature, LoggerFeature,
    FileSystemFeature, RandomFeature, ShellColorsFeature, ShutdownFeature,
    SslFeature, V8ShellFeaturePhase, ShellFeature, V8PlatformFeature,
    V8ShellFeature, LanguageFeature, V8SecurityFeature,
    ProcessMonitoringFeature, TempFeature>;
struct ArangoshFeatures : ArangoshFeaturesList {};
using ArangoshServer = ApplicationServerT<ArangoshFeatures>;
using ArangoshFeature = ApplicationFeatureT<ArangoshServer>;

}  // namespace arangodb
