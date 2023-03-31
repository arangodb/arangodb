#!/bin/sh

cmake --preset community-developer
(
  cd build/community-developer/
  cmake --build . --target flex_vector_test
  ctest -R '^flex_vector_test$' --output-on-failure
)
