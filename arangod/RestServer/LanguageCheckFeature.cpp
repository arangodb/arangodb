////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "LanguageCheckFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/directories.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabasePathFeature.h"

namespace {
static std::string const FEATURE_NAME("LanguageCheck");

/// @brief reads previous default langauge from file
arangodb::Result readLanguage(arangodb::application_features::ApplicationServer& server,
                              std::string& language) {
  auto& databasePath = server.getFeature<arangodb::DatabasePathFeature>();
  std::string filename = databasePath.subdirectoryName("LANGUAGE");

  if (!TRI_ExistsFile(filename.c_str())) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  std::string found;
  try {
    VPackBuilder builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
    VPackSlice content = builder.slice();
    if (!content.isObject()) {
      return TRI_ERROR_INTERNAL;
    }
    VPackSlice defaultSlice = content.get("default");
    if (!defaultSlice.isString()) {
      return TRI_ERROR_INTERNAL;
    }
    found = defaultSlice.copyString();
  } catch (...) {
    // Nothing to free
    return TRI_ERROR_INTERNAL;
  }

  language = found;
  LOG_TOPIC("c499e", TRACE, arangodb::Logger::CONFIG) << "using default language: " << found;

  return TRI_ERROR_NO_ERROR;
}

/// @brief writes the default language to file
int writeLanguage(arangodb::application_features::ApplicationServer& server,
                  std::string const& language) {
  auto& databasePath = server.getFeature<arangodb::DatabasePathFeature>();
  std::string filename = databasePath.subdirectoryName("LANGUAGE");

  // generate json
  VPackBuilder builder;
  try {
    builder.openObject();
    builder.add("default", VPackValue(language));
    builder.close();
  } catch (...) {
    // out of memory
    LOG_TOPIC("4fa50", ERR, arangodb::Logger::CONFIG)
        << "cannot save default language in file '" << filename << "': out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // save json info to file
  LOG_TOPIC("08f3c", DEBUG, arangodb::Logger::CONFIG)
      << "Writing default language to file '" << filename << "'";
  bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(filename,
                                                                 builder.slice(), true);
  if (!ok) {
    LOG_TOPIC("c2fd7", ERR, arangodb::Logger::CONFIG)
        << "could not save default language in file '" << filename
        << "': " << TRI_last_error();
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

std::string getOrSetPreviousLanguage(arangodb::application_features::ApplicationServer& server,
                                     std::string const& input) {
  std::string language;
  arangodb::Result res = ::readLanguage(server, language);
  if (res.ok()) {
    return language;
  }

  // okay, we didn't find it, let's write out the input instead
  language = input;
  ::writeLanguage(server, language);

  return language;
}
}  // namespace

namespace arangodb {

LanguageCheckFeature::LanguageCheckFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, ::FEATURE_NAME) {
  setOptional(false);
  startsAfter<DatabasePathFeature>();
  startsAfter<LanguageFeature>();
}

LanguageCheckFeature::~LanguageCheckFeature() = default;

void LanguageCheckFeature::start() {
  auto& feature = server().getFeature<LanguageFeature>();
  auto defaultLang = feature.getDefaultLanguage();
  auto language = feature.getCollatorLanguage();
  auto previous = ::getOrSetPreviousLanguage(server(), language);

  if (defaultLang.empty() && !previous.empty()) {
    // override the empty current setting with the previous one
    feature.resetDefaultLanguage(previous);
    return;
  }

  if (language != previous) {
    // current not empty and not the same as previous, get out!
    LOG_TOPIC("7ef60", FATAL, arangodb::Logger::CONFIG)
        << "specified language '" << language
        << "' does not match previously used language '" << previous << "'";
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
