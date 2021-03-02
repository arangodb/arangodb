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

#include "V8ShellFeature.h"

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Basics/terminal-utils.h"
#include "Utilities/IsArangoExecutable.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/RandomFeature.h"
#include "Random/RandomGenerator.h"
#include "Rest/CommonDefines.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "Shell/ConsoleFeature.h"
#include "Shell/V8ClientConnection.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8/v8-deadline.h"
#include "V8/v8-vpack.h"

#include <regex>

extern "C" {
#include <linenoise.h>
}

using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

static std::string const DEFAULT_CLIENT_MODULE = "client.js";

namespace arangodb {

V8ShellFeature::V8ShellFeature(application_features::ApplicationServer& server,
                               std::string const& name)
    : ApplicationFeature(server, "V8Shell"),
      _startupDirectory("js"),
      _clientModule(DEFAULT_CLIENT_MODULE),
      _currentModuleDirectory(true),
      _copyInstallation(false),
      _removeCopyInstallation(false),
      _gcInterval(50),
      _name(name),
      _isolate(nullptr) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseClient>();

  startsAfter<ConsoleFeature>();
  startsAfter<RandomFeature>();
  startsAfter<V8PlatformFeature>();
  startsAfter<V8SecurityFeature>();
}

void V8ShellFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the JavaScript engine");

  options->addOption("--javascript.startup-directory",
                     "startup paths containing the JavaScript files",
                     new StringParameter(&_startupDirectory),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--javascript.client-module",
                     "client module to use at startup", new StringParameter(&_clientModule),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption(
      "--javascript.copy-directory",
      "target directory to copy files from 'javascript.startup-directory' into "
      "(only used when `--javascript.copy-installation` is enabled)",
      new StringParameter(&_copyDirectory));

  options->addOption("--javascript.module-directory",
                     "additional paths containing JavaScript modules",
                     new VectorParameter<StringParameter>(&_moduleDirectories),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--javascript.current-module-directory",
                     "add current directory to module path",
                     new BooleanParameter(&_currentModuleDirectory));

  options->addOption("--javascript.copy-installation",
                     "copy contents of 'javascript.startup-directory'",
                     new BooleanParameter(&_copyInstallation));

  options->addOption(
      "--javascript.gc-interval",
      "request-based garbage collection interval (each n.th command)",
      new UInt64Parameter(&_gcInterval));
}

void V8ShellFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  if (_startupDirectory.empty()) {
    LOG_TOPIC("6380f", FATAL, arangodb::Logger::FIXME)
        << "no '--javascript.startup-directory' has been supplied, giving up";
    FATAL_ERROR_EXIT();
  }

  if (!_moduleDirectories.empty()) {
    LOG_TOPIC("90ca0", DEBUG, Logger::V8) << "using JavaScript modules at '"
                                 << StringUtils::join(_moduleDirectories, ";") << "'";
  }
}

void V8ShellFeature::start() {
  auto& platform = server().getFeature<V8PlatformFeature>();

  if (_copyInstallation) {
    copyInstallationFiles();  // will exit process on error
  }

  LOG_TOPIC("9c2f7", DEBUG, Logger::V8)
      << "using JavaScript startup files at '" << _startupDirectory << "'";

  _isolate = platform.createIsolate();

  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  auto* isolate = _isolate;
  TRI_GET_GLOBALS();
  v8g = TRI_CreateV8Globals(server(), isolate, 0);
  v8g->_securityContext = arangodb::JavaScriptSecurityContext::createAdminScriptContext();

  // create the global template
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(_isolate);

  // create the context
  _context.Reset(_isolate, v8::Context::New(_isolate, nullptr, global));

  auto context = v8::Local<v8::Context>::New(_isolate, _context);

  if (context.IsEmpty()) {
    LOG_TOPIC("5f5dd", FATAL, arangodb::Logger::FIXME) << "cannot initialize V8 engine";
    FATAL_ERROR_EXIT();
  }

  // fill the global object
  v8::Context::Scope context_scope{context};

  v8::Handle<v8::Object> globalObj = context->Global();
  globalObj->Set(context, TRI_V8_ASCII_STRING(_isolate, "GLOBAL"), globalObj).FromMaybe(false);
  globalObj->Set(context, TRI_V8_ASCII_STRING(_isolate, "global"), globalObj).FromMaybe(false);
  globalObj->Set(context, TRI_V8_ASCII_STRING(_isolate, "root"),   globalObj).FromMaybe(false);

  initGlobals();
}

