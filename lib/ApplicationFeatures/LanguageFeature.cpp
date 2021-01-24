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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#include "LanguageFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/application-exit.h"
#include "Basics/directories.h"
#include "Basics/error.h"
#include "Basics/exitcodes.h"
#include "Basics/files.h"
#include "Basics/memory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace {
void setCollator(std::string const& language, void* icuDataPtr) {
  using arangodb::basics::Utf8Helper;
  if (!Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(language, icuDataPtr)) {
    LOG_TOPIC("01490", FATAL, arangodb::Logger::FIXME)
        << "error setting collator language to '" << language << "'. "
        << "The icudtl.dat file might be of the wrong version. "
        << "Check for an incorrectly set ICU_DATA environment variable";
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_ICU_INITIALIZATION_FAILED);
  }
}

  void setLocale(icu::Locale& locale) {
  using arangodb::basics::Utf8Helper;
  std::string languageName;

  if (Utf8Helper::DefaultUtf8Helper.getCollatorCountry() != "") {
    languageName =
        std::string(Utf8Helper::DefaultUtf8Helper.getCollatorLanguage() + "_" +
                    Utf8Helper::DefaultUtf8Helper.getCollatorCountry());
    locale = icu::Locale(Utf8Helper::DefaultUtf8Helper.getCollatorLanguage().c_str(),
                         Utf8Helper::DefaultUtf8Helper.getCollatorCountry().c_str()
                    /*
                       const   char * variant  = 0,
                       const   char * keywordsAndValues = 0
                    */
    );
  } else {
    locale = icu::Locale(Utf8Helper::DefaultUtf8Helper.getCollatorLanguage().c_str());
    languageName = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
  }

  LOG_TOPIC("f6e04", DEBUG, arangodb::Logger::CONFIG)
      << "using default language '" << languageName << "'";
}
}  // namespace

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

LanguageFeature::LanguageFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Language"),
      _locale(),
      _binaryPath(server.getBinaryPath()),
      _icuDataPtr(nullptr) {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

LanguageFeature::~LanguageFeature() {
  if (_icuDataPtr != nullptr) {
    TRI_Free(_icuDataPtr);
  }
}

void LanguageFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addOption("--default-language", "ISO-639 language code",
                     new StringParameter(&_language),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}

void* LanguageFeature::prepareIcu(std::string const& binaryPath,
                                  std::string const& binaryExecutionPath,
                                  std::string& path, std::string const& binaryName) {
  std::string fn("icudtl.dat");
  if (TRI_GETENV("ICU_DATA", path)) {
    path = FileUtils::buildFilename(path, fn);
  }
  if (path.empty() || !TRI_IsRegularFile(path.c_str())) {
    if (!path.empty()) {
      LOG_TOPIC("581d1", WARN, arangodb::Logger::FIXME)
          << "failed to locate '" << fn << "' at '" << path << "'";
    }

    std::string bpfn = FileUtils::buildFilename(binaryExecutionPath, fn);

    if (TRI_IsRegularFile(fn.c_str())) {
      path = fn;
    } else if (TRI_IsRegularFile(bpfn.c_str())) {
      path = bpfn;
    } else {
      std::string argv0 = FileUtils::buildFilename(binaryExecutionPath, binaryName);
      path = TRI_LocateInstallDirectory(argv0.c_str(), binaryPath.c_str());
      path = FileUtils::buildFilename(path, ICU_DESTINATION_DIRECTORY, fn);

      if (!TRI_IsRegularFile(path.c_str())) {
        // Try whether we have an absolute install prefix:
        path = FileUtils::buildFilename(ICU_DESTINATION_DIRECTORY, fn);
      }
    }

    if (!TRI_IsRegularFile(path.c_str())) {
      std::string msg = std::string("failed to initialize ICU library. Could not locate '")
                        + path + "'. Please make sure it is available. "
                        "The environment variable ICU_DATA";
      std::string icupath;
      if (TRI_GETENV("ICU_DATA", icupath)) {
        msg += "='" + icupath + "'";
      }
      msg += " should point to the directory containing '" + fn + "'";

      LOG_TOPIC("0de77", FATAL, arangodb::Logger::FIXME) << msg;
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_ICU_INITIALIZATION_FAILED);
    } else {
      std::string icu_path = path.substr(0, path.length() - fn.length());
      FileUtils::makePathAbsolute(icu_path);
      FileUtils::normalizePath(icu_path);
#ifndef _WIN32
      setenv("ICU_DATA", icu_path.c_str(), 1);
#else
      icu::UnicodeString uicuEnv(icu_path.c_str(), (uint16_t)icu_path.length());
      SetEnvironmentVariableW(L"ICU_DATA", (wchar_t*)uicuEnv.getTerminatedBuffer());
#endif
    }
  }

  void* icuDataPtr = TRI_SlurpFile(path.c_str(), nullptr);

  if (icuDataPtr == nullptr) {
    LOG_TOPIC("d8a98", FATAL, arangodb::Logger::FIXME) << "failed to load '" << fn << "' at '"
                                              << path << "' - " << TRI_last_error();
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_ICU_INITIALIZATION_FAILED);
  }
  return icuDataPtr;
}

void LanguageFeature::prepare() {
  std::string p;
  auto context = ArangoGlobalContext::CONTEXT;
  std::string binaryExecutionPath = context->getBinaryPath();
  std::string binaryName = context->binaryName();
  _icuDataPtr = LanguageFeature::prepareIcu(_binaryPath, binaryExecutionPath, p, binaryName);

  ::setCollator(_language, _icuDataPtr);
}

void LanguageFeature::start() { ::setLocale(_locale); }

icu::Locale& LanguageFeature::getLocale() { return _locale; }

std::string const& LanguageFeature::getDefaultLanguage() const {
  return _language;
}

std::string LanguageFeature::getCollatorLanguage() const {
  using arangodb::basics::Utf8Helper;
  std::string languageName;
  if (Utf8Helper::DefaultUtf8Helper.getCollatorCountry() != "") {
    languageName =
        std::string(Utf8Helper::DefaultUtf8Helper.getCollatorLanguage() + "_" +
                    Utf8Helper::DefaultUtf8Helper.getCollatorCountry());
  } else {
    languageName = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
  }
  return languageName;
}

void LanguageFeature::resetDefaultLanguage(std::string const& language) {
  _language = language;
  ::setCollator(_language, _icuDataPtr);
  ::setLocale(_locale);
}

}  // namespace arangodb
