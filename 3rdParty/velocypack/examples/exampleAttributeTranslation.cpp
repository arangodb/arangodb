#include <iostream>
#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

static void printObject(Builder const& b) {
  // a Slice is a lightweight accessor for a VPack value
  Slice s(b.start());

  // inspect the outermost value (should be an Object...)
  std::cout << "Slice: " << s << std::endl;
  std::cout << "Type: " << s.type() << std::endl;
  std::cout << "Bytesize: " << s.byteSize() << std::endl;
  std::cout << "Members: " << s.length() << std::endl;

  // now iterate over the object members
  // the attribute name translator works transparently
  std::cout << "Iterating Object members:" << std::endl;
  for (auto const& it : ObjectIterator(s)) {
    std::cout << "key: " << it.key.copyString()
              << ", value: " << it.value.toJson() << std::endl;
  }
}

static Builder buildObject(Options const* options) {
  Builder b(options);

  b(Value(ValueType::Object));
  b.add("foo", Value(42));
  b.add("bar", Value("some string value"));
  b.add("baz", Value(true));
  b.close();

  return b;
}

int main(int, char* []) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);
  // these attribute names will be translated into short integer values
  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->seal();

  Options withTranslatorOptions;
  // register the above attribute name translator
  withTranslatorOptions.attributeTranslator = translator.get();

  // build an object using attribute name translations
  std::cout << "Building object with translations:" << std::endl;
  Builder b1 = buildObject(&withTranslatorOptions);
  printObject(b1);

  std::cout << std::endl;

  // now build the same object again, but without translations
  Options noTranslatorOptions;
  noTranslatorOptions.attributeTranslator = nullptr;

  std::cout << "Building object without translations:" << std::endl;
  Builder b2 = buildObject(&noTranslatorOptions);
  printObject(b2);
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