void V8ShellFeature::unprepare() {
  {
    v8::Locker locker{_isolate};

    v8::Isolate::Scope isolate_scope(_isolate);
    v8::HandleScope handle_scope(_isolate);

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

    v8::Context::Scope context_scope{context};

    // clear globals to free memory
    auto isolate = _isolate;
    auto globals = _isolate->GetCurrentContext()->Global();
    v8::Handle<v8::Array> names = globals->GetOwnPropertyNames(context).FromMaybe(v8::Local<v8::Array>());
    uint32_t const n = names->Length();
    for (uint32_t i = 0; i < n; ++i) {
      TRI_DeleteProperty(context, isolate, globals, names->Get(context, i).FromMaybe(v8::Local<v8::Value>()));
    }

    TRI_RunGarbageCollectionV8(_isolate, 2500.0);
  }

  {
    v8::Locker locker{_isolate};
    v8::Isolate::Scope isolate_scope{_isolate};

    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
        _isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));
    _isolate->SetData(arangodb::V8PlatformFeature::V8_DATA_SLOT, nullptr);

    delete v8g;

    _context.Reset();
  }

  _isolate->Dispose();
}

void V8ShellFeature::stop() {
  if (_removeCopyInstallation && !_copyDirectory.empty()) {
    auto res = TRI_RemoveDirectory(_copyDirectory.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("cac43", DEBUG, Logger::V8)
          << "could not cleanup installation file copy in path '"
          << _copyDirectory << "': " << TRI_errno_string(res);
    }
  }
}

void V8ShellFeature::copyInstallationFiles() {
  if (_copyDirectory.empty()) {
    uint64_t r = RandomGenerator::interval(UINT64_MAX);
    char buf[sizeof(uint64_t) * 2 + 1];
    auto len = TRI_StringUInt64HexInPlace(r, buf);
    std::string name("arangosh-js-");
    name.append(buf, len);
    _copyDirectory = FileUtils::buildFilename(TRI_GetTempPath(), name);
    _removeCopyInstallation = true;
  }

  LOG_TOPIC("65ed7", DEBUG, Logger::V8) << "Copying JS installation files from '" << _startupDirectory
                               << "' to '" << _copyDirectory << "'";

  _nodeModulesDirectory = _startupDirectory;

  if (FileUtils::exists(_copyDirectory)) {
    auto res = TRI_ERROR_NO_ERROR;
    res = TRI_RemoveDirectory(_copyDirectory.c_str());
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("379f5", FATAL, Logger::V8) << "Error cleaning JS installation path '" << _copyDirectory
                                   << "': " << TRI_errno_string(res);
      FATAL_ERROR_EXIT();
    }
  }

  if (auto res = TRI_ERROR_NO_ERROR; !FileUtils::createDirectory(_copyDirectory, &res)) {
    auto err = TRI_last_error();
    LOG_TOPIC("6d915", FATAL, Logger::V8)
        << "Error creating JS installation path '" << _copyDirectory << "': " << err;
    FATAL_ERROR_EXIT();
  }

  // intentionally do not copy js/node/node_modules...
  // we avoid copying this directory because it contains 5000+ files at the
  // moment, and copying them one by one is darn slow at least on Windows...
  std::string const versionAppendix =
      std::regex_replace(rest::Version::getServerVersion(), std::regex("-.*$"),
                         "");
  std::string const nodeModulesPath =
      FileUtils::buildFilename("js", "node", "node_modules");
  std::string const nodeModulesPathVersioned =
      basics::FileUtils::buildFilename("js", versionAppendix, "node",
                                       "node_modules");
  std::regex const binRegex("[/\\\\]\\.bin[/\\\\]", std::regex::ECMAScript);

  auto filter = [&nodeModulesPath, &nodeModulesPathVersioned, &binRegex](std::string const& filename) -> bool {
    if (std::regex_search(filename, binRegex)) {
      // don't copy files in .bin
      return true;
    }

    std::string normalized = filename;
    FileUtils::normalizePath(normalized);
    if ((!nodeModulesPath.empty() && 
         normalized.size() >= nodeModulesPath.size() &&
         normalized.substr(normalized.size() - nodeModulesPath.size(), nodeModulesPath.size()) == nodeModulesPath) ||
        (!nodeModulesPathVersioned.empty() &&
         normalized.size() >= nodeModulesPathVersioned.size() &&
         normalized.substr(normalized.size() - nodeModulesPathVersioned.size(), nodeModulesPathVersioned.size()) == nodeModulesPathVersioned)) {
      // filter it out!
      return true;
    }
    // let the file/directory pass through
    return false;
  };
  std::string error;
  if (!FileUtils::copyRecursive(_startupDirectory, _copyDirectory, filter, error)) {
    LOG_TOPIC("913c4", FATAL, Logger::V8) << "Error copying JS installation files to '"
                                 << _copyDirectory << "': " << error;
    FATAL_ERROR_EXIT();
  }

  _startupDirectory = _copyDirectory;
}

