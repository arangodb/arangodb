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

#include "UnitTestsFeature.h"

#include "Basics/messages.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/ConsoleThread.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

UnitTestsFeature::UnitTestsFeature(application_features::ApplicationServer* server, int* result)
    : ApplicationFeature(server, "UnitTests"),
      _result(result) {
  startsAfter("Nonce");
  startsAfter("Server");
  startsAfter("GeneralServer");
  startsAfter("Bootstrap");
}

void UnitTestsFeature::start() {
  auto server = ApplicationServer::getFeature<ServerFeature>("Server");
  auto operationMode = server->operationMode();

  if (operationMode != OperationMode::MODE_UNITTESTS) {
    return;
  }

  LOG_TOPIC(TRACE, Logger::STARTUP) << "server about to run unit-tests";
  *_result = runUnitTests(server->unitTests());
}

int UnitTestsFeature::runUnitTests(std::vector<std::string> const& unitTests) {
  DatabaseFeature* database = 
      ApplicationServer::getFeature<DatabaseFeature>("Database");
  V8Context* context =
      V8DealerFeature::DEALER->enterContext(database->systemDatabase(), true);

  if (context == nullptr) {
    LOG(FATAL) << "cannot acquire V8 context";
    FATAL_ERROR_EXIT();
  }

  TRI_DEFER(V8DealerFeature::DEALER->exitContext(context));

  auto isolate = context->_isolate;

  bool ok = false;
  {
    v8::HandleScope scope(isolate);
    v8::TryCatch tryCatch;

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::Context::Scope contextScope(localContext);
      // set-up unit tests array
      v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

      for (size_t i = 0; i < unitTests.size(); ++i) {
        sysTestFiles->Set((uint32_t)i, TRI_V8_STD_STRING(unitTests[i]));
      }

      localContext->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"),
                                  sysTestFiles);
      localContext->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"),
                                  v8::True(isolate));

      v8::Local<v8::String> name(
          TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME));

      // run tests
      auto input = TRI_V8_ASCII_STRING(
          "require(\"@arangodb/testrunner\").runCommandLineTests();");
      TRI_ExecuteJavaScriptString(isolate, localContext, input, name, true);

      if (tryCatch.HasCaught()) {
        if (tryCatch.CanContinue()) {
          std::cerr << TRI_StringifyV8Exception(isolate, &tryCatch);
        } else {
          // will stop, so need for v8g->_canceled = true;
          TRI_ASSERT(!ok);
        }
      } else {
        ok = TRI_ObjectToBoolean(localContext->Global()->Get(
            TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
      }
    }
    localContext->Exit();
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
