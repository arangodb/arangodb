#include <iostream>
#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

int main(int, char* []) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  Builder b;
  // build an object with attribute names "b", "a", "l", "name"
  b(Value(ValueType::Object))("b", Value(12))("a", Value(true))(
      "l", Value(ValueType::Array))(Value(1))(Value(2))(Value(3))()(
      "name", Value("Gustav"))();

  // a Slice is a lightweight accessor for a VPack value
  Slice s(b.start());

  // now dump the Slice into an std::string sink
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(s);

  // and print it
  std::cout << "Resulting VPack:" << std::endl << buffer << std::endl;
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