bool V8ShellFeature::printHello(V8ClientConnection* v8connection) {
  ConsoleFeature& console = server().getFeature<ConsoleFeature>();
  bool promptError = false;

  // for the banner see
  // http://www.network-science.de/ascii/   Font: ogre

  if (!console.quiet()) {
    if (_clientModule == DEFAULT_CLIENT_MODULE) {
      std::string g = ShellColorsFeature::SHELL_COLOR_GREEN;
      std::string r = ShellColorsFeature::SHELL_COLOR_RED;
      std::string z = ShellColorsFeature::SHELL_COLOR_RESET;

      if (!console.colors()) {
        g = "";
        r = "";
        z = "";
      }

      // clang-format off

      console.printLine("");
      console.printLine(g + "                                  "     + r + "     _     " + z);
      console.printLine(g + "  __ _ _ __ __ _ _ __   __ _  ___ "     + r + " ___| |__  " + z);
      console.printLine(g + " / _` | '__/ _` | '_ \\ / _` |/ _ \\"   + r + "/ __| '_ \\ " + z);
      console.printLine(g + "| (_| | | | (_| | | | | (_| | (_) "     + r + "\\__ \\ | | |" + z);
      console.printLine(g + " \\__,_|_|  \\__,_|_| |_|\\__, |\\___/" + r + "|___/_| |_|" + z);
      console.printLine(g + "                       |___/      "     + r + "           " + z);
      console.printLine("");

      // clang-format on

      std::ostringstream s;

      s << "arangosh (" << rest::Version::getVerboseVersionString() << ")\n"
        << "Copyright (c) ArangoDB GmbH";

      console.printLine(s.str());
      console.printLine("");

      console.printWelcomeInfo();
    }
        
    ClientFeature& client =
          server().getFeature<HttpEndpointProvider, ClientFeature>();

    if (v8connection != nullptr) {
      if (v8connection->isConnected() &&
          v8connection->lastHttpReturnCode() == (int)rest::ResponseCode::OK) {
        std::string msg = ClientFeature::buildConnectedMessage(
            v8connection->endpointSpecification(), v8connection->version(),
            v8connection->role(), v8connection->mode(), v8connection->databaseName(),
            v8connection->username());
        console.printLine(msg);

        if (v8connection->role() == "PRIMARY" || v8connection->role() == "DBSERVER") {
          std::string msg("WARNING: You connected to a DBServer node, but operations in a cluster should be carried out via a Coordinator");
          if (console.colors()) {
            msg = ShellColorsFeature::SHELL_COLOR_RED + msg + ShellColorsFeature::SHELL_COLOR_RESET;
          }
          console.printErrorLine(msg);
        }
      } else if (client.endpoint() != "none") {
        std::ostringstream is;
        is << "Could not connect to endpoint '" << client.endpoint()
           << "', database: '" << v8connection->databaseName()
           << "', username: '" << v8connection->username() << "'";

        console.printErrorLine(is.str());

        if (!v8connection->lastErrorMessage().empty()) {
          std::ostringstream is2;

          is2 << "Error message: '" << v8connection->lastErrorMessage() << "'";

          console.printErrorLine(is2.str());
        }

        promptError = true;
      }

      console.printLine("");
    }
  }

  return promptError;
}

// the result is wrapped in a JavaScript variable SYS_ARANGO
std::shared_ptr<V8ClientConnection> V8ShellFeature::setup(
    v8::Local<v8::Context>& context, bool createConnection,
    std::vector<std::string> const& positionals, bool* promptError) {
  std::shared_ptr<V8ClientConnection> v8connection;

  bool haveClient = false;
  if (createConnection) {
    if (server().hasFeature<HttpEndpointProvider>()) {
      haveClient = true;
      ClientFeature& client = server().getFeature<HttpEndpointProvider, ClientFeature>();
      v8connection = std::make_shared<V8ClientConnection>(server(), client);
      if (client.isEnabled()) {
        v8connection->connect();
      }
    }
  }

  initMode(ShellFeature::RunMode::INTERACTIVE, positionals);

  if (createConnection && haveClient) {
    v8connection->initServer(_isolate, context);
  }

  bool pe = printHello(v8connection.get());
  loadModules(ShellFeature::RunMode::INTERACTIVE);

  if (promptError != nullptr) {
    *promptError = pe;
  }

  return v8connection;
}

