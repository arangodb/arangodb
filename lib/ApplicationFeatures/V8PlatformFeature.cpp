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

#include "ApplicationFeatures/V8PlatformFeature.h"

#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::options;

namespace {
  class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
   public:
    virtual void* Allocate(size_t length) override {
      void* data = AllocateUninitialized(length);
      return data == nullptr ? data : memset(data, 0, length);
    }
    virtual void* AllocateUninitialized(size_t length) override {
      return malloc(length);
    }
    virtual void Free(void* data, size_t) override { free(data); }
  };
}

V8PlatformFeature::V8PlatformFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "V8Platform") {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void V8PlatformFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");

  options->addHiddenOption("--javascript.v8-options", "options to pass to v8",
                           new StringParameter(&_v8options));
}

void V8PlatformFeature::start() {
  v8::V8::InitializeICU();

  // explicit option --javascript.v8-options used
  if (!_v8options.empty()) {
    LOG(INFO) << "using V8 options '" << _v8options << "'";
    v8::V8::SetFlagsFromString(_v8options.c_str(), (int)_v8options.size());
  }

#ifdef TRI_FORCE_ARMV6
  std::string const forceARMv6 = "--noenable-armv7";
  v8::V8::SetFlagsFromString(forceARMv6.c_str(), (int)forceARMv6.size());
#endif

  _platform.reset(v8::platform::CreateDefaultPlatform());
  v8::V8::InitializePlatform(_platform.get());
  v8::V8::Initialize();

  _allocator.reset(new ArrayBufferAllocator);
}

void V8PlatformFeature::unprepare() {
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  _platform.reset();
  _allocator.reset();
}
