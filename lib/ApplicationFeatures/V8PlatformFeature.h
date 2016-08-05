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

#ifndef ARANGODB_APPLICATION_FEATURES_V8PLATFORM_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_V8PLATFORM_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include <libplatform/libplatform.h>
#include <v8.h>

namespace arangodb {
class V8PlatformFeature final
    : public application_features::ApplicationFeature {
 private:
  struct IsolateData {
    bool _outOfMemory = false;
    size_t _heapSizeAtStart = 0;
  };

 public:
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

 public:
  static const uint32_t V8_INFO = 0;
  static const uint32_t V8_DATA_SLOT = 1;

 public:
  explicit V8PlatformFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void unprepare() override final;

 private:
  std::vector<std::string> _v8Options;
#if defined(__arm__)
  uint64_t _v8MaxHeap = 1 * 1024;
#elif defined(__aarch64__)
  uint64_t _v8MaxHeap = 1 * 1024;
#else
  uint64_t _v8MaxHeap = 3 * 1024;
#endif  

 public:
  v8::Isolate* createIsolate();

 private:
  std::unique_ptr<v8::Platform> _platform;
  std::unique_ptr<v8::ArrayBuffer::Allocator> _allocator;
  std::string _v8CombinedOptions;
  std::vector<std::unique_ptr<IsolateData>> _isolateData;
};
}

#endif