ErrorCode V8ShellFeature::runShell(std::vector<std::string> const& positionals) {
  ConsoleFeature& console = server().getFeature<ConsoleFeature>();

  auto isolate = _isolate;
  v8::Locker locker{_isolate};
  v8::Isolate::Scope isolate_scope(_isolate);

  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  bool promptError;
  auto v8connection = setup(context, true, positionals, &promptError);

  V8LineEditor v8LineEditor(_isolate, context,
                            console.useHistory() ? "." + _name + ".history" : "");
  

  if (v8connection != nullptr) {
    v8LineEditor.setSignalFunction(
        [v8connection]() { v8connection->setInterrupted(true); });
  }

  v8LineEditor.open(console.autoComplete());

  v8::Local<v8::String> name(TRI_V8_ASCII_STRING(_isolate, TRI_V8_SHELL_COMMAND_NAME));

  uint64_t nrCommands = 0;

  ClientFeature* client = nullptr;
  if (server().hasFeature<HttpEndpointProvider>()) {
    client = &(server().getFeature<HttpEndpointProvider, ClientFeature>());
    if (!client->isEnabled()) {
      client = nullptr;
    }
  }

  bool const isBatch = isatty(STDIN_FILENO) == 0;
  bool lastEmpty = isBatch;
  double lastDuration = 0.0;

  while (true) {
    console.setLastDuration(lastDuration);
    console.setPromptError(promptError);
    auto prompt = console.buildPrompt(client);

    ShellBase::EofType eof = ShellBase::EOF_NONE;
    std::string input = v8LineEditor.prompt(prompt._colored, prompt._plain, eof);

    if (eof == ShellBase::EOF_FORCE_ABORT || (eof == ShellBase::EOF_ABORT && lastEmpty)) {
      break;
    }

    if (input.empty()) {
      promptError = false;
      lastEmpty = true;
      lastDuration = 0.0;
      continue;
    }
    lastEmpty = isBatch;

    console.log(prompt._plain + input + "\n");

    std::string i = StringUtils::trim(input);

    if (i == "exit" || i == "quit" || i == "exit;" || i == "quit;") {
      break;
    }

    if (i == "help" || i == "help;") {
      input = "help()";
    }

    v8LineEditor.addHistory(input);

    v8::TryCatch tryCatch(isolate);

    console.startPager();

    // assume the command succeeds
    promptError = false;

    // execute command and register its result in __LAST__
    v8LineEditor.setExecutingCommand(true);
    double t1 = TRI_microtime();

    v8::Handle<v8::Value> v =
        TRI_ExecuteJavaScriptString(_isolate, context,
                                    TRI_V8_STD_STRING(_isolate, input), name, true);

    lastDuration = TRI_microtime() - t1;

    v8LineEditor.setExecutingCommand(false);

    if (v.IsEmpty()) {
      context->Global()->Set(context, TRI_V8_ASCII_STRING(_isolate, "_last"), v8::Undefined(_isolate)).FromMaybe(false);
    } else {
      context->Global()->Set(context, TRI_V8_ASCII_STRING(_isolate, "_last"), v).FromMaybe(false);
    }

    // command failed
    if (tryCatch.HasCaught()) {
      std::string exception;

      if (!tryCatch.CanContinue() || tryCatch.HasTerminated()) {
        exception = "command locally aborted\n";
      } else {
        exception = TRI_StringifyV8Exception(_isolate, &tryCatch);
      }

      console.printErrorLine(exception);
      console.log(exception);
      i = extractShellExecutableName(i);
      if (!i.empty()) {
        LOG_TOPIC("abeec", WARN, arangodb::Logger::FIXME)
          << "This command must be executed in a system shell and cannot be used inside of arangosh: '" << i << "'";
      }

      // this will change the prompt for the next round
      promptError = true;
    }

    if (v8connection != nullptr && v8connection->isConnected()) {
      v8connection->setInterrupted(false);
    }

    console.stopPager();
    console.printLine("");

    console.log("\n");

    // make sure the last command result makes it into the log file
    console.flushLog();

    // gc
    if (++nrCommands >= _gcInterval || V8PlatformFeature::isOutOfMemory(_isolate)) {
      nrCommands = 0;
      TRI_RunGarbageCollectionV8(_isolate, 500.0);

      // needs to be reset after the garbage collection
      V8PlatformFeature::resetOutOfMemory(_isolate);
    }
  }

  if (!console.quiet()) {
    console.printLine("");
    console.printByeBye();
  }

  return promptError ? TRI_ERROR_INTERNAL : TRI_ERROR_NO_ERROR;
}

