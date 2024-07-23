////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/operating-system.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <v8-platform.h>
#include <v8.h>

namespace arangodb {

namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

class V8PlatformFeature final
    : public application_features::ApplicationFeature {
 public:
  template<typename Server>
  explicit V8PlatformFeature(Server& server)
      : ApplicationFeature{server, *this}, _binaryPath(server.getBinaryPath()) {
    setOptional(true);
  }

  static constexpr std::string_view name() noexcept { return "V8Platform"; }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void unprepare() override final;

  v8::Isolate* createIsolate();
  void disposeIsolate(v8::Isolate*);

  struct IsolateData {
    bool _outOfMemory = false;
    size_t _heapSizeAtStart = 0;
  };

  static IsolateData* getIsolateData(v8::Isolate* isolate) {
    return reinterpret_cast<IsolateData*>(isolate->GetData(V8_INFO));
  }

  static bool isOutOfMemory(v8::Isolate* isolate) {
    return getIsolateData(isolate)->_outOfMemory;
  }

  static void setOutOfMemory(v8::Isolate* isolate) {
    getIsolateData(isolate)->_outOfMemory = true;
  }

  static void resetOutOfMemory(v8::Isolate* isolate) {
    getIsolateData(isolate)->_outOfMemory = false;
  }

  static constexpr uint32_t V8_INFO = 0;
  static constexpr uint32_t V8_DATA_SLOT = 1;

 private:
  // "icudtl.dat", needs to be 0-terminated
  static std::string const fn;

  std::string determineICUDataPath();

  char const* _binaryPath;

  std::vector<std::string> _v8Options;
  uint64_t _v8MaxHeap = TRI_V8_MAXHEAP;

  std::unique_ptr<v8::Platform> _platform;
  std::unique_ptr<v8::ArrayBuffer::Allocator> _allocator;
  std::string _v8CombinedOptions;
  std::mutex _lock;  // to protect vector _isolateData
  std::unordered_map<v8::Isolate*, std::unique_ptr<IsolateData>> _isolateData;
};

}  // namespace arangodb
