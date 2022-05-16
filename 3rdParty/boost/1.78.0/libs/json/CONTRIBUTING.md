# Contributing to Boost.JSON

## Quickstart

Here is minimal sequence of steps required to prepare development environment:

1. Download Boost superproject
2. Download Boost.JSON
3. Bootstrap Boost
4. Build Boost.JSON tests, benchmarks and examples

```
git clone --recurse-submodules --jobs 8 https://github.com/boostorg/boost.git
cd boost/libs
git clone --recurse-submodules https://github.com/cppalliance/json.git
cd ..
./bootstrap.sh
./b2 headers
cd libs/json
../../b2 test
../../b2 bench
../../b2 example
```