bool V8ShellFeature::runScript(std::vector<std::string> const& files,
                               std::vector<std::string> const& positionals, bool execute,
                               std::vector<std::string> const& mainArgs, bool runMain) {
  auto isolate = _isolate;
  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  auto v8connection = setup(context, execute, positionals);

  bool ok = true;

  for (auto const& file : files) {
    if (!FileUtils::exists(file)) {
      LOG_TOPIC("4beec", ERR, arangodb::Logger::FIXME)
          << "error: JavaScript file not found: '" << file << "'";
      ok = false;
      continue;
    }

    if (execute) {
      v8::TryCatch tryCatch(isolate);

      v8::Handle<v8::String> name = TRI_V8_STD_STRING(_isolate, file);
      v8::Handle<v8::Value> args[] = {name};
      v8::Handle<v8::Value> filename = args[0];

      v8::Handle<v8::Object> current = _isolate->GetCurrentContext()->Global();

      auto oldFilename =
        current->Get(context, TRI_V8_ASCII_STRING(_isolate, "__filename")).FromMaybe(v8::Handle<v8::Value>());

      current->Set(context, TRI_V8_ASCII_STRING(_isolate, "__filename"), filename).FromMaybe(false);

      auto oldDirname = current->Get(context, TRI_V8_ASCII_STRING(_isolate, "__dirname")).FromMaybe(v8::Handle<v8::Value>());

      auto dirname = FileUtils::dirname(TRI_ObjectToString(isolate, filename));

      current->Set(context,
                   TRI_V8_ASCII_STRING(_isolate, "__dirname"),
                   TRI_V8_STD_STRING(_isolate, dirname)).FromMaybe(false);

      ok = TRI_ExecuteGlobalJavaScriptFile(_isolate, file.c_str());

      // restore old values for __dirname and __filename
      if (oldFilename.IsEmpty() || oldFilename->IsUndefined()) {
        TRI_DeleteProperty(context, isolate, current, "__filename");
      } else {
        current->Set(context,
                     TRI_V8_ASCII_STRING(_isolate, "__filename"), oldFilename).FromMaybe(false);
      }

      if (oldDirname.IsEmpty() || oldDirname->IsUndefined()) {
        TRI_DeleteProperty(context, isolate, current, "__dirname");
      } else {
        current->Set(context, TRI_V8_ASCII_STRING(_isolate, "__dirname"), oldDirname).FromMaybe(false);
      }

      if (tryCatch.HasCaught()) {
        std::string exception(TRI_StringifyV8Exception(_isolate, &tryCatch));
        LOG_TOPIC("c254f", ERR, arangodb::Logger::FIXME) << exception;
        ok = false;
      }
      if (ok && runMain) {
        v8::TryCatch tryCatch(isolate);
        // run the garbage collection for at most 30 seconds
        TRI_RunGarbageCollectionV8(isolate, 30.0);

        // parameter array
        v8::Handle<v8::Array> params = v8::Array::New(isolate);

        params->Set(context, 0, TRI_V8_STD_STRING(isolate, files[files.size() - 1])).FromMaybe(false);

        for (size_t i = 0; i < mainArgs.size(); ++i) {
          params
              ->Set(context, (uint32_t)(i + 1), TRI_V8_STD_STRING(isolate, mainArgs[i]))
              .FromMaybe(false);
        }

        // call main
        v8::Handle<v8::String> mainFuncName = TRI_V8_ASCII_STRING(isolate, "main");
        v8::Handle<v8::Function> main = v8::Handle<v8::Function>::Cast(
            context->Global()->Get(context, mainFuncName).FromMaybe(v8::Handle<v8::Value>()));

        if (!main.IsEmpty() && !main->IsUndefined()) {
          v8::Handle<v8::Value> args[] = {params};
          try {
            v8::Handle<v8::Value> result =
                main->Call(TRI_IGETC, main, 1, args).FromMaybe(v8::Local<v8::Value>());
            if (tryCatch.HasCaught()) {
              if (tryCatch.CanContinue()) {
                TRI_LogV8Exception(isolate, &tryCatch);
              } else {
                // will stop, so need for v8g->_canceled = true;
                TRI_ASSERT(!ok);
              }
            } else {
              ok = TRI_ObjectToDouble(isolate, result) == 0;
            }
          } catch (arangodb::basics::Exception const& ex) {
            LOG_TOPIC("525a4", ERR, arangodb::Logger::FIXME)
                << "caught exception " << TRI_errno_string(ex.code()) << ": " << ex.what();
            ok = false;
          } catch (std::bad_alloc const&) {
            LOG_TOPIC("abc8b", ERR, arangodb::Logger::FIXME)
                << "caught exception " << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
            ok = false;
          } catch (...) {
            LOG_TOPIC("4da99", ERR, arangodb::Logger::FIXME)
                << "caught unknown exception";
            ok = false;
          }
        } else {
          LOG_TOPIC("5da99", ERR, arangodb::Logger::FIXME)
                << "Function 'main' was not found";
          ok = false;
        }
      }
    } else {
      ok = TRI_ParseJavaScriptFile(_isolate, file.c_str());
    }
  }
  ConsoleFeature& console = server().getFeature<ConsoleFeature>();
  console.flushLog();

  return ok;
}

bool V8ShellFeature::runString(std::vector<std::string> const& strings,
                               std::vector<std::string> const& positionals) {
  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  auto v8connection = setup(context, true, positionals);

  bool ok = true;
  for (auto const& script : strings) {
    v8::TryCatch tryCatch(_isolate);

    v8::Handle<v8::Value> result = TRI_ExecuteJavaScriptString(
        _isolate, context, TRI_V8_STD_STRING(_isolate, script),
        TRI_V8_ASCII_STRING(_isolate, "(command-line)"), false);

    if (tryCatch.HasCaught()) {
      std::string exception(TRI_StringifyV8Exception(_isolate, &tryCatch));
      LOG_TOPIC("979b9", ERR, arangodb::Logger::FIXME) << exception;
      ok = false;
    } else {
      // check return value of script
      if (result->IsNumber()) {
        int64_t intResult = TRI_ObjectToInt64(_isolate, result);

        if (intResult != 0) {
          ok = false;
        }
      }
    }
  }

  ConsoleFeature& console = server().getFeature<ConsoleFeature>();
  console.flushLog();

  return ok;
}

