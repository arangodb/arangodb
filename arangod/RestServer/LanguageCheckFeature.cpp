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

/// @brief reads previous default langauge from file
arangodb::Result readLanguage(arangodb::ArangodServer& server,
                              std::string& defaultLanguage,
                              std::string& icuLanguage) {
  auto& databasePath = server.getFeature<arangodb::DatabasePathFeature>();
  std::string filename = databasePath.subdirectoryName("LANGUAGE");

  if (!TRI_ExistsFile(filename.c_str())) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  std::string defaultFound = {};
  std::string icuFound = {};

  try {
    VPackBuilder builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
    VPackSlice content = builder.slice();
    if (!content.isObject()) {
      return TRI_ERROR_INTERNAL;
    }
    VPackSlice defaultSlice = content.get("default");
    VPackSlice icuSlice = content.get("icu_language");

    if (!(defaultSlice.isString() ^ icuSlice.isString())) {
        return TRI_ERROR_INTERNAL;
    }

    if (defaultSlice.isString()) {
      defaultFound = defaultSlice.copyString();
    } else {
      icuFound = icuSlice.copyString();
    }

  } catch (...) {
    // Nothing to free
    return TRI_ERROR_INTERNAL;
  }

  defaultLanguage = defaultFound;
  icuLanguage = icuFound;

  if (!defaultLanguage.empty()) {
    LOG_TOPIC("c499e", TRACE, arangodb::Logger::CONFIG)
        << "using default language: " << defaultLanguage;
  } else {
    LOG_TOPIC("c499e", TRACE, arangodb::Logger::CONFIG)
        << "using icu language: " << icuLanguage;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief writes the default language to file
ErrorCode writeLanguage(arangodb::ArangodServer& server,
                        std::string const& lang,
                        bool isDefaultLang) {
  auto& databasePath = server.getFeature<arangodb::DatabasePathFeature>();
  std::string filename = databasePath.subdirectoryName("LANGUAGE");

  // generate json
  VPackBuilder builder;
  try {
    builder.openObject();
    if (isDefaultLang) {
      builder.add("default", VPackValue(lang));
    } else {
      builder.add("icu_language", VPackValue(lang));
    }

    builder.close();
  } catch (...) {
    // out of memory
    if (isDefaultLang) {
      LOG_TOPIC("4fa50", ERR, arangodb::Logger::CONFIG)
          << "cannot save default language in file '" << filename
          << "': out of memory";
    } else {
      LOG_TOPIC("4fa50", ERR, arangodb::Logger::CONFIG)
          << "cannot save icu language in file '" << filename
          << "': out of memory";
    }

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // save json info to file
  LOG_TOPIC("08f3c", DEBUG, arangodb::Logger::CONFIG)
      << "Writing language to file '" << filename << "'";
  bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(
      filename, builder.slice(), true);
  if (!ok) {
    LOG_TOPIC("c2fd7", ERR, arangodb::Logger::CONFIG)
        << "could not save language in file '" << filename
        << "': " << TRI_last_error();
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

std::tuple<std::string, bool> getOrSetPreviousLanguage(arangodb::ArangodServer& server,
                                     std::string const& collatorLang) {
  std::string defaultLanguage;
  std::string icuLanguage;
  arangodb::Result res = ::readLanguage(server, defaultLanguage, icuLanguage);

  bool isDefaultLang = !defaultLanguage.empty();
  if (res.ok()) {
    if (isDefaultLang) {
      return std::make_tuple(defaultLanguage, isDefaultLang);
    } else {
      return std::make_tuple(icuLanguage, isDefaultLang);
    }
  }

  // okay, we didn't find it, let's write out the input instead

  // we will treat collatorLang as 'default'
  isDefaultLang = true;
  ::writeLanguage(server, collatorLang, isDefaultLang);

  return std::make_tuple(collatorLang, isDefaultLang);
}
}  // namespace

namespace arangodb {

LanguageCheckFeature::LanguageCheckFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(false);
  startsAfter<DatabasePathFeature>();
  startsAfter<LanguageFeature>();
}

void LanguageCheckFeature::start() {
  auto& feature = server().getFeature<LanguageFeature>();
  auto defaultLang = feature.getDefaultLanguage();
  auto icuLang = feature.getIcuLanguage();

  auto collatorLang = feature.getCollatorLanguage();
  std::string previousLang;
  bool isDefaultLang;

  std::tie(previousLang, isDefaultLang)= ::getOrSetPreviousLanguage(server(), collatorLang);

  if (isDefaultLang && !previousLang.empty()) {
    // override the empty current setting for default with the previous one
    feature.resetDefaultLanguage(previousLang);
    return;
  } else if (!isDefaultLang && !previousLang.empty()) {
    // override the empty current setting for default with the previous one
    feature.resetIcuLanguage(previousLang);
    return;
  }

  if (collatorLang != previousLang) {
    if (feature.forceLanguageCheck()) {
      // current not empty and not the same as previous, get out!
      LOG_TOPIC("7ef60", FATAL, arangodb::Logger::CONFIG)
          << "specified language '" << collatorLang
          << "' does not match previously used language '" << previousLang << "'";
      FATAL_ERROR_EXIT();
    } else {
      LOG_TOPIC("54a68", WARN, arangodb::Logger::CONFIG)
          << "specified language '" << collatorLang
          << "' does not match previously used language '" << previousLang
          << "'. starting anyway due to --default-language-check=false "
             "setting";
    }
  }
}

}  // namespace arangodb
