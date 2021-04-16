#include <iostream>
#include <iomanip>
#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

int main(int, char* []) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  // create an object with attributes "b", "a", "l" and "name"
  // note that the attribute names will be sorted in the target VPack object!
  Builder b;

  b.add(Value(ValueType::Object));
  b.add("b", Value(12));
  b.add("a", Value(true));
  b.add("l", Value(ValueType::Array));
  b.add(Value(1));
  b.add(Value(2));
  b.add(Value(3));
  b.close();
  b.add("name", Value("Gustav"));
  b.close();

  // now dump the resulting VPack value
  std::cout << "Resulting VPack:" << std::endl;
  std::cout << "Resulting VPack:" << b.slice() << std::endl;
  std::cout << HexDump(b.slice()) << std::endl;
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