bool V8ShellFeature::jslint(std::vector<std::string> const& files) {
  auto isolate = _isolate;
  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  setup(context, false, {});

  bool ok = true;

  // set-up jslint files array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(_isolate);

  uint32_t i = 0;
  for (auto& file : files) {
    if (!FileUtils::exists(file)) {
      LOG_TOPIC("4f748", ERR, arangodb::Logger::FIXME)
          << "error: JavaScript file not found: '" << file << "'";
      ok = false;
      continue;
    }

    sysTestFiles->Set(context, i, TRI_V8_STD_STRING(_isolate, file)).FromMaybe(false);
    ++i;
  }

  context->Global()->Set(context, TRI_V8_ASCII_STRING(_isolate, "SYS_UNIT_TESTS"), sysTestFiles).FromMaybe(false);

  context->Global()->Set(context, TRI_V8_ASCII_STRING(_isolate, "SYS_UNIT_TESTS_RESULT"),
                         v8::True(_isolate)).FromMaybe(false);

  // run tests
  auto input =
      TRI_V8_ASCII_STRING(_isolate,
                          "require(\"jslint\").runCommandLineTests({});");

  auto name = TRI_V8_ASCII_STRING(_isolate, TRI_V8_SHELL_COMMAND_NAME);

  v8::TryCatch tryCatch(isolate);
  TRI_ExecuteJavaScriptString(_isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    LOG_TOPIC("25acc", ERR, arangodb::Logger::FIXME)
        << TRI_StringifyV8Exception(_isolate, &tryCatch);
    ok = false;
  } else {
    bool res =
        TRI_ObjectToBoolean(isolate,
                            TRI_GetProperty(context, isolate, context->Global(),
                                                    "SYS_UNIT_TESTS_RESULT"));

    ok = ok && res;
  }

  return ok;
}

