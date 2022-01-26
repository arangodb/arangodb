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
#include "Basics/TypeList.h"

namespace arangodb {
namespace application_features {

template<typename Features>
class ApplicationServerT;
class BasicFeaturePhaseClient;
class CommunicationFeaturePhase;
class GreetingsFeaturePhase;
class V8ShellFeaturePhase;
}  // namespace application_features

class ClientFeature;
class ConfigFeature;
class ShellConsoleFeature;
class LanguageFeature;
class LoggerFeature;
class RandomFeature;
class ShellColorsFeature;
class ShellFeature;
class ShutdownFeature;
class SslFeature;
class TempFeature;
class V8PlatformFeature;
class V8SecurityFeature;
class V8ShellFeature;
class VersionFeature;
class HttpEndpointProvider;
#ifdef USE_ENTERPRISE
class EncryptionFeature;
#endif

using namespace application_features;

using ArangoshFeatures = TypeList<
    // Phases
    BasicFeaturePhaseClient, CommunicationFeaturePhase, GreetingsFeaturePhase,
    V8ShellFeaturePhase,
    // Features
    HttpEndpointProvider, ConfigFeature, ShellConsoleFeature, LanguageFeature,
    LoggerFeature, RandomFeature, ShellColorsFeature, ShellFeature,
    ShutdownFeature, SslFeature, TempFeature, V8PlatformFeature,
    V8SecurityFeature, V8ShellFeature,
#ifdef USE_ENTERPRISE
    EncryptionFeature,
#endif
    VersionFeature>;

using ArangoshServer = ApplicationServerT<ArangoshFeatures>;
using ArangoshFeature = ApplicationFeatureT<ArangoshServer>;

}  // namespace arangodb
