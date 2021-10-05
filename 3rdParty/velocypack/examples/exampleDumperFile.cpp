#include <iostream>
#include <fstream>
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

  // now dump the Slice into an outfile
  Options dumperOptions;
  dumperOptions.prettyPrint = true;

  // this is our output file
  try {
    std::ofstream ofs("prettified.json", std::ofstream::out);

    OutputFileStreamSink sink(&ofs);
    Dumper::dump(s, &sink);
    std::cout << "successfully wrote JSON to outfile 'prettified.json'"
              << std::endl;
  } catch (std::exception const& ex) {
    std::cout << "could not write outfile 'prettified.json': " << ex.what()
              << std::endl;
  }
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