bool V8ShellFeature::runUnitTests(std::vector<std::string> const& files,
                                  std::vector<std::string> const& positionals,
                                  std::string const& testFilter) {
  auto isolate = _isolate;
  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  auto v8connection = setup(context, true, positionals);
  bool ok = true;

  // set-up unit tests array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(_isolate);

  uint32_t i = 0;

  for (auto const& file : files) {
    if (!FileUtils::exists(file)) {
      LOG_TOPIC("51bdb", ERR, arangodb::Logger::FIXME)
          << "error: JavaScript file not found: '" << file << "'";
      ok = false;
      continue;
    }

    sysTestFiles->Set(context, i, TRI_V8_STD_STRING(_isolate, file)).FromMaybe(false);
    ++i;
  }

  TRI_AddGlobalVariableVocbase(_isolate,
                               TRI_V8_ASCII_STRING(_isolate, "SYS_UNIT_TESTS"), sysTestFiles);

  // do not use TRI_AddGlobalVariableVocbase because it creates read-only
  // variables!!
  context->Global()->Set(context,
                         TRI_V8_ASCII_STRING(_isolate, "SYS_UNIT_TESTS_RESULT"),
                         v8::True(_isolate)).FromMaybe(false);

  context->Global()->Set(context,
                         TRI_V8_ASCII_STRING(_isolate, "SYS_UNIT_FILTER_TEST"),
                         TRI_V8_ASCII_STD_STRING(_isolate, testFilter)).FromMaybe(false);

  // run tests
  auto input = TRI_V8_ASCII_STRING(
      _isolate, "require(\"@arangodb/testrunner\").runCommandLineTests();");

  auto name = TRI_V8_ASCII_STRING(_isolate, TRI_V8_SHELL_COMMAND_NAME);

  v8::TryCatch tryCatch(isolate);
  TRI_ExecuteJavaScriptString(_isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    std::string exception(TRI_StringifyV8Exception(_isolate, &tryCatch));
    LOG_TOPIC("59589", ERR, arangodb::Logger::FIXME) << exception;
    ok = false;
  } else {
    bool res =
        TRI_ObjectToBoolean(isolate,
                            TRI_GetProperty(context, isolate, context->Global(),
                                                    "SYS_UNIT_TESTS_RESULT"));

    ok = ok && res;
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
////////////////////////////////////////////////////////////////////////////////

static void JS_PagerOutput(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ConsoleFeature* console = static_cast<ConsoleFeature*>(wrap->Value());

  for (int i = 0; i < args.Length(); i++) {
    // extract the next argument
    console->print(TRI_ObjectToString(isolate, args[i]));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StartOutputPager(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ConsoleFeature* console = static_cast<ConsoleFeature*>(wrap->Value());

  if (console->pager()) {
    console->print("Using pager already.\n");
  } else {
    console->setPager(true);
    console->print(std::string(std::string("Using pager ") + console->pagerCommand() +
                               " for output buffering.\n"));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StopOutputPager(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ConsoleFeature* console = static_cast<ConsoleFeature*>(wrap->Value());

  if (console->pager()) {
    console->print("Stopping pager.\n");
  } else {
    console->print("Pager not running.\n");
  }

  console->setPager(false);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_NormalizeString(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("NORMALIZE_STRING(<string>)");
  }

  TRI_normalize_V8_Obj(args, args[0]);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_CompareString(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "COMPARE_STRING(<left string>, <right string>)");
  }

  v8::String::Value left(isolate, args[0]);
  v8::String::Value right(isolate, args[1]);

  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(*left, left.length(),
                                                          *right, right.length());

  TRI_V8_RETURN(v8::Integer::New(isolate, result));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return client version
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionClient(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  bool details = false;
  if (args.Length() > 0) {
    details = TRI_ObjectToBoolean(isolate, args[0]);
  }

  if (!details) {
    // return version string
    TRI_V8_RETURN(TRI_V8_ASCII_STRING(isolate, ARANGODB_VERSION));
  }

  // return version details
  VPackBuilder builder;
  builder.openObject();
  rest::Version::getVPack(builder);
  builder.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exit now
////////////////////////////////////////////////////////////////////////////////

static void JS_Exit(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  int64_t code = 0;

  if (args.Length() > 0) {
    code = TRI_ObjectToInt64(isolate, args[0]);
  }

  TRI_GET_GLOBALS();
  ShellFeature& shell = v8g->_server.getFeature<ShellFeature>();

  shell.setExitCode(static_cast<int>(code));

  isolate->TerminateExecution();

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes global JavaScript variables
////////////////////////////////////////////////////////////////////////////////

void V8ShellFeature::initGlobals() {
  ConsoleFeature& console = server().getFeature<ConsoleFeature>();
  auto context = _isolate->GetCurrentContext();

  // string functions
  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "NORMALIZE_STRING"),
      v8::FunctionTemplate::New(_isolate, JS_NormalizeString)->GetFunction(context).FromMaybe(v8::Local<v8::Function>()));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "COMPARE_STRING"),
      v8::FunctionTemplate::New(_isolate, JS_CompareString)->GetFunction(context).FromMaybe(v8::Local<v8::Function>()));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "ARANGODB_CLIENT_VERSION"),
      v8::FunctionTemplate::New(_isolate, JS_VersionClient)->GetFunction(context).FromMaybe(v8::Local<v8::Function>()));

  // is quite
  TRI_AddGlobalVariableVocbase(_isolate,
                               TRI_V8_ASCII_STRING(_isolate, "ARANGO_QUIET"),
                               v8::Boolean::New(_isolate, console.quiet()));

  auto ctx = ArangoGlobalContext::CONTEXT;

  if (ctx == nullptr) {
    LOG_TOPIC("b754a", FATAL, arangodb::Logger::FIXME) << "failed to get global context";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(_startupDirectory, "javascript.startup-directory", true);
  ctx->normalizePath(_moduleDirectories, "javascript.module-directory", false);

  V8SecurityFeature& v8security = server().getFeature<V8SecurityFeature>();

  // try to append the current version name to the startup directory,
  // so instead of "/path/to/js" we will get "/path/to/js/3.4.0"
  std::string const versionAppendix =
      std::regex_replace(rest::Version::getServerVersion(), std::regex("-.*$"),
                         "");
  std::string versionedPath =
      basics::FileUtils::buildFilename(_startupDirectory, versionAppendix);

  LOG_TOPIC("5095d", DEBUG, Logger::V8)
      << "checking for existence of version-specific startup-directory '"
      << versionedPath << "'";
  if (basics::FileUtils::isDirectory(versionedPath)) {
    // version-specific js path exists!
    _startupDirectory = versionedPath;
  }
  v8security.addToInternalAllowList(_startupDirectory, FSAccessType::READ);

  for (auto& it : _moduleDirectories) {
    versionedPath = basics::FileUtils::buildFilename(it, versionAppendix);

    LOG_TOPIC("2abe3", DEBUG, Logger::V8)
        << "checking for existence of version-specific module-directory '"
        << versionedPath << "'";
    if (basics::FileUtils::isDirectory(versionedPath)) {
      // version-specific js path exists!
      it = versionedPath;
    }
    v8security.addToInternalAllowList(it, FSAccessType::READ);  // expand
  }

  LOG_TOPIC("930d9", DEBUG, Logger::V8)
      << "effective startup-directory is '" << _startupDirectory
      << "', effective module-directory is " << _moduleDirectories;

  // initialize standard modules
  std::vector<std::string> directories;
  directories.insert(directories.end(), _moduleDirectories.begin(),
                     _moduleDirectories.end());
  directories.emplace_back(_startupDirectory);
  if (!_nodeModulesDirectory.empty()) {
    directories.emplace_back(_nodeModulesDirectory);
  }

  std::string modules = "";
  std::string sep = "";

  for (auto const& directory : directories) {
    modules += sep;
    sep = ";";

    modules += FileUtils::buildFilename(directory, "client/modules") + sep +
               FileUtils::buildFilename(directory, "common/modules") + sep +
               FileUtils::buildFilename(directory, "node");
  }

  if (_currentModuleDirectory) {
    modules += sep + FileUtils::currentDirectory().result();
    v8security.addToInternalAllowList(FileUtils::currentDirectory().result(),
                                      FSAccessType::READ);
  }

  v8security.dumpAccessLists();

  // we take the last entry in _startupDirectory as global path;
  // all the other entries are only used for the modules

  TRI_InitV8Buffer(_isolate);
  TRI_InitV8Utils(_isolate, context, _startupDirectory, modules);
  TRI_InitV8Deadline(_isolate);
  TRI_InitV8Shell(_isolate);

  // pager functions (overwrite existing SYS_OUTPUT from InitV8Utils)
  v8::Local<v8::Value> consoleWrapped = v8::External::New(_isolate, &console);

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "SYS_OUTPUT"),
      v8::FunctionTemplate::New(_isolate, JS_PagerOutput, consoleWrapped)->GetFunction(context).FromMaybe(v8::Local<v8::Function>()));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "SYS_START_PAGER"),
      v8::FunctionTemplate::New(_isolate, JS_StartOutputPager, consoleWrapped)->GetFunction(context).FromMaybe(v8::Local<v8::Function>()));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "SYS_STOP_PAGER"),
      v8::FunctionTemplate::New(_isolate, JS_StopOutputPager, consoleWrapped)->GetFunction(context).FromMaybe(v8::Local<v8::Function>()));
}

