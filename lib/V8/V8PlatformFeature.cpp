////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <stdlib.h>
#include <string.h>
#include <limits>
#include <type_traits>
#include <utility>

#include <libplatform/libplatform.h>

#include "V8PlatformFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "V8/v8-globals.h"

using namespace arangodb;
using namespace arangodb::basics;
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

static void gcPrologueCallback(v8::Isolate* isolate, v8::GCType /*type*/,
                               v8::GCCallbackFlags /*flags*/) {
  // if (type != v8::kGCTypeMarkSweepCompact) {
  //   return;
  // }

  v8::HeapStatistics h;
  isolate->GetHeapStatistics(&h);

  V8PlatformFeature::getIsolateData(isolate)->_heapSizeAtStart =
      h.used_heap_size();
}

static void gcEpilogueCallback(v8::Isolate* isolate, v8::GCType type,
                               v8::GCCallbackFlags /*flags*/) {
  TRI_GET_GLOBALS();
  size_t constexpr LIMIT_ABS = 200 * 1024 * 1024;
  size_t minFreed = LIMIT_ABS / 10;

  if (type != v8::kGCTypeMarkSweepCompact) {
    minFreed = 0;
  }

  v8::HeapStatistics h;
  isolate->GetHeapStatistics(&h);

  auto now = TRI_microtime();
  size_t freed = 0;
  size_t heapSizeAtStop = h.used_heap_size();
  size_t heapSizeAtStart =
      V8PlatformFeature::getIsolateData(isolate)->_heapSizeAtStart;

  if (heapSizeAtStop < heapSizeAtStart) {
    freed = heapSizeAtStart - heapSizeAtStop;
  }

  size_t heapSizeLimit = h.heap_size_limit();
  size_t usedHeapSize = h.used_heap_size();
  size_t stillFree = heapSizeLimit - usedHeapSize;

  if (now - v8g->_lastMaxTime > 10) {
    v8g->_heapMax = heapSizeAtStart;
    v8g->_heapLow = heapSizeAtStop;
    v8g->_countOfTimes = 0;
    v8g->_lastMaxTime = now;
  } else {
    v8g->_countOfTimes++;
    if (heapSizeAtStart > v8g->_heapMax) {
      v8g->_heapMax = heapSizeAtStart;
    }
    if (v8g->_heapLow > heapSizeAtStop) {
      v8g->_heapLow = heapSizeAtStop;
    }
  }

  if (stillFree <= LIMIT_ABS && freed <= minFreed) {
    const char* whereFreed =
        (v8g->_inForcedCollect) ? "Forced collect" : "V8 internal collection";
    LOG_TOPIC("95f66", WARN, arangodb::Logger::V8)
        << "reached heap-size limit of #" << v8g->_id
        << " interrupting V8 execution ("
        << "heap size limit " << heapSizeLimit << ", used " << usedHeapSize
        << ") during " << whereFreed;

    isolate->TerminateExecution();
    V8PlatformFeature::setOutOfMemory(isolate);
  }
}

// this callback is executed by V8 when it runs out of memory.
// after the callback returns, V8 will call std::abort() and
// terminate the entire process
static void oomCallback(char const* location, bool isHeapOOM) {
  if (isHeapOOM) {
    LOG_TOPIC("fd5c4", FATAL, arangodb::Logger::V8)
        << "out of heap hemory in V8 (" << location << ")";
  } else {
    LOG_TOPIC("5d980", FATAL, arangodb::Logger::V8)
        << "out of memory in V8 (" << location << ")";
  }
  FATAL_ERROR_EXIT();
}

// this callback is executed by V8 when it encounters a fatal error.
// after the callback returns, V8 will call std::abort() and
// terminate the entire process
static void fatalCallback(char const* location, char const* message) {
  if (message == nullptr) {
    message = "no message";
  }
  LOG_TOPIC("531c0", FATAL, arangodb::Logger::V8)
      << "fatal error in V8 (" << location << "): " << message;
  FATAL_ERROR_EXIT();
}

}  // namespace

