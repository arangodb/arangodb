# -*- mode: CMAKE; -*-

# build libvelocypack
add_library(
  velocypack
  STATIC
  velocypack-common.cpp
  AttributeTranslator.cpp
  Builder.cpp
  Collection.cpp
  Dumper.cpp
  Exception.cpp
  HexDump.cpp
  Iterator.cpp
  Options.cpp
  Parser.cpp
  Slice.cpp
  ValueType.cpp
  Version.cpp
  asm-functions.cpp
  fasthash.cpp
  fpconv.cpp
)

install(TARGETS velocypack DESTINATION lib)
