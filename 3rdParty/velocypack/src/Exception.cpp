////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <ostream>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"

#ifdef __linux__
#include <execinfo.h>
#endif

#include <sstream>

namespace {

std::vector<void*> read_backtrace() {
  std::vector<void*> trace;
#ifdef __linux__
  static constexpr int maxFrames = 100;
  void* traces[maxFrames];
  int numFrames = backtrace(traces, maxFrames);

  for (int i = 0; i < numFrames; ++i) {
    trace.push_back(traces[i]);
  }
#endif
  return trace;
}

}

using namespace arangodb::velocypack;

std::ostream& operator<<(std::ostream& stream, Exception const* ex) {
  stream << "[Exception " << ex->what() << "]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, Exception const& ex) {
  return operator<<(stream, &ex);
}

char const* Exception::message(Exception::ExceptionType type) noexcept {
  switch (type) {
    case InternalError:
      return "Internal error";
    case NotImplemented:
      return "Not implemented";
    case NoJsonEquivalent:
      return "Type has no equivalent in JSON";
    case ParseError:
      return "Parse error";
    case UnexpectedControlCharacter:
      return "Unexpected control character";
    case DuplicateAttributeName:
      return "Duplicate attribute name";
    case IndexOutOfBounds:
      return "Index out of bounds";
    case NumberOutOfRange:
      return "Number out of range";
    case InvalidUtf8Sequence:
      return "Invalid UTF-8 sequence";
    case InvalidAttributePath:
      return "Invalid attribute path";
    case InvalidValueType:
      return "Invalid value type for operation";
    case NeedCustomTypeHandler:
      return "Cannot execute operation without custom type handler";
    case NeedAttributeTranslator:
      return "Cannot execute operation without attribute translator";
    case CannotTranslateKey:
      return "Cannot translate key";
    case KeyNotFound:
      return "Key not found";
    case BuilderNotSealed:
      return "Builder value not yet sealed";
    case BuilderNeedOpenObject:
      return "Need open Object";
    case BuilderNeedOpenArray:
      return "Need open Array";
    case BuilderNeedSubvalue:
      return "Need subvalue in current Object or Array";
    case BuilderNeedOpenCompound:
      return "Need open compound value (Array or Object)";
    case BuilderUnexpectedType:
      return "Unexpected type";
    case BuilderUnexpectedValue:
      return "Unexpected value";
    case BuilderExternalsDisallowed:
      return "Externals are not allowed in this configuration";
    case BuilderKeyAlreadyWritten:
      return "The key of the next key/value pair is already written";
    case BuilderKeyMustBeString:
      return "The key of the next key/value pair must be a string";
    case BuilderCustomDisallowed:
      return "Custom types are not allowed in this configuration";

    case ValidatorInvalidType:
      return "Invalid type found in binary data";
    case ValidatorInvalidLength:
      return "Invalid length found in binary data";

    case UnknownError:
    default:
      return "Unknown error";
  }
}

std::vector<void*> Exception::generateBacktrace() {
  return read_backtrace();
}

std::string Exception::formatBacktrace() const {
  std::stringstream ss;
#ifdef __linux__
  char** syms = backtrace_symbols(_backtrace.data(), _backtrace.size());

  for (size_t i = 0; i < _backtrace.size(); i++) {
    ss << syms[i] << std::endl;
  }

  free(syms);
#endif

  return std::move(ss).str();
}
