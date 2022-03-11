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

constexpr std::string_view kDefaultLangKey = "default";
constexpr std::string_view kIcuLangKey = "icu-language";

/// @brief reads previous default langauge from file
arangodb::Result readLanguage(arangodb::ArangodServer& server,
                              std::string& language,
                              arangodb::LanguageType& type) {
  auto& databasePath = server.getFeature<arangodb::DatabasePathFeature>();
  std::string filename = databasePath.subdirectoryName("LANGUAGE");

  if (!TRI_ExistsFile(filename.c_str())) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  try {
    VPackBuilder builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
    VPackSlice content = builder.slice();
    if (!content.isObject()) {
      return TRI_ERROR_INTERNAL;
    }
    VPackSlice defaultSlice = content.get(kDefaultLangKey);
    VPackSlice icuSlice = content.get(kIcuLangKey);

    // both languages are specified in files
    if (defaultSlice.isString() && icuSlice.isString()) {
      LOG_TOPIC("4fa52", ERR, arangodb::Logger::CONFIG)
          << "Only one language should be specified";
      return TRI_ERROR_INTERNAL;
    }

    if (defaultSlice.isString()) {
      language = defaultSlice.stringView();
      type = arangodb::LanguageType::DEFAULT;
    } else if (icuSlice.isString()) {
      language = icuSlice.stringView();
      type = arangodb::LanguageType::ICU;
    } else {
      return TRI_ERROR_INTERNAL;
    }

  } catch (...) {
    // Nothing to free
    return TRI_ERROR_INTERNAL;
  }

  if (arangodb::LanguageType::DEFAULT == type) {
    LOG_TOPIC("c499e", TRACE, arangodb::Logger::CONFIG)
        << "using default language: " << language;
  } else {
    LOG_TOPIC("c490e", TRACE, arangodb::Logger::CONFIG)
        << "using icu language: " << language;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief writes the default language to file
ErrorCode writeLanguage(arangodb::ArangodServer& server, std::string_view lang,
                        arangodb::LanguageType currLangType) {
  auto& databasePath = server.getFeature<arangodb::DatabasePathFeature>();
  std::string filename = databasePath.subdirectoryName("LANGUAGE");

  // generate json
  VPackBuilder builder;
  try {
    builder.openObject();
    if (arangodb::LanguageType::DEFAULT == currLangType) {
      builder.add(kDefaultLangKey, VPackValue(lang));
    } else {
      builder.add(kIcuLangKey, VPackValue(lang));
    }

    builder.close();
  } catch (...) {
    // out of memory
    if (arangodb::LanguageType::DEFAULT == currLangType) {
      LOG_TOPIC("4fa50", ERR, arangodb::Logger::CONFIG)
          << "cannot save default language in file '" << filename
          << "': out of memory";
    } else {
      LOG_TOPIC("4fa51", ERR, arangodb::Logger::CONFIG)
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

std::tuple<std::string, arangodb::LanguageType> getOrSetPreviousLanguage(
    arangodb::ArangodServer& server, std::string collatorLang,
    arangodb::LanguageType currLangType) {
  std::string prevLanguage;
  arangodb::LanguageType prevType;
  arangodb::Result res = ::readLanguage(server, prevLanguage, prevType);

  if (res.ok()) {
    TRI_ASSERT(!prevLanguage.empty());
    return {prevLanguage, prevType};
  }

  // okay, we didn't find it, let's write out the input instead
  ::writeLanguage(server, collatorLang, currLangType);

  return {collatorLang, currLangType};
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
  auto currDefaultLang = feature.getDefaultLanguage();
  auto currIcuLang = feature.getIcuLanguage();
  auto currLangType = feature.getLanguageType();
  auto collatorLang = feature.getCollatorLanguage();

  std::string prevLang;
  LanguageType prevLangType = LanguageType::INVALID;  // default value
  std::tie(prevLang, prevLangType) =
      ::getOrSetPreviousLanguage(server(), collatorLang, currLangType);

  if (currDefaultLang.empty() && currIcuLang.empty() && !prevLang.empty()) {
    // we found something in LANGUAGE file
    if (prevLangType == LanguageType::DEFAULT) {
      // override the empty current setting for default with the previous one
      feature.resetDefaultLanguage(prevLang);
      return;
    } else if (prevLangType == LanguageType::ICU) {
      // override the empty current setting for icu-lang with the previous one
      feature.resetIcuLanguage(prevLang);
      return;
    } else {
      LOG_TOPIC("7ef61", FATAL, arangodb::Logger::CONFIG)
          << "Specified language '" << collatorLang << " has invalid type";
      FATAL_ERROR_EXIT();
    }
  }

  if (collatorLang != prevLang) {
    if (feature.forceLanguageCheck()) {
      // current not empty and not the same as previous, get out!
      LOG_TOPIC("7ef60", FATAL, arangodb::Logger::CONFIG)
          << "Specified language '" << collatorLang
          << "' does not match previously used language '" << prevLang << "'";
      FATAL_ERROR_EXIT();
    } else {
      LOG_TOPIC("54a68", WARN, arangodb::Logger::CONFIG)
          << "Specified language '" << collatorLang
          << "' does not match previously used language '" << prevLang
          << "'. starting anyway due to --default-language-check=false "
             "setting";
    }
  }

  if (prevLangType != currLangType) {
    if (feature.forceLanguageCheck()) {
      // current not empty and not the same as previous, get out!
      LOG_TOPIC("7ef40", FATAL, arangodb::Logger::CONFIG)
          << "Specified language type does not match previously used language "
             "type";
      FATAL_ERROR_EXIT();
    } else {
      LOG_TOPIC("54a40", WARN, arangodb::Logger::CONFIG)
          << "Specified language type does not match previously used language "
             "type"
          << "'. starting anyway due to --default-language-check=false "
             "setting";
    }
  }
}

}  // namespace arangodb
