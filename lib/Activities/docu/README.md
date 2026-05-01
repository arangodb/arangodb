
## Installation


```
sudo apt install libclang-dev
// creates libclang.so, e.g. in /usr/lib/llvm-18/lib

// python3 -m venv venv
// source venv/bin/activate

// pip install clang==<version> // put in version of libclang-dev
sudo apt install python3-clang-18

```

with clang-query
```
sudo apt install clang-tools
```
```
clang-query -p build/ path/to/file.cpp
```
with build including the `compile_commands.json`

with LibTooling (via C++)
```
sudo apt install clang libclang-19-dev llvm-19-dev cmake (libzstd-dev)
cmake -S . -B build -DClang_DIR=/usr/lib/llvm-18/lib/cmake/clang -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
```
./build/find-vars-of-type -p /path/to/project/build \
    --type=MyType \
    /path/to/project/src/*.cpp
```
jq -r '.[].file' ./compile_commands.json | sort -u | xargs -n 50 -P 8 ./build/find-vars-of-type -p . --type=std::basic_string

jq -r '.[] | select(.file | startswith("/home/jvolmer/code/arangodb/arangod")) | .file' ~/code/arangodb/build-presets/my-edition/compile_commands.json | sort -u | xargs -n 50 -P 8 ./build/find-vars-of-type -p ~/code/arangodb/build-presets/my-edition --type=arangodb::activities::Activity > find_activity

-P 8 runs eight processes at once, -n 50 gives each one 50 files
