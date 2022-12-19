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

#include "Basics/TypeList.h"

namespace arangodb {
namespace application_features {

template<typename Features>
class ApplicationServerT;
class BasicFeaturePhaseClient;
class CommunicationFeaturePhase;
class GreetingsFeaturePhase;
}  // namespace application_features

class ClientFeature;
class ConfigFeature;
class FileSystemFeature;
class LoggerFeature;
class OptionsCheckFeature;
class RandomFeature;
class ShellColorsFeature;
class ShutdownFeature;
class SslFeature;
class VersionFeature;
class HttpEndpointProvider;
class ArangoGlobalContext;

using namespace application_features;

template<typename... T>
using ArangoClientFeaturesList = TypeList<
    // Phases
    CommunicationFeaturePhase, GreetingsFeaturePhase,
    // Features
    VersionFeature,  // VersionFeature must go first
    HttpEndpointProvider, ConfigFeature, FileSystemFeature, LoggerFeature,
    OptionsCheckFeature, RandomFeature, ShellColorsFeature, ShutdownFeature,
    SslFeature, T...>;

}  // namespace arangodb