void V8PlatformFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "JavaScript engine and execution");

  options
      ->addOption("--javascript.v8-options", "Options to pass to V8.",
                  new VectorParameter<StringParameter>(&_v8Options),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can optionally pass arguments to the V8
JavaScript engine. The V8 engine runs with the default settings unless you
explicitly specify them. The options are forwarded to the V8 engine, which
parses them on its own. Passing invalid options may result in an error being
printed on stderr and the option being ignored.

You need to pass the options as one string, with V8 option names being prefixed
with two hyphens. Multiple options need to be separated by whitespace. To get
a list of all available V8 options, you can use the value `"--help"` as follows:

```
--javascript.v8-options="--help"
```

Another example of specific V8 options being set at startup:

```
--javascript.v8-options="--log --no-logfile-per-isolate --logfile=v8.log"
```

Names and features or usable options depend on the version of V8 being used, and
might change in the future if a different version of V8 is being used in
ArangoDB. Not all options offered by V8 might be sensible to use in the context
of ArangoDB. Use the specific options only if you are sure that they are not
harmful for the regular database operation.)");

  options->addOption("--javascript.v8-max-heap",
                     "The maximal heap size (in MiB).",
                     new UInt64Parameter(&_v8MaxHeap));
}

void V8PlatformFeature::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/) {
  if (!_v8Options.empty()) {
    _v8CombinedOptions = StringUtils::join(_v8Options, " ");

    if (_v8CombinedOptions == "help") {
      std::string help = "--help";
      v8::V8::SetFlagsFromString(help.c_str(), (int)help.size());
      exit(EXIT_SUCCESS);
    }
  }

  if (0 < _v8MaxHeap) {
    // we have to compare against INT_MAX here, because the value is an int
    // inside V8
    if (_v8MaxHeap > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
      LOG_TOPIC("81a63", FATAL, arangodb::Logger::V8)
          << "value for '--javascript.v8-max-heap' exceeds maximum value "
          << (std::numeric_limits<int>::max)();
      FATAL_ERROR_EXIT();
    }
  }
}

void V8PlatformFeature::start() {
  v8::V8::InitializeICU();

  // explicit option --javascript.v8-options used
  if (!_v8CombinedOptions.empty()) {
    LOG_TOPIC("d064a", INFO, Logger::V8)
        << "using V8 options '" << _v8CombinedOptions << "'";
    v8::V8::SetFlagsFromString(_v8CombinedOptions.c_str(),
                               (int)_v8CombinedOptions.size());
  }

#ifdef TRI_FORCE_ARMV6
  std::string const forceARMv6 = "--noenable-armv7";
  v8::V8::SetFlagsFromString(forceARMv6.c_str(), (int)forceARMv6.size());
#endif

  _platform = v8::platform::NewDefaultPlatform();
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

v8::Isolate* V8PlatformFeature::createIsolate() {
  v8::Isolate::CreateParams createParams;
  createParams.array_buffer_allocator = _allocator.get();

  if (0 < _v8MaxHeap) {
    createParams.constraints.set_max_old_space_size(
        static_cast<int>(_v8MaxHeap));
  }

  auto isolate = v8::Isolate::New(createParams);
  isolate->SetOOMErrorHandler(oomCallback);
  isolate->SetFatalErrorHandler(fatalCallback);
  isolate->AddGCPrologueCallback(gcPrologueCallback);
  isolate->AddGCEpilogueCallback(gcEpilogueCallback);

  auto data = std::make_unique<IsolateData>();
  isolate->SetData(V8_INFO, data.get());

  {
    std::lock_guard guard{_lock};
    try {
      _isolateData.try_emplace(isolate, std::move(data));
    } catch (...) {
      isolate->SetData(V8_INFO, nullptr);
      isolate->Dispose();
      throw;
    }
  }

  return isolate;
}

void V8PlatformFeature::disposeIsolate(v8::Isolate* isolate) {
  // must first remove from isolate-data map
  {
    std::lock_guard guard{_lock};
    _isolateData.erase(isolate);
  }
  // because Isolate::Dispose() will delete isolate!
  isolate->Dispose();
}
