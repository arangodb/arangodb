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
  b.add("baz", Value(ValueType::Object));
  b.add("qux", Value(true));
  b.add("bart", Value("this is a string"));
  b.close();
  b.add("quux", Value(12345));
  b.close();

  // a Slice is a lightweight accessor for a VPack value
  Slice s(b.start());

  // now fetch the string in the object's "bar" attribute
  if (s.hasKey("bar")) {
    Slice bar(s.get("bar"));
    std::cout << "'bar' attribute value has type: " << bar.type() << std::endl;
  }

  // fetch non-existing attribute "quetzal"
  Slice quetzal(s.get("quetzal"));
  // note: this returns a slice of type None
  std::cout << "'quetzal' attribute value has type: " << quetzal.type()
            << std::endl;
  std::cout << "'quetzal' attribute is None: " << std::boolalpha
            << quetzal.isNone() << std::endl;

  // fetch subattribute "baz.qux"
  Slice qux(s.get(std::vector<std::string>({"baz", "qux"})));
  std::cout << "'baz'.'qux' attribute has type: " << qux.type() << std::endl;
  std::cout << "'baz'.'qux' attribute has bool value: " << std::boolalpha
            << qux.getBoolean() << std::endl;
  std::cout << "Complete value of 'baz' is: " << s.get("baz").toJson()
            << std::endl;

  // fetch non-existing subattribute "bark.foobar"
  Slice foobar(s.get(std::vector<std::string>({"bark", "foobar"})));
  std::cout << "'bark'.'foobar' attribute is None: " << std::boolalpha
            << foobar.isNone() << std::endl;

  // check if subattribute "baz"."bart" does exist
  if (s.hasKey(std::vector<std::string>({"baz", "bart"}))) {
    // access subattribute using operator syntax
    std::cout << "'baz'.'bart' attribute has type: " << s["baz"]["bart"].type()
              << std::endl;
    std::cout << "'baz'.'bart' attribute has value: '"
              << s["baz"]["bart"].copyString() << "'" << std::endl;
  }
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