void V8ShellFeature::initMode(ShellFeature::RunMode runMode,
                              std::vector<std::string> const& positionals) {
  // add positional arguments
  v8::Handle<v8::Array> p = v8::Array::New(_isolate, (int)positionals.size());
  auto context = _isolate->GetCurrentContext();
  for (uint32_t i = 0; i < positionals.size(); ++i) {
    p->Set(context, i, TRI_V8_STD_STRING(_isolate, positionals[i])).FromMaybe(false);
  }

  TRI_AddGlobalVariableVocbase(_isolate,
                               TRI_V8_ASCII_STRING(_isolate, "ARGUMENTS"), p);
 
  std::string const& binaryPath = ArangoGlobalContext::CONTEXT->getBinaryPath();
  TRI_AddGlobalVariableVocbase(_isolate, TRI_V8_ASCII_STRING(_isolate, "ARANGOSH_PATH"),
                               TRI_V8_STD_STRING(_isolate, binaryPath));

  // set mode flags
  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "IS_EXECUTE_SCRIPT"),
      v8::Boolean::New(_isolate, runMode == ShellFeature::RunMode::EXECUTE_SCRIPT));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "IS_EXECUTE_STRING"),
      v8::Boolean::New(_isolate, runMode == ShellFeature::RunMode::EXECUTE_STRING));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "IS_CHECK_SCRIPT"),
      v8::Boolean::New(_isolate, runMode == ShellFeature::RunMode::CHECK_SYNTAX));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING(_isolate, "IS_UNIT_TESTS"),
      v8::Boolean::New(_isolate, runMode == ShellFeature::RunMode::UNIT_TESTS));

  TRI_AddGlobalVariableVocbase(_isolate,
                               TRI_V8_ASCII_STRING(_isolate, "IS_JS_LINT"),
                               v8::Boolean::New(_isolate, runMode == ShellFeature::RunMode::JSLINT));

  TRI_AddGlobalFunctionVocbase(_isolate,
                               TRI_V8_ASCII_STRING(_isolate, "SYS_EXIT"), JS_Exit);
}

void V8ShellFeature::loadModules(ShellFeature::RunMode runMode) {
  auto context = _isolate->GetCurrentContext();

  JSLoader loader;
  loader.setDirectory(_startupDirectory);

  // load all init files
  std::vector<std::string> files;
  files.reserve(16);

  files.push_back("common/bootstrap/scaffolding.js");
  files.push_back("common/bootstrap/modules/internal.js");  // deps: -
  files.push_back("common/bootstrap/errors.js");            // deps: internal
  files.push_back("client/bootstrap/modules/internal.js");  // deps: internal
  files.push_back("common/bootstrap/modules/vm.js");        // deps: internal
  files.push_back("common/bootstrap/modules/console.js");   // deps: internal
  files.push_back("common/bootstrap/modules/assert.js");    // deps: -
  files.push_back("common/bootstrap/modules/buffer.js");    // deps: internal
  files.push_back("common/bootstrap/modules/fs.js");  // deps: internal, buffer (hidden)
  files.push_back("common/bootstrap/modules/path.js");     // deps: internal, fs
  files.push_back("common/bootstrap/modules/events.js");   // deps: -
  files.push_back("common/bootstrap/modules/process.js");  // deps: internal,
                                                           // fs, events,
                                                           // console
  files.push_back("common/bootstrap/modules.js");  // must come last before patches

  files.push_back("client/" + _clientModule);  // needs internal

  for (auto const& file : files) {
    switch (loader.loadScript(_isolate, context, file, nullptr)) {
      case JSLoader::eSuccess:
        LOG_TOPIC("edc8d", TRACE, arangodb::Logger::FIXME)
            << "loaded JavaScript file '" << file << "'";
        break;
      case JSLoader::eFailLoad:
        LOG_TOPIC("022a8", FATAL, arangodb::Logger::FIXME)
            << "cannot load JavaScript file '" << file << "'";
        FATAL_ERROR_EXIT();
        break;
      case JSLoader::eFailExecute:
        LOG_TOPIC("22385", FATAL, arangodb::Logger::FIXME)
            << "error during execution of JavaScript file '" << file << "'";
        FATAL_ERROR_EXIT();
        break;
    }
  }
}

}  // namespace arangodb
