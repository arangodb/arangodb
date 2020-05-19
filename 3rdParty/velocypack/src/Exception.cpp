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

#include "velocypack/Exception.h"
#include "velocypack/velocypack-common.h"

#include <execinfo.h>
#include <sstream>
  
#if defined(__GNUC__) && (__GNUC___ > 9 || (__GNUC__ == 9 && __GNUC_MINOR__ >= 2))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-address"
#endif

namespace {
void* getreturnaddr(int level) {
  switch (level) {
    case 0:
      return __builtin_return_address(1);
    case 1:
      return __builtin_return_address(2);
    case 2:
      return __builtin_return_address(3);
    case 3:
      return __builtin_return_address(4);
    case 4:
      return __builtin_return_address(5);
    case 5:
      return __builtin_return_address(6);
    case 6:
      return __builtin_return_address(7);
    case 7:
      return __builtin_return_address(8);
    case 8:
      return __builtin_return_address(9);
    case 9:
      return __builtin_return_address(10);
    case 10:
      return __builtin_return_address(11);
    case 11:
      return __builtin_return_address(12);
    case 12:
      return __builtin_return_address(13);
    case 13:
      return __builtin_return_address(14);
    case 14:
      return __builtin_return_address(15);
    case 15:
      return __builtin_return_address(16);
    case 16:
      return __builtin_return_address(17);
    case 17:
      return __builtin_return_address(18);
    case 18:
      return __builtin_return_address(19);
    case 19:
      return __builtin_return_address(20);
    case 20:
      return __builtin_return_address(21);
    case 21:
      return __builtin_return_address(22);
    case 22:
      return __builtin_return_address(23);
    case 23:
      return __builtin_return_address(24);
    case 24:
      return __builtin_return_address(25);
    case 25:
      return __builtin_return_address(26);
    case 26:
      return __builtin_return_address(27);
    case 27:
      return __builtin_return_address(28);
    case 28:
      return __builtin_return_address(29);
    case 29:
      return __builtin_return_address(30);
    case 30:
      return __builtin_return_address(31);
    case 31:
      return __builtin_return_address(32);
    case 32:
      return __builtin_return_address(33);
    case 33:
      return __builtin_return_address(34);
    case 34:
      return __builtin_return_address(35);
    case 35:
      return __builtin_return_address(36);
    case 36:
      return __builtin_return_address(37);
    case 37:
      return __builtin_return_address(38);
    case 38:
      return __builtin_return_address(39);
    case 39:
      return __builtin_return_address(40);
    case 40:
      return __builtin_return_address(41);
    case 41:
      return __builtin_return_address(42);
    case 42:
      return __builtin_return_address(43);
    case 43:
      return __builtin_return_address(44);
    case 44:
      return __builtin_return_address(45);
    case 45:
      return __builtin_return_address(46);
    case 46:
      return __builtin_return_address(47);
    case 47:
      return __builtin_return_address(48);
    case 48:
      return __builtin_return_address(49);
    case 49:
      return __builtin_return_address(50);
    case 50:
      return __builtin_return_address(51);
    case 51:
      return __builtin_return_address(52);
    case 52:
      return __builtin_return_address(53);
    case 53:
      return __builtin_return_address(54);
    case 54:
      return __builtin_return_address(55);
    case 55:
      return __builtin_return_address(56);
    case 56:
      return __builtin_return_address(57);
    case 57:
      return __builtin_return_address(58);
    case 58:
      return __builtin_return_address(59);
    case 59:
      return __builtin_return_address(60);
    case 60:
      return __builtin_return_address(61);
    case 61:
      return __builtin_return_address(62);
    case 62:
      return __builtin_return_address(63);
    case 63:
      return __builtin_return_address(64);
    case 64:
      return __builtin_return_address(65);
    case 65:
      return __builtin_return_address(66);
    case 66:
      return __builtin_return_address(67);
    case 67:
      return __builtin_return_address(68);
    case 68:
      return __builtin_return_address(69);
    case 69:
      return __builtin_return_address(70);
    case 70:
      return __builtin_return_address(71);
    case 71:
      return __builtin_return_address(72);
    case 72:
      return __builtin_return_address(73);
    case 73:
      return __builtin_return_address(74);
    case 74:
      return __builtin_return_address(75);
    case 75:
      return __builtin_return_address(76);
    case 76:
      return __builtin_return_address(77);
    case 77:
      return __builtin_return_address(78);
    case 78:
      return __builtin_return_address(79);
    case 79:
      return __builtin_return_address(80);
    case 80:
      return __builtin_return_address(81);
    case 81:
      return __builtin_return_address(82);
    case 82:
      return __builtin_return_address(83);
    case 83:
      return __builtin_return_address(84);
    case 84:
      return __builtin_return_address(85);
    case 85:
      return __builtin_return_address(86);
    case 86:
      return __builtin_return_address(87);
    default:
      return nullptr;
  }
}

#if defined(__GNUC__) && (__GNUC__ > 9 || (__GNUC__ == 9 && __GNUC_MINOR__ >= 2))
#pragma GCC diagnostic pop
#endif

extern "C" {
char* mystart = nullptr;   // will be overwritten by main() in `arangod`
int main(int argc, char* argv[]);
}

std::vector<void*> read_backtrace() {
  std::vector<void*> trace;
  trace.resize(40, nullptr);

  constexpr size_t main_size = 0x289;   // from nm -S, LOL
  constexpr size_t start_size = 0x14d;  // from nm -S, LOL

  size_t i = 0;
  for (; i < trace.size() - 1; i++) {
    trace[i] = getreturnaddr(i + 1);
    if ((char*) trace[i] >= (char*) main && (char*) trace[i] < (char*) main + main_size) {
      break;
    }
    if ((char*) trace[i] >= mystart && trace[i] < mystart + start_size) {
      break;
    }
  }

  trace.resize(i);
  return trace;
}

}  // namespace

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

std::vector<void*> Exception::generateBacktrace() { return read_backtrace(); }

std::string Exception::formatBacktrace() const {
  std::stringstream ss;
  char** syms = backtrace_symbols(_backtrace.data(), _backtrace.size());

  for (size_t i = 0; i < _backtrace.size(); i++) {
    ss << syms[i] << std::endl;
  }

  free(syms);

  return std::move(ss).str();
}

#include <iostream>

Exception::Exception(Exception::ExceptionType type, const char* msg)
    : _type(type), _msg(msg), _backtrace(generateBacktrace()) {
  std::cout << "VPackException msg = " << msg << std::endl << formatBacktrace();
}
