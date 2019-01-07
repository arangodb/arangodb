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

#include "ScriptFeature.h"

#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

ScriptFeature::ScriptFeature(application_features::ApplicationServer& server, int* result)
    : ApplicationFeature(server, "Script"), _result(result) {
  setOptional(true);
  startsAfter("AgencyPhase");
}

void ScriptFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");

  options->addOption("--javascript.script-parameter", "script parameter",
                     new VectorParameter<StringParameter>(&_scriptParameters));
}

void ScriptFeature::start() {
  auto server = ApplicationServer::getFeature<ServerFeature>("Server");
  auto operationMode = server->operationMode();

  if (operationMode != OperationMode::MODE_SCRIPT) {
    return;
  }

  LOG_TOPIC(TRACE, Logger::STARTUP) << "server about to run scripts";
  *_result = runScript(server->scripts());
}

int ScriptFeature::runScript(std::vector<std::string> const& scripts) {
  bool ok = false;
  auto* sysDbFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto database = sysDbFeature->use();
  V8Context* context = V8DealerFeature::DEALER->enterContext(database.get(), true);

  if (context == nullptr) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "cannot acquire V8 context";
    FATAL_ERROR_EXIT();
  }

  TRI_DEFER(V8DealerFeature::DEALER->exitContext(context));

  auto isolate = context->_isolate;

  {
    v8::HandleScope globalScope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::Context::Scope contextScope(localContext);
      for (auto const& script : scripts) {
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "executing script '" << script << "'";
        bool r = TRI_ExecuteGlobalJavaScriptFile(isolate, script.c_str(), true);

        if (!r) {
          LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
              << "cannot load script '" << script << "', giving up";
          FATAL_ERROR_EXIT();
        }
      }

      v8::TryCatch tryCatch;
      // run the garbage collection for at most 30 seconds
      TRI_RunGarbageCollectionV8(isolate, 30.0);

      // parameter array
      v8::Handle<v8::Array> params = v8::Array::New(isolate);

      params->Set(0, TRI_V8_STD_STRING(isolate, scripts[scripts.size() - 1]));

      for (size_t i = 0; i < _scriptParameters.size(); ++i) {
        params->Set((uint32_t)(i + 1), TRI_V8_STD_STRING(isolate, _scriptParameters[i]));
      }

      // call main
      v8::Handle<v8::String> mainFuncName =
          TRI_V8_ASCII_STRING(isolate, "main");
      v8::Handle<v8::Function> main =
          v8::Handle<v8::Function>::Cast(localContext->Global()->Get(mainFuncName));

      if (main.IsEmpty() || main->IsUndefined()) {
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
            << "no main function defined, giving up";
        FATAL_ERROR_EXIT();
      } else {
        v8::Handle<v8::Value> args[] = {params};

        try {
          v8::Handle<v8::Value> result = main->Call(main, 1, args);

          if (tryCatch.HasCaught()) {
            if (tryCatch.CanContinue()) {
              TRI_LogV8Exception(isolate, &tryCatch);
            } else {
              // will stop, so need for v8g->_canceled = true;
              TRI_ASSERT(!ok);
            }
          } else {
            ok = TRI_ObjectToDouble(result) == 0;
          }
        } catch (arangodb::basics::Exception const& ex) {
          LOG_TOPIC(ERR, arangodb::Logger::FIXME)
              << "caught exception " << TRI_errno_string(ex.code()) << ": "
              << ex.what();
          ok = false;
        } catch (std::bad_alloc const&) {
          LOG_TOPIC(ERR, arangodb::Logger::FIXME)
              << "caught exception " << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
          ok = false;
        } catch (...) {
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception";
          ok = false;
        }
      }
    }

    localContext->Exit();
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace arangodb
