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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <string_view>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

namespace arangodb {

class InputProcessor {
 public:
  virtual ~InputProcessor() = default;

  // returns if there is an item to get
  virtual bool valid() const noexcept = 0;

  // returns the current value and advances the input. requires valid()
  virtual velocypack::Slice value() = 0;
};

class InputProcessorJSONL : public InputProcessor {
 public:
  explicit InputProcessorJSONL(std::string_view data);
  ~InputProcessorJSONL();

  // returns if there is an item to get
  bool valid() const noexcept override;

  // returns the current value and advances the input. requires valid()
  velocypack::Slice value() override;

 private:
  void consumeWhitespace();

  std::string_view _data;
  size_t _position;
  velocypack::Builder _builder;
  velocypack::Parser _parser;
};

class InputProcessorVPackArray : public InputProcessor {
 public:
  explicit InputProcessorVPackArray(std::string_view data);
  ~InputProcessorVPackArray();

  // returns if there is an item to get
  bool valid() const noexcept override;

  // returns the current value and advances the input. requires valid()
  velocypack::Slice value() override;

 private:
  velocypack::Slice _data;
  velocypack::ArrayIterator _iterator;
};

}  // namespace arangodb
