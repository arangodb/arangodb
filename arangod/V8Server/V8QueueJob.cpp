////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "V8QueueJob.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Logger/Logger.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new V8 job
////////////////////////////////////////////////////////////////////////////////

V8QueueJob::V8QueueJob(size_t queue, TRI_vocbase_t* vocbase,
                       std::shared_ptr<VPackBuilder> parameters)
    : Job("V8 Queue Job"),
      _queue(queue),
      _vocbase(vocbase),
      _parameters(parameters),
      _canceled(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a V8 job
////////////////////////////////////////////////////////////////////////////////

V8QueueJob::~V8QueueJob() {}

size_t V8QueueJob::queue() const { return _queue; }

void V8QueueJob::work() {
  if (_canceled) {
    return;
  }

  auto context = V8DealerFeature::DEALER->enterContext(_vocbase, false);

  // note: the context might be 0 in case of shut-down
  if (context == nullptr) {
    return;
  }

  TRI_DEFER(V8DealerFeature::DEALER->exitContext(context));

  // now execute the function within this context
  {
    auto isolate = context->_isolate;
    v8::HandleScope scope(isolate);

    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    auto current = isolate->GetCurrentContext()->Global();
    auto main = v8::Local<v8::Function>::Cast(
        current->Get(TRI_V8_ASCII_STRING("MAIN")));

    if (!main.IsEmpty()) {
      // only execute if main was compiled successfully
      v8::Handle<v8::Value> fArgs;

      if (_parameters != nullptr) {
        fArgs = TRI_VPackToV8(isolate, _parameters->slice());
      } else {
        fArgs = v8::Undefined(isolate);
      }

      // call the function
      try {
        v8::TryCatch tryCatch;
        main->Call(current, 1, &fArgs);

        if (tryCatch.HasCaught()) {
          if (tryCatch.CanContinue()) {
            TRI_LogV8Exception(isolate, &tryCatch);
          } else {
            TRI_GET_GLOBALS();

            v8g->_canceled = true;
            LOG(WARN) << "caught non-catchable exception (aka termination) in "
                         "V8 queue job";
          }
        }
      } catch (arangodb::basics::Exception const& ex) {
        LOG(ERR) << "caught exception in V8 queue job: "
                 << TRI_errno_string(ex.code()) << " " << ex.what();
      } catch (std::bad_alloc const&) {
        LOG(ERR) << "caught exception in V8 queue job: "
                 << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
      } catch (...) {
        LOG(ERR) << "caught unknown exception in V8 queue job";
      }
    }
  }
}

bool V8QueueJob::cancel() {
  _canceled = true;
  return true;
}

void V8QueueJob::cleanup(DispatcherQueue* queue) {
  queue->removeJob(this);
  delete this;
}

void V8QueueJob::handleError(Exception const& ex) {}
