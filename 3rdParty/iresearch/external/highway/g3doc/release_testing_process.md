## Release testing process

We run the following before a release:

### Windows x86

```
run_tests.bat
```

### Linux x86

#### Default

```
./run_tests.sh
```

#### GCC

```
for VER in 8 9 10; do
  rm -rf build_g$VER && mkdir build_g$VER && cd build_g$VER && CC=gcc-$VER CXX=g++-$VER cmake .. -DHWY_WARNINGS_ARE_ERRORS:BOOL=ON && make -j && make test && cd .. && rm -rf build_g$VER
done
```

#### ARMv7 cross compile (GCC)

```
export QEMU_LD_PREFIX=/usr/arm-linux-gnueabihf
rm -rf build_arm7 && mkdir build_arm7 && cd build_arm7
CC=arm-linux-gnueabihf-gcc CXX=arm-linux-gnueabihf-g++ cmake .. -DHWY_CMAKE_ARM7:BOOL=ON -DHWY_WARNINGS_ARE_ERRORS:BOOL=ON
make -j8 && ctest && cd ..
```

#### ARMv8 cross compile (GCC)

```
export QEMU_LD_PREFIX=/usr/aarch64-linux-gnu
rm -rf build_arm8 && mkdir build_arm8 && cd build_arm8
CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++ cmake .. -DHWY_WARNINGS_ARE_ERRORS:BOOL=ON
make -j8 && ctest && cd ..
```

#### JPEG XL clang (debug, asan, msan)

```
for VER in 7 8 9 10 11; do
  rm -rf build_debug$VER && CC=clang-$VER CXX=clang++-$VER BUILD_DIR=build_debug$VER SKIP_TEST=1 ./ci.sh debug && ./ci.sh test -R PassesTest && rm -rf build_debug$VER
  rm -rf build_asan$VER  && CC=clang-$VER CXX=clang++-$VER BUILD_DIR=build_asan$VER  ./ci.sh asan  && rm -rf build_asan$VER
  rm -rf build_msan$VER  && CC=clang-$VER CXX=clang++-$VER BUILD_DIR=build_msan$VER  ./ci.sh msan  && rm -rf build_msan$VER
done
```

#### JPEG XL tests

```
git -C third_party/highway pull -r origin master
git diff
vi deps.sh
git commit -a -m"Highway test"
git push git@gitlab.com:$USER/jpeg-xl.git HEAD:main --force
```
