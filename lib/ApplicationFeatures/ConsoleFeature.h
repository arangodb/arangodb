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

#ifndef APPLICATION_FEATURES_CONSOLE_FEATURE_H
#define APPLICATION_FEATURES_CONSOLE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
class ClientFeature;

class ConsoleFeature final : public application_features::ApplicationFeature {
 public:
  explicit ConsoleFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void stop() override;

 public:
  bool quiet() const { return _quiet; }
  void setQuiet(bool value) { _quiet = value; }
  bool colors() const { return _colors; }
  bool autoComplete() const { return _autoComplete; }
  bool prettyPrint() const { return _prettyPrint; }
  bool pager() const { return _pager; }
  void setPager(bool value) { _pager = value; }
  std::string const& pagerCommand() const { return _pagerCommand; }
  std::string const& prompt() const { return _prompt; }

 private:
#ifdef WIN32
  int16_t _codePage;
  bool _cygwinShell;
#endif
  bool _quiet;
  bool _colors;
  bool _autoComplete;
  bool _prettyPrint;
  std::string _auditFile;
  bool _pager;
  std::string _pagerCommand;
  std::string _prompt;

 public:
  static void printContinuous(std::string const&);
  static void printLine(std::string const&, bool forceNewLine = false);
  static void printErrorLine(std::string const&);
  static std::string readPassword(std::string const& message);

 public:
  void setPromptError(bool value) { _promptError = value; }
  void setSupportsColors(bool value) { _supportsColors = value; }
  void printWelcomeInfo();
  void printByeBye();
  void print(std::string const&);
  void openLog();
  void closeLog();
  void log(std::string const&);
  void flushLog();

  struct Prompt {
    std::string _plain;
    std::string _colored;
  };

  Prompt buildPrompt(ClientFeature*);
  void startPager();
  void stopPager();

 private:
  bool _promptError;
  bool _supportsColors;
  FILE* _toPager;
  FILE* _toAuditFile;
};
}

#endif
