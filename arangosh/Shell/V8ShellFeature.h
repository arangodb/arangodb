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

#ifndef ARANGODB_SHELL_V8SHELL_FEATURE_H
#define ARANGODB_SHELL_V8SHELL_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include <v8.h>
#include <libplatform/libplatform.h>

#include "Shell/ConsoleFeature.h"
#include "Shell/ShellFeature.h"

namespace arangodb {
class ConsoleFeature;
class V8ClientConnection;

class V8ShellFeature final : public application_features::ApplicationFeature {
 public:
  V8ShellFeature(application_features::ApplicationServer* server,
                 std::string const&);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void start() override final;
  void unprepare() override final;

 private:
  std::string _startupDirectory;
  bool _currentModuleDirectory;
  uint64_t _gcInterval;

 public:
  int runShell(std::vector<std::string> const&);
  bool runScript(std::vector<std::string> const& files,
                 std::vector<std::string> const&, bool);
  bool runString(std::vector<std::string> const& files,
                 std::vector<std::string> const&);
  bool runUnitTests(std::vector<std::string> const& files,
                    std::vector<std::string> const&);
  bool jslint(std::vector<std::string> const& files);

 private:
  bool printHello(V8ClientConnection*);
  void initGlobals();
  void initMode(ShellFeature::RunMode, std::vector<std::string> const&);
  void loadModules(ShellFeature::RunMode);
  V8ClientConnection* setup(v8::Local<v8::Context>& context, bool,
                            std::vector<std::string> const&,
                            bool* promptError = nullptr);

 private:
  std::string _name;
  v8::Isolate* _isolate;
  v8::Persistent<v8::Context> _context;
  ConsoleFeature* _console;
};
}

#endif
