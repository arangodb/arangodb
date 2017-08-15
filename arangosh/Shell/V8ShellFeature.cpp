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

#include "V8ShellFeature.h"

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "Shell/V8ClientConnection.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

V8ShellFeature::V8ShellFeature(application_features::ApplicationServer* server,
                               std::string const& name)
    : ApplicationFeature(server, "V8Shell"),
      _startupDirectory("js"),
      _currentModuleDirectory(true),
      _gcInterval(50),
      _name(name),
      _isolate(nullptr),
      _console(nullptr) {
  requiresElevatedPrivileges(false);
  setOptional(false);

  startsAfter("Logger");
  startsAfter("Console");
  startsAfter("V8Platform");
}

void V8ShellFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");

  options->addHiddenOption("--javascript.startup-directory",
                           "startup paths containing the Javascript files",
                           new StringParameter(&_startupDirectory));

  options->addHiddenOption(
      "--javascript.module-directory",
      "additional paths containing JavaScript modules",
      new VectorParameter<StringParameter>(&_moduleDirectory));

  options->addOption("--javascript.current-module-directory",
                     "add current directory to module path",
                     new BooleanParameter(&_currentModuleDirectory));

  options->addOption(
      "--javascript.gc-interval",
      "request-based garbage collection interval (each n.th commands)",
      new UInt64Parameter(&_gcInterval));
}

void V8ShellFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  if (_startupDirectory.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "no 'javascript.startup-directory' has been supplied, giving up";
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC(DEBUG, Logger::V8) << "using Javascript startup files at '"
                               << _startupDirectory << "'";

  if (!_moduleDirectory.empty()) {
    LOG_TOPIC(DEBUG, Logger::V8) << "using Javascript modules at '"
                                 << StringUtils::join(_moduleDirectory, ";")
                                 << "'";
  }
}

void V8ShellFeature::start() {
  _console =
      application_features::ApplicationServer::getFeature<ConsoleFeature>(
          "Console");
  auto platform =
      application_features::ApplicationServer::getFeature<V8PlatformFeature>(
          "V8Platform");

  _isolate = platform->createIsolate();

  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  // create the global template
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(_isolate);

  // create the context
  _context.Reset(_isolate, v8::Context::New(_isolate, nullptr, global));

  auto context = v8::Local<v8::Context>::New(_isolate, _context);

  if (context.IsEmpty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "cannot initialize V8 engine";
    FATAL_ERROR_EXIT();
  }

  // fill the global object
  v8::Context::Scope context_scope{context};

  v8::Handle<v8::Object> globalObj = context->Global();
  globalObj->Set(TRI_V8_ASCII_STRING2(_isolate, "GLOBAL"), globalObj);
  globalObj->Set(TRI_V8_ASCII_STRING2(_isolate, "global"), globalObj);
  globalObj->Set(TRI_V8_ASCII_STRING2(_isolate, "root"), globalObj);

  initGlobals();
}

