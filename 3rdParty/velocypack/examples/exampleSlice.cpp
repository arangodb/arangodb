#include <iostream>
#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

int main(int, char* []) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  // create an object with attributes "b", "a", "l" and "name"
  // note that the attribute names will be sorted in the target VPack object!
  Builder b;

  b(Value(ValueType::Object))("b", Value(12))("a", Value(true))(
      "l", Value(ValueType::Array))(Value(1))(Value(2))(Value(3))()(
      "name", Value("Gustav"))();

  // a Slice is a lightweight accessor for a VPack value
  Slice s(b.start());

  // inspect the outermost value (should be an Object...)
  std::cout << "Slice: " << s << std::endl;
  std::cout << "Type: " << s.type() << std::endl;
  std::cout << "Bytesize: " << s.byteSize() << std::endl;
  std::cout << "Members: " << s.length() << std::endl;

  if (s.isObject()) {
    Slice ss = s.get("l");  // Now ss points to the subvalue under "l"
    std::cout << "Length of .l: " << ss.length() << std::endl;
    std::cout << "Second entry of .l:" << ss.at(1).getInt() << std::endl;
  }

  Slice sss = s.get("name");
  if (sss.isString()) {
    ValueLength len;
    char const* st = sss.getString(len);
    std::cout << "Name in .name: " << std::string(st, checkOverflow(len))
              << std::endl;
  }

  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
