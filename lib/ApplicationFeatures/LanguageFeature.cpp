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

#include "Basics/Utf8Helper.h"
#include "Basics/files.h"
#include "Basics/directories.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

LanguageFeature::~LanguageFeature() {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, _icuDataPtr);
}

LanguageFeature::LanguageFeature(
    application_features::ApplicationServer* server)
  : 
    ApplicationFeature(server, "Language"),
    _binaryPath(server->getBinaryPath()){
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void LanguageFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addHiddenOption("--default-language", "ISO-639 language code",
                           new StringParameter(&_language));
}

void * LanguageFeature::prepareIcu(std::string const& binaryPath, std::string const& binaryExecutionPath, std::string & path) {
  void *icuDataPtr = nullptr;
  std::string fn("icudtl.dat");
  char const* icuDataEnv = getenv("ICU_DATA");

  if (icuDataEnv != nullptr) {
    path = icuDataEnv + fn;
  }

  if (path.length() == 0 || !TRI_IsRegularFile(path.c_str())) {
    if (path.length() > 0) {
      LOG(WARN) << "failed to locate '"<< fn << "' at '"<< path << "'";
    }
    std::string bpfn = binaryExecutionPath + TRI_DIR_SEPARATOR_STR  + fn;
    if (TRI_IsRegularFile(fn.c_str())) {
      path = fn;
    }
    else if (TRI_IsRegularFile(bpfn.c_str())) {
      path = bpfn;
    }
    else {
# if _WIN32
      path = TRI_LocateInstallDirectory(binaryPath);
#else
      path = TRI_DIR_SEPARATOR_STR;
# endif
      path += ICU_DESTINATION_DIRECTORY TRI_DIR_SEPARATOR_STR + fn;
    }
    if (! TRI_IsRegularFile(path.c_str())) {
      std::string msg =
        std::string("cannot locate ") + path + "; please make it is available; "
        "the variable ICU_DATA='";
      if (getenv("ICU_DATA") != nullptr) {
        msg += getenv("ICU_DATA");
      }
      msg += "' should point the directory containing " + fn;

      LOG(FATAL) << msg;
      FATAL_ERROR_EXIT();
    }
  }

  icuDataPtr = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, path.c_str(), nullptr);

  if (icuDataPtr == nullptr) {
    LOG(FATAL) << "failed to load " << fn << " at '" << path << "' - " << TRI_last_error();
    FATAL_ERROR_EXIT();
  }
  return icuDataPtr;
}

void LanguageFeature::prepare() {
  auto context = ArangoGlobalContext::CONTEXT;
  std::string p;
  std::string binaryExecutionPath = context->getBinaryPath();
  _icuDataPtr = LanguageFeature::prepareIcu(_binaryPath, binaryExecutionPath, p);
  if(!Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(_language, _icuDataPtr)) {
    LOG(FATAL) << "error initializing ICU with the contents of '" << p;
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

  LOG(DEBUG) << "using default language '" << languageName << "'";
}
