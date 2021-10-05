#include <iostream>
#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

int main(int, char* []) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  // create an array with 10 number members
  Builder b;

  b(Value(ValueType::Array));
  for (size_t i = 0; i < 10; ++i) {
    b.add(Value(i));
  }
  b.close();

  // a Slice is a lightweight accessor for a VPack value
  Slice s(b.start());

  // filter out all array values less than 5
  Builder filtered =
      Collection::filter(s, [](Slice const& current, ValueLength) {
        if (current.getNumber<int>() < 5) {
          // exclude
          return false;
        }
        // include
        return true;
      });

  // print filtered array
  Slice f(filtered.slice());
  std::cout << "Filtered length: " << f.length() << std::endl;

  // now iterate over the (left over) array members
  std::cout << "Iterating Array members:" << std::endl;
  for (auto const& it : ArrayIterator(f)) {
    std::cout << it << ", number value: " << it.getUInt() << std::endl;
  }

  // iterate again, this time using Collection::forEach
  std::cout << "Iterating some Array members using forEach:" << std::endl;
  Collection::forEach(f, [](Slice const& current, ValueLength index) {
    std::cout << current << ", number value: " << current.getUInt()
              << std::endl;
    return (index != 2);  // stop after the 3rd element (indexes are 0-based)
  });
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
