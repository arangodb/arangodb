#include <iostream>
#include <iomanip>
#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

int main(int, char* []) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  // this is the JSON string we are going to parse
  std::string const json = "{\"a\":12}";
  std::cout << "Parsing JSON string '" << json << "'" << std::endl;

  Parser parser;
  try {
    ValueLength nr = parser.parse(json);
    std::cout << "Number of values: " << nr << std::endl;
  } catch (std::bad_alloc const&) {
    std::cout << "Out of memory!" << std::endl;
    throw;
  } catch (Exception const& ex) {
    std::cout << "Parse error: " << ex.what() << std::endl;
    std::cout << "Position of error: " << parser.errorPos() << std::endl;
    throw;
  }

  // get a pointer to the start of the data
  std::shared_ptr<Builder> b = parser.steal();

  // now dump the resulting VPack value
  std::cout << "Resulting VPack:" << std::endl;
  std::cout << HexDump(b->slice()) << std::endl;
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
