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

#include "ApplicationFeatures/LanguageFeature.h"
#include "Basics/ArangoGlobalContext.h"

#include "Basics/FileUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/directories.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

LanguageFeature::LanguageFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Language"),
      _binaryPath(server->getBinaryPath()),
      _icuDataPtr(nullptr) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

LanguageFeature::~LanguageFeature() {
  if (_icuDataPtr != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _icuDataPtr);
  }
}

void LanguageFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addHiddenOption("--default-language", "ISO-639 language code",
                           new StringParameter(&_language));
}

void* LanguageFeature::prepareIcu(std::string const& binaryPath,
                                  std::string const& binaryExecutionPath,
                                  std::string& path,
                                  std::string const& binaryName) {
  char const* icuDataEnv = getenv("ICU_DATA");
  std::string fn("icudtl.dat");

  if (icuDataEnv != nullptr) {
    path = FileUtils::buildFilename(icuDataEnv, fn);
  }

  if (path.empty() || !TRI_IsRegularFile(path.c_str())) {
    if (!path.empty()) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "failed to locate '" << fn << "' at '" << path << "'";
    }

    std::string bpfn = FileUtils::buildFilename(binaryExecutionPath, fn);

    if (TRI_IsRegularFile(fn.c_str())) {
      path = fn;
    } else if (TRI_IsRegularFile(bpfn.c_str())) {
      path = bpfn;
    } else {
      std::string argv_0 = FileUtils::buildFilename(binaryExecutionPath, binaryName);
      path = TRI_LocateInstallDirectory(argv_0.c_str(), binaryPath.c_str());
      path = FileUtils::buildFilename(path, ICU_DESTINATION_DIRECTORY, fn);

      if (!TRI_IsRegularFile(path.c_str())) {
        // Try whether we have an absolute install prefix:
        path = FileUtils::buildFilename(ICU_DESTINATION_DIRECTORY, fn);
      }
    }

    if (!TRI_IsRegularFile(path.c_str())) {
      std::string msg = std::string("cannot locate '") + path +
                        "'; please make sure it is available; "
                        "the variable ICU_DATA='";
      if (getenv("ICU_DATA") != nullptr) {
        msg += getenv("ICU_DATA");
      }
      msg += "' should point to the directory containing '" + fn + "'";

      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << msg;
      FATAL_ERROR_EXIT();
    } else {
      std::string icu_path = path.substr(0, path.length() - fn.length());
      FileUtils::makePathAbsolute(icu_path);
      FileUtils::normalizePath(icu_path);
#ifndef _WIN32
      setenv("ICU_DATA", icu_path.c_str(), 1);
#else
      SetEnvironmentVariable("ICU_DATA", icu_path.c_str());
#endif
    }
  }

  void* icuDataPtr = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, path.c_str(), nullptr);

  if (icuDataPtr == nullptr) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "failed to load '" << fn << "' at '" << path << "' - "
        << TRI_last_error();
    FATAL_ERROR_EXIT();
  }
  return icuDataPtr;
}

void LanguageFeature::prepare() {
  std::string p;
  auto context = ArangoGlobalContext::CONTEXT;
  std::string binaryExecutionPath = context->getBinaryPath();
  std::string binaryName = context->binaryName();
  _icuDataPtr = LanguageFeature::prepareIcu(_binaryPath, binaryExecutionPath, p,
                                            binaryName);

  if (!Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(_language,
                                                         _icuDataPtr)) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "error initializing ICU with the contents of '" << p << "'";
    FATAL_ERROR_EXIT();
  }
}

void LanguageFeature::start() {
  std::string languageName;

  if (Utf8Helper::DefaultUtf8Helper.getCollatorCountry() != "") {
    languageName =
        std::string(Utf8Helper::DefaultUtf8Helper.getCollatorLanguage() + "_" +
                    Utf8Helper::DefaultUtf8Helper.getCollatorCountry());
  } else {
    languageName = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
  }

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
      << "using default language '" << languageName << "'";
}
