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

  b(Value(ValueType::Object))("b", Value(12))("a", Value(true))(
      "l", Value(ValueType::Array))(Value(1))(Value(2))(Value(3))()(
      "name", Value("Gustav"))();

  // now dump the resulting VPack value
  std::cout << "Resulting VPack:" << std::endl;
  std::cout << HexDump(b.slice()) << std::endl;
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