void V8ShellFeature::unprepare() {
  // turn off memory allocation failures before we move into V8 code
  TRI_DisallowMemoryFailures();

  {
    v8::Locker locker{_isolate};

    v8::Isolate::Scope isolate_scope(_isolate);
    v8::HandleScope handle_scope(_isolate);

    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(_isolate, _context);

    v8::Context::Scope context_scope{context};

    // clear globals to free memory
    auto globals = _isolate->GetCurrentContext()->Global();
    v8::Handle<v8::Array> names = globals->GetOwnPropertyNames();
    uint32_t const n = names->Length();
    for (uint32_t i = 0; i < n; ++i) {
      globals->Delete(names->Get(i));
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
  
  // turn on memory allocation failures again
  TRI_AllowMemoryFailures();
}

bool V8ShellFeature::printHello(V8ClientConnection* v8connection) {
  bool promptError = false;

  // for the banner see
  // http://www.network-science.de/ascii/   Font: ogre

  if (!_console->quiet()) {
    std::string g = ShellColorsFeature::SHELL_COLOR_GREEN;
    std::string r = ShellColorsFeature::SHELL_COLOR_RED;
    std::string z = ShellColorsFeature::SHELL_COLOR_RESET;

    if (!_console->colors()) {
      g = "";
      r = "";
      z = "";
    }

    // clang-format off

    _console->printLine("");
    _console->printLine(g + "                                  "     + r + "     _     " + z);
    _console->printLine(g + "  __ _ _ __ __ _ _ __   __ _  ___ "     + r + " ___| |__  " + z);
    _console->printLine(g + " / _` | '__/ _` | '_ \\ / _` |/ _ \\"   + r + "/ __| '_ \\ " + z);
    _console->printLine(g + "| (_| | | | (_| | | | | (_| | (_) "     + r + "\\__ \\ | | |" + z);
    _console->printLine(g + " \\__,_|_|  \\__,_|_| |_|\\__, |\\___/" + r + "|___/_| |_|" + z);
    _console->printLine(g + "                       |___/      "     + r + "           " + z);
    _console->printLine("");

    // clang-format on

    std::ostringstream s;

    s << "arangosh (" << rest::Version::getVerboseVersionString() << ")\n"
      << "Copyright (c) ArangoDB GmbH";

    _console->printLine(s.str());
    _console->printLine("");

    _console->printWelcomeInfo();

    if (v8connection != nullptr) {
      if (v8connection->isConnected() &&
          v8connection->lastHttpReturnCode() == (int)rest::ResponseCode::OK) {
        std::ostringstream is;

        is << "Connected to ArangoDB '" << v8connection->endpointSpecification()
           << "' version: " << v8connection->version() << " ["
           << v8connection->mode() << "], database: '"
           << v8connection->databaseName() << "', username: '"
           << v8connection->username() << "'";

        _console->printLine(is.str());
      } else {
        std::ostringstream is;

        is << "Could not connect to endpoint '"
           << v8connection->endpointSpecification() << "', database: '"
           << v8connection->databaseName() << "', username: '"
           << v8connection->username() << "'";

        _console->printErrorLine(is.str());

        if (!v8connection->lastErrorMessage().empty()) {
          std::ostringstream is2;

          is2 << "Error message: '" << v8connection->lastErrorMessage() << "'";

          _console->printErrorLine(is2.str());
        }

        promptError = true;
      }

      _console->printLine("");
    }
  }

  return promptError;
}

// the result is wrapped in a Javascript variable SYS_ARANGO
V8ClientConnection* V8ShellFeature::setup(
    v8::Local<v8::Context>& context, bool createConnection,
    std::vector<std::string> const& positionals, bool* promptError) {
  std::unique_ptr<V8ClientConnection> v8connection;

  ClientFeature* client = nullptr;

  if (createConnection) {
    client = dynamic_cast<ClientFeature*>(server()->feature("Client"));

    if (client != nullptr && client->isEnabled()) {
      auto connection = client->createConnection();
      v8connection = std::make_unique<V8ClientConnection>(
          connection, client->databaseName(), client->username(),
          client->password(), client->requestTimeout());
    } else {
      client = nullptr;
    }
  }

  initMode(ShellFeature::RunMode::INTERACTIVE, positionals);

  if (createConnection && client != nullptr) {
    v8connection->initServer(_isolate, context, client);
  }

  bool pe = printHello(v8connection.get());
  loadModules(ShellFeature::RunMode::INTERACTIVE);

  if (promptError != nullptr) {
    *promptError = pe;
  }

  return v8connection.release();
}

int V8ShellFeature::runShell(std::vector<std::string> const& positionals) {
  v8::Locker locker{_isolate};
  v8::Isolate::Scope isolate_scope(_isolate);

  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  bool promptError;
  auto v8connection = setup(context, true, positionals, &promptError);
  std::unique_ptr<V8ClientConnection> guard(v8connection);

  V8LineEditor v8LineEditor(_isolate, context, "." + _name + ".history");

  if (v8connection != nullptr) {
    v8LineEditor.setSignalFunction(
        [&v8connection]() { v8connection->setInterrupted(true); });
  }

  v8LineEditor.open(_console->autoComplete());

  v8::Local<v8::String> name(
      TRI_V8_ASCII_STRING2(_isolate, TRI_V8_SHELL_COMMAND_NAME));

  uint64_t nrCommands = 0;

  ClientFeature* client = server()->getFeature<ClientFeature>("Client");

  if (!client->isEnabled()) {
    client = nullptr;
  }

  bool const isBatch = isatty(STDIN_FILENO) == 0;
  bool lastEmpty = isBatch;

  while (true) {
    _console->setPromptError(promptError);
    auto prompt = _console->buildPrompt(client);

    ShellBase::EofType eof = ShellBase::EOF_NONE;
    std::string input =
        v8LineEditor.prompt(prompt._colored, prompt._plain, eof);

    if (eof == ShellBase::EOF_FORCE_ABORT || (eof == ShellBase::EOF_ABORT && lastEmpty)) {
      break;
    }

    if (input.empty()) {
      promptError = false;
      lastEmpty = true;
      continue;
    }
    lastEmpty = isBatch;

    _console->log(prompt._plain + input + "\n");

    std::string i = StringUtils::trim(input);

    if (i == "exit" || i == "quit" || i == "exit;" || i == "quit;") {
      break;
    }

    if (i == "help" || i == "help;") {
      input = "help()";
    }

    v8LineEditor.addHistory(input);

    v8::TryCatch tryCatch;

    _console->startPager();

    // assume the command succeeds
    promptError = false;

    // execute command and register its result in __LAST__
    v8LineEditor.setExecutingCommand(true);

    v8::Handle<v8::Value> v = TRI_ExecuteJavaScriptString(
        _isolate, context, TRI_V8_STRING2(_isolate, input.c_str()), name, true);

    v8LineEditor.setExecutingCommand(false);

    if (v.IsEmpty()) {
      context->Global()->Set(TRI_V8_ASCII_STRING2(_isolate, "_last"),
                             v8::Undefined(_isolate));
    } else {
      context->Global()->Set(TRI_V8_ASCII_STRING2(_isolate, "_last"), v);
    }

    // command failed
    if (tryCatch.HasCaught()) {
      std::string exception;

      if (!tryCatch.CanContinue() || tryCatch.HasTerminated()) {
        exception = "command locally aborted\n";
      } else {
        exception = TRI_StringifyV8Exception(_isolate, &tryCatch);
      }

      _console->printErrorLine(exception);
      _console->log(exception);

      // this will change the prompt for the next round
      promptError = true;
    }

    if (v8connection != nullptr) {
      v8connection->setInterrupted(false);
    }

    _console->stopPager();
    _console->printLine("");

    _console->log("\n");

    // make sure the last command result makes it into the log file
    _console->flushLog();

    // gc
    if (++nrCommands >= _gcInterval ||
        V8PlatformFeature::isOutOfMemory(_isolate)) {
      nrCommands = 0;
      TRI_RunGarbageCollectionV8(_isolate, 500.0);

      // needs to be reset after the garbage collection
      V8PlatformFeature::resetOutOfMemory(_isolate);
    }
  }
     
  if (!_console->quiet()) {
    _console->printLine("");
    _console->printByeBye();
  }

  return promptError ? TRI_ERROR_INTERNAL : TRI_ERROR_NO_ERROR;
}

bool V8ShellFeature::runScript(std::vector<std::string> const& files,
                               std::vector<std::string> const& positionals,
                               bool execute) {
  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  auto v8connection = setup(context, execute, positionals);
  std::unique_ptr<V8ClientConnection> guard(v8connection);

  bool ok = true;

  for (auto const& file : files) {
    if (!FileUtils::exists(file)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error: Javascript file not found: '" << file << "'";
      ok = false;
      continue;
    }

    if (execute) {
      v8::TryCatch tryCatch;

      v8::Handle<v8::String> name = TRI_V8_STD_STRING2(_isolate, file);
      v8::Handle<v8::Value> args[] = {name};
      v8::Handle<v8::Value> filename = args[0];

      v8::Handle<v8::Object> current = _isolate->GetCurrentContext()->Global();

      auto oldFilename =
          current->Get(TRI_V8_ASCII_STRING2(_isolate, "__filename"));

      current->ForceSet(TRI_V8_ASCII_STRING2(_isolate, "__filename"), filename);

      auto oldDirname =
          current->Get(TRI_V8_ASCII_STRING2(_isolate, "__dirname"));

      auto dirname = FileUtils::dirname(TRI_ObjectToString(filename));

      current->ForceSet(TRI_V8_ASCII_STRING2(_isolate, "__dirname"),
                        TRI_V8_STD_STRING2(_isolate, dirname));

      ok = TRI_ExecuteGlobalJavaScriptFile(_isolate, file.c_str(), true);

      // restore old values for __dirname and __filename
      if (oldFilename.IsEmpty() || oldFilename->IsUndefined()) {
        current->Delete(TRI_V8_ASCII_STRING2(_isolate, "__filename"));
      } else {
        current->ForceSet(TRI_V8_ASCII_STRING2(_isolate, "__filename"),
                          oldFilename);
      }

      if (oldDirname.IsEmpty() || oldDirname->IsUndefined()) {
        current->Delete(TRI_V8_ASCII_STRING2(_isolate, "__dirname"));
      } else {
        current->ForceSet(TRI_V8_ASCII_STRING2(_isolate, "__dirname"),
                          oldDirname);
      }

      if (tryCatch.HasCaught()) {
        std::string exception(TRI_StringifyV8Exception(_isolate, &tryCatch));
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << exception;
        ok = false;
      }
    } else {
      ok = TRI_ParseJavaScriptFile(_isolate, file.c_str(), true);
    }
  }

  _console->flushLog();

  return ok;
}

bool V8ShellFeature::runString(std::vector<std::string> const& strings,
                               std::vector<std::string> const& positionals) {
  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  auto v8connection = setup(context, true, positionals);
  std::unique_ptr<V8ClientConnection> guard(v8connection);

  bool ok = true;

  for (auto const& script : strings) {
    v8::TryCatch tryCatch;

    v8::Handle<v8::Value> result = TRI_ExecuteJavaScriptString(
        _isolate, context, TRI_V8_STD_STRING2(_isolate, script),
        TRI_V8_ASCII_STRING2(_isolate, "(command-line)"), false);

    if (tryCatch.HasCaught()) {
      std::string exception(TRI_StringifyV8Exception(_isolate, &tryCatch));
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << exception;
      ok = false;
    } else {
      // check return value of script
      if (result->IsNumber()) {
        int64_t intResult = TRI_ObjectToInt64(result);

        if (intResult != 0) {
          ok = false;
        }
      }
    }
  }

  _console->flushLog();

  return ok;
}

bool V8ShellFeature::jslint(std::vector<std::string> const& files) {
  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  setup(context, false, {});

  bool ok = true;

  // set-up jslint files array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(_isolate);

  uint32_t i = 0;
  for (auto& file : files) {
    if (!FileUtils::exists(file)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error: Javascript file not found: '" << file << "'";
      ok = false;
      continue;
    }

    sysTestFiles->Set(i, TRI_V8_STD_STRING2(_isolate, file));
    ++i;
  }

  context->Global()->Set(TRI_V8_ASCII_STRING2(_isolate, "SYS_UNIT_TESTS"),
                         sysTestFiles);

  context->Global()->Set(
      TRI_V8_ASCII_STRING2(_isolate, "SYS_UNIT_TESTS_RESULT"),
      v8::True(_isolate));

  // run tests
  auto input = TRI_V8_ASCII_STRING2(
      _isolate, "require(\"jslint\").runCommandLineTests({});");

  auto name = TRI_V8_ASCII_STRING2(_isolate, TRI_V8_SHELL_COMMAND_NAME);

  v8::TryCatch tryCatch;
  TRI_ExecuteJavaScriptString(_isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << TRI_StringifyV8Exception(_isolate, &tryCatch);
    ok = false;
  } else {
    bool res = TRI_ObjectToBoolean(context->Global()->Get(
        TRI_V8_ASCII_STRING2(_isolate, "SYS_UNIT_TESTS_RESULT")));

    ok = ok && res;
  }

  return ok;
}

bool V8ShellFeature::runUnitTests(std::vector<std::string> const& files,
                                  std::vector<std::string> const& positionals) {
  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  auto v8connection = setup(context, true, positionals);
  std::unique_ptr<V8ClientConnection> guard(v8connection);

  bool ok = true;

  // set-up unit tests array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(_isolate);

  uint32_t i = 0;

  for (auto const& file : files) {
    if (!FileUtils::exists(file)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error: Javascript file not found: '" << file << "'";
      ok = false;
      continue;
    }

    sysTestFiles->Set(i, TRI_V8_STD_STRING2(_isolate, file));
    ++i;
  }

  TRI_AddGlobalVariableVocbase(_isolate,
                               TRI_V8_ASCII_STRING2(_isolate, "SYS_UNIT_TESTS"),
                               sysTestFiles);

  // do not use TRI_AddGlobalVariableVocBase because it creates read-only
  // variables!!
  context->Global()->Set(
      TRI_V8_ASCII_STRING2(_isolate, "SYS_UNIT_TESTS_RESULT"),
      v8::True(_isolate));

  // run tests
  auto input = TRI_V8_ASCII_STRING2(
      _isolate, "require(\"@arangodb/testrunner\").runCommandLineTests();");

  auto name = TRI_V8_ASCII_STRING2(_isolate, TRI_V8_SHELL_COMMAND_NAME);

  v8::TryCatch tryCatch;
  TRI_ExecuteJavaScriptString(_isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    std::string exception(TRI_StringifyV8Exception(_isolate, &tryCatch));
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << exception;
    ok = false;
  } else {
    bool res = TRI_ObjectToBoolean(context->Global()->Get(
        TRI_V8_ASCII_STRING2(_isolate, "SYS_UNIT_TESTS_RESULT")));

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
    console->print(TRI_ObjectToString(args[i]));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StartOutputPager(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ConsoleFeature* console = static_cast<ConsoleFeature*>(wrap->Value());

  if (console->pager()) {
    console->print("Using pager already.\n");
  } else {
    console->setPager(true);
    console->print(std::string(std::string("Using pager ") +
                               console->pagerCommand() +
                               " for output buffering.\n"));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StopOutputPager(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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

static void JS_NormalizeString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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

  v8::String::Value left(args[0]);
  v8::String::Value right(args[1]);

  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(
      *left, left.length(), *right, right.length());

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
    details = TRI_ObjectToBoolean(args[0]);
  }

  if (!details) {
    // return version string
    TRI_V8_RETURN(TRI_V8_ASCII_STRING(ARANGODB_VERSION));
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
/// @brief initializes global Javascript variables
////////////////////////////////////////////////////////////////////////////////

void V8ShellFeature::initGlobals() {
  auto context = _isolate->GetCurrentContext();

  // set pretty print default
  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "PRETTY_PRINT"),
      v8::Boolean::New(_isolate, _console->prettyPrint()));

  // add colors for print.js
  TRI_AddGlobalVariableVocbase(_isolate,
                               TRI_V8_ASCII_STRING2(_isolate, "COLOR_OUTPUT"),
                               v8::Boolean::New(_isolate, _console->colors()));

  // string functions
  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "NORMALIZE_STRING"),
      v8::FunctionTemplate::New(_isolate, JS_NormalizeString)->GetFunction());

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "COMPARE_STRING"),
      v8::FunctionTemplate::New(_isolate, JS_CompareString)->GetFunction());

  TRI_AddGlobalVariableVocbase(
      _isolate, 
      TRI_V8_ASCII_STRING2(_isolate, "ARANGODB_CLIENT_VERSION"),
      v8::FunctionTemplate::New(_isolate, JS_VersionClient)->GetFunction());

  // is quite
  TRI_AddGlobalVariableVocbase(_isolate, 
                               TRI_V8_ASCII_STRING2(_isolate, "ARANGO_QUIET"),
                               v8::Boolean::New(_isolate, _console->quiet()));

  auto ctx = ArangoGlobalContext::CONTEXT;

  if (ctx == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to get global context.  ";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(_startupDirectory, "javascript.startup-directory", true);
  ctx->normalizePath(_moduleDirectory, "javascript.module-directory", false);

  // initialize standard modules
  std::vector<std::string> directories;
  directories.insert(directories.end(), _moduleDirectory.begin(),
                     _moduleDirectory.end());
  directories.emplace_back(_startupDirectory);

  std::string modules = "";
  std::string sep = "";

  for (auto directory : directories) {
    modules += sep;
    sep = ";";

    modules += FileUtils::buildFilename(directory, "client/modules") + sep +
               FileUtils::buildFilename(directory, "common/modules") + sep +
               FileUtils::buildFilename(directory, "node");
  }

  if (_currentModuleDirectory) {
    modules += sep + FileUtils::currentDirectory().result();
  }

  // we take the last entry in _startupDirectory as global path;
  // all the other entries are only used for the modules

  TRI_InitV8Buffer(_isolate, context);
  TRI_InitV8Utils(_isolate, context, _startupDirectory, modules);
  TRI_InitV8Shell(_isolate, context);

  // pager functions (overwrite existing SYS_OUTPUT from InitV8Utils)
  v8::Local<v8::Value> console = v8::External::New(_isolate, _console);

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "SYS_OUTPUT"),
      v8::FunctionTemplate::New(_isolate, JS_PagerOutput, console)
          ->GetFunction());

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "SYS_START_PAGER"),
      v8::FunctionTemplate::New(_isolate, JS_StartOutputPager, console)
          ->GetFunction());

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "SYS_STOP_PAGER"),
      v8::FunctionTemplate::New(_isolate, JS_StopOutputPager, console)
          ->GetFunction());
}

