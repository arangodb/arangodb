#!/bin/bash

git clone git@github.com:boostorg/boost.git || exit 1

pushd boost/libs && git submodule init callable_traits \
  algorithm \
  align \
  any \
  array \
  array \
  assert \
  atomic \
  bind \
  concept_check \
  config \
  container \
  container_hash \
  core \
  detail \
  exception \
  filesystem \
  format \
  function \
  functional \
  io \
  iostreams \
  integer \
  iterator \
  lexical_cast \
  move \
  mpl \
  numeric \
  optional \
  optional \
  predef \
  preprocessor \
  program_options \
  range \
  regex \
  smart_ptr \
  spirit \
  static_assert \
  system \
  throw_exception \
  tokenizer \
  tuple \
  type_index \
  type_traits \
  unordered \
  utility || exit 1

popd
pushd boost && git submodule init tools || exit 1

git submodule update || exit 1
./bootstrap.sh || exit 1

./b2 headers
./b2 tools/quickbook
./b2 libs/callable_traits/doc && echo "$PWD/libs/callable_traits/doc/html/index.html"

popd

