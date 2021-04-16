#include <iostream>
#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

int main(int, char* []) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  // create an object with a few members
  Builder b;

  b(Value(ValueType::Object));
  b.add("foo", Value(42));
  b.add("bar", Value("some string value"));
  b.add("baz", Value(ValueType::Array));
  b.add(Value(1));
  b.add(Value(2));
  b.close();
  b.add("qux", Value(true));
  b.close();

  // a Slice is a lightweight accessor for a VPack value
  Slice s(b.start());

  // inspect the outermost value (should be an Object...)
  std::cout << "Slice: " << s << std::endl;
  std::cout << "Type: " << s.type() << std::endl;
  std::cout << "Bytesize: " << s.byteSize() << std::endl;
  std::cout << "Members: " << s.length() << std::endl;

  // now iterate over the object members
  std::cout << "Iterating Object members:" << std::endl;
  for (auto const& it : ObjectIterator(s)) {
    std::cout << "key: " << it.key.copyString() << ", value: " << it.value
              << std::endl;
  }
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