void V8ShellFeature::initMode(ShellFeature::RunMode runMode,
                              std::vector<std::string> const& positionals) {
  // add positional arguments
  v8::Handle<v8::Array> p = v8::Array::New(_isolate, (int)positionals.size());

  for (uint32_t i = 0; i < positionals.size(); ++i) {
    p->Set(i, TRI_V8_STD_STRING2(_isolate, positionals[i]));
  }

  TRI_AddGlobalVariableVocbase(_isolate,
                               TRI_V8_ASCII_STRING2(_isolate, "ARGUMENTS"), p);

  // set mode flags
  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "IS_EXECUTE_SCRIPT"),
      v8::Boolean::New(_isolate,
                       runMode == ShellFeature::RunMode::EXECUTE_SCRIPT));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "IS_EXECUTE_STRING"),
      v8::Boolean::New(_isolate,
                       runMode == ShellFeature::RunMode::EXECUTE_STRING));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "IS_CHECK_SCRIPT"),
      v8::Boolean::New(_isolate,
                       runMode == ShellFeature::RunMode::CHECK_SYNTAX));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "IS_UNIT_TESTS"),
      v8::Boolean::New(_isolate, runMode == ShellFeature::RunMode::UNIT_TESTS));

  TRI_AddGlobalVariableVocbase(
      _isolate, TRI_V8_ASCII_STRING2(_isolate, "IS_JS_LINT"),
      v8::Boolean::New(_isolate, runMode == ShellFeature::RunMode::JSLINT));
}

