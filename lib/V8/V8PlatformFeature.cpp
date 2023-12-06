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
void gcPrologueCallback(v8::Isolate* isolate, v8::GCType /*type*/,
                        v8::GCCallbackFlags /*flags*/) {
  // if (type != v8::kGCTypeMarkSweepCompact) {
  //   return;
  // }

  v8::HeapStatistics h;
  isolate->GetHeapStatistics(&h);

  V8PlatformFeature::getIsolateData(isolate)->_heapSizeAtStart =
      h.used_heap_size();
}

void gcEpilogueCallback(v8::Isolate* isolate, v8::GCType type,
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
    char const* whereFreed =
        (v8g->_inForcedCollect) ? "Forced collect" : "V8 internal collection";
    LOG_TOPIC("95f66", WARN, arangodb::Logger::V8)
        << "reached heap-size limit of context #" << v8g->_id
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
#ifdef V8_UPGRADE
void oomCallback(char const* location, v8::OOMDetails const& details) {
  LOG_TOPIC("cfa4b", FATAL, arangodb::Logger::V8)
      << "out of " << (details.is_heap_oom ? "heap " : "") << "memory in V8 ("
      << location << ")" << (details.detail == nullptr ? "" : ": ")
      << (details.detail == nullptr ? "" : details.detail);
  FATAL_ERROR_EXIT();
}
#else
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
#endif

// this callback is executed by V8 when it encounters a fatal error.
// after the callback returns, V8 will call std::abort() and
// terminate the entire process
void fatalCallback(char const* location, char const* message) {
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

    if (_v8CombinedOptions == "help" || _v8CombinedOptions == "--help") {
      std::string_view help = "--help";
      v8::V8::SetFlagsFromString(help.data(), help.size());
      exit(EXIT_SUCCESS);
    }
  }
}

void V8PlatformFeature::start() {
  v8::V8::InitializeICU();

  _platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(_platform.get());

  // explicit option --javascript.v8-options used
  if (!_v8CombinedOptions.empty()) {
    LOG_TOPIC("d064a", INFO, Logger::V8)
        << "using V8 options '" << _v8CombinedOptions << "'";
  }

  // must be turned off currently due to assertion failures
  // and segfaults within V8's regexp peephole optimizer.
  _v8CombinedOptions.append(" --no-regexp-peephole-optimization");
  v8::V8::SetFlagsFromString(_v8CombinedOptions.c_str(),
                             _v8CombinedOptions.size());

  v8::V8::Initialize();

  _allocator = std::unique_ptr<v8::ArrayBuffer::Allocator>(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
}

void V8PlatformFeature::unprepare() {
  v8::V8::Dispose();
#ifdef V8_UPGRADE
  v8::V8::DisposePlatform();
#else
  v8::V8::ShutdownPlatform();
#endif
  _platform.reset();
  _allocator.reset();
}

v8::Isolate* V8PlatformFeature::createIsolate() {
  v8::Isolate::CreateParams createParams;
  createParams.array_buffer_allocator = _allocator.get();

  if (_v8MaxHeap > 0) {
    // note: _v8HeapMax is specified in megabytes
    createParams.constraints.ConfigureDefaultsFromHeapSize(
        /*initial_heap_size_in_bytes*/ 0,
        /*maximum_heap_size_in_bytes*/ _v8MaxHeap * 1024 * 1024);
  }

  auto isolate = v8::Isolate::New(createParams);
  isolate->SetOOMErrorHandler(::oomCallback);
  isolate->SetFatalErrorHandler(::fatalCallback);
  isolate->AddGCPrologueCallback(::gcPrologueCallback);
  isolate->AddGCEpilogueCallback(::gcEpilogueCallback);

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
