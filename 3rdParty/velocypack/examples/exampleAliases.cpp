#include <iostream>
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"
#include "velocypack/velocypack-aliases.h"
#include "velocypack/velocypack-exception-macros.h"

int main(int, char* []) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  // create an array with 10 number members
  VPackBuilder b;

  b(VPackValue(VPackValueType::Array));
  for (size_t i = 0; i < 10; ++i) {
    b.add(VPackValue(i));
  }
  b.close();

  // a Slice is a lightweight accessor for a VPack value
  VPackSlice s(b.start());

  // inspect the outermost value (should be an Array...)
  std::cout << "Slice: " << s << std::endl;
  std::cout << "Type: " << s.type() << std::endl;
  std::cout << "Bytesize: " << s.byteSize() << std::endl;
  std::cout << "Members: " << s.length() << std::endl;

  // now iterate over the array members
  std::cout << "Iterating Array members:" << std::endl;
  for (auto const& it : VPackArrayIterator(s)) {
    std::cout << it << ", number value: " << it.getUInt() << std::endl;
  }
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