void V8ShellFeature::loadModules(ShellFeature::RunMode runMode) {
  auto context = _isolate->GetCurrentContext();

  JSLoader loader;
  loader.setDirectory(_startupDirectory);

  // load all init files
  std::vector<std::string> files;

  files.push_back("common/bootstrap/scaffolding.js");
  files.push_back("common/bootstrap/modules/internal.js");  // deps: -
  files.push_back("common/bootstrap/errors.js");            // deps: internal
  files.push_back("client/bootstrap/modules/internal.js");  // deps: internal
  files.push_back("common/bootstrap/modules/vm.js");        // deps: internal
  files.push_back("common/bootstrap/modules/console.js");   // deps: internal
  files.push_back("common/bootstrap/modules/assert.js");    // deps: -
  files.push_back("common/bootstrap/modules/buffer.js");    // deps: internal
  files.push_back(
      "common/bootstrap/modules/fs.js");  // deps: internal, buffer (hidden)
  files.push_back("common/bootstrap/modules/path.js");     // deps: internal, fs
  files.push_back("common/bootstrap/modules/events.js");   // deps: -
  files.push_back("common/bootstrap/modules/process.js");  // deps: internal,
                                                           // fs, events,
                                                           // console
  files.push_back(
      "common/bootstrap/modules.js");  // must come last before patches

  files.push_back("client/client.js");  // needs internal

  for (size_t i = 0; i < files.size(); ++i) {
    switch (loader.loadScript(_isolate, context, files[i], nullptr)) {
      case JSLoader::eSuccess:
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "loaded JavaScript file '" << files[i] << "'";
        break;
      case JSLoader::eFailLoad:
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "cannot load JavaScript file '" << files[i] << "'";
        FATAL_ERROR_EXIT();
        break;
      case JSLoader::eFailExecute:
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "error during execution of JavaScript file '" << files[i]
                   << "'";
        FATAL_ERROR_EXIT();
        break;
    }
  }
}
