////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "SslFeature.h"

#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/FileUtils.h"
#include "Basics/Thread.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/UniformCharacter.h"
#include "Ssl/ssl-helper.h"

#ifndef OPENSSL_THREADS
#error missing thread support for openssl, please recomple OpenSSL with threads
#endif

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

const asio_ns::ssl::detail::openssl_init<true> SslFeature::sslBase{};

SslFeature::SslFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Ssl") {
  setOptional(true);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void SslFeature::prepare() {}

void SslFeature::unprepare() {}

}  // namespace arangodb
